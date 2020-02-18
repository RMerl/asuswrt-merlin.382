/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file main.c
 * \brief Invocation module.  Initializes subsystems and runs the main loop.
 **/

#include "core/or/or.h"

#include "app/config/config.h"
#include "app/config/statefile.h"
#include "app/main/main.h"
#include "app/main/ntmain.h"
#include "core/mainloop/connection.h"
#include "core/mainloop/cpuworker.h"
#include "core/mainloop/mainloop.h"
#include "core/mainloop/netstatus.h"
#include "core/or/channel.h"
#include "core/or/channelpadding.h"
#include "core/or/channeltls.h"
#include "core/or/circuitlist.h"
#include "core/or/circuitmux_ewma.h"
#include "core/or/command.h"
#include "core/or/connection_edge.h"
#include "core/or/connection_or.h"
#include "core/or/dos.h"
#include "core/or/policies.h"
#include "core/or/protover.h"
#include "core/or/relay.h"
#include "core/or/scheduler.h"
#include "core/or/status.h"
#include "feature/api/tor_api.h"
#include "feature/api/tor_api_internal.h"
#include "feature/client/addressmap.h"
#include "feature/client/bridges.h"
#include "feature/client/entrynodes.h"
#include "feature/client/transports.h"
#include "feature/control/control.h"
#include "feature/dirauth/bwauth.h"
#include "feature/dirauth/keypin.h"
#include "feature/dirauth/process_descs.h"
#include "feature/dircache/consdiffmgr.h"
#include "feature/dircache/dirserv.h"
#include "feature/dirparse/routerparse.h"
#include "feature/hibernate/hibernate.h"
#include "feature/hs/hs_cache.h"
#include "feature/nodelist/authcert.h"
#include "feature/nodelist/microdesc.h"
#include "feature/nodelist/networkstatus.h"
#include "feature/nodelist/nodelist.h"
#include "feature/nodelist/routerlist.h"
#include "feature/relay/dns.h"
#include "feature/relay/ext_orport.h"
#include "feature/relay/onion_queue.h"
#include "feature/relay/routerkeys.h"
#include "feature/relay/routermode.h"
#include "feature/rend/rendcache.h"
#include "feature/rend/rendclient.h"
#include "feature/rend/rendservice.h"
#include "feature/stats/geoip_stats.h"
#include "feature/stats/predict_ports.h"
#include "feature/stats/rephist.h"
#include "lib/compress/compress.h"
#include "lib/container/buffers.h"
#include "lib/crypt_ops/crypto_rand.h"
#include "lib/crypt_ops/crypto_s2k.h"
#include "lib/err/backtrace.h"
#include "lib/geoip/geoip.h"

#include "lib/process/waitpid.h"

#include "lib/meminfo/meminfo.h"
#include "lib/osinfo/uname.h"
#include "lib/sandbox/sandbox.h"
#include "lib/fs/lockfile.h"
#include "lib/net/resolve.h"
#include "lib/tls/tortls.h"
#include "lib/evloop/compat_libevent.h"
#include "lib/encoding/confline.h"
#include "lib/evloop/timers.h"
#include "lib/crypt_ops/crypto_init.h"

#include <event2/event.h>

#include "feature/dirauth/dirvote.h"
#include "feature/dirauth/authmode.h"
#include "feature/dirauth/shared_random.h"

#include "core/or/or_connection_st.h"
#include "core/or/port_cfg_st.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYSTEMD
#   if defined(__COVERITY__) && !defined(__INCLUDE_LEVEL__)
/* Systemd's use of gcc's __INCLUDE_LEVEL__ extension macro appears to confuse
 * Coverity. Here's a kludge to unconfuse it.
 */
#   define __INCLUDE_LEVEL__ 2
#endif /* defined(__COVERITY__) && !defined(__INCLUDE_LEVEL__) */
#include <systemd/sd-daemon.h>
#endif /* defined(HAVE_SYSTEMD) */

void evdns_shutdown(int);

#ifdef HAVE_RUST
// helper function defined in Rust to output a log message indicating if tor is
// running with Rust enabled. See src/rust/tor_util
void rust_log_welcome_string(void);
#endif

/********* PROTOTYPES **********/

static void dumpmemusage(int severity);
static void dumpstats(int severity); /* log stats */
static void process_signal(int sig);

/********* START VARIABLES **********/

/** Decides our behavior when no logs are configured/before any
 * logs have been configured.  For 0, we log notice to stdout as normal.
 * For 1, we log warnings only.  For 2, we log nothing.
 */
int quiet_level = 0;

/********* END VARIABLES ************/

/** Called when we get a SIGHUP: reload configuration files and keys,
 * retry all connections, and so on. */
static int
do_hup(void)
{
  const or_options_t *options = get_options();

  log_notice(LD_GENERAL,"Received reload signal (hup). Reloading config and "
             "resetting internal state.");
  if (accounting_is_enabled(options))
    accounting_record_bandwidth_usage(time(NULL), get_or_state());

  router_reset_warnings();
  routerlist_reset_warnings();
  /* first, reload config variables, in case they've changed */
  if (options->ReloadTorrcOnSIGHUP) {
    /* no need to provide argc/v, they've been cached in init_from_config */
    int init_rv = options_init_from_torrc(0, NULL);
    if (init_rv < 0) {
      log_err(LD_CONFIG,"Reading config failed--see warnings above. "
              "For usage, try -h.");
      return -1;
    } else if (BUG(init_rv > 0)) {
      // LCOV_EXCL_START
      /* This should be impossible: the only "return 1" cases in
       * options_init_from_torrc are ones caused by command-line arguments;
       * but they can't change while Tor is running. */
      return -1;
      // LCOV_EXCL_STOP
    }
    options = get_options(); /* they have changed now */
    /* Logs are only truncated the first time they are opened, but were
       probably intended to be cleaned up on signal. */
    if (options->TruncateLogFile)
      truncate_logs();
  } else {
    char *msg = NULL;
    log_notice(LD_GENERAL, "Not reloading config file: the controller told "
               "us not to.");
    /* Make stuff get rescanned, reloaded, etc. */
    if (set_options((or_options_t*)options, &msg) < 0) {
      if (!msg)
        msg = tor_strdup("Unknown error");
      log_warn(LD_GENERAL, "Unable to re-set previous options: %s", msg);
      tor_free(msg);
    }
  }
  if (authdir_mode(options)) {
    /* reload the approved-routers file */
    if (dirserv_load_fingerprint_file() < 0) {
      /* warnings are logged from dirserv_load_fingerprint_file() directly */
      log_info(LD_GENERAL, "Error reloading fingerprints. "
               "Continuing with old list.");
    }
  }

  /* Rotate away from the old dirty circuits. This has to be done
   * after we've read the new options, but before we start using
   * circuits for directory fetches. */
  circuit_mark_all_dirty_circs_as_unusable();

  /* retry appropriate downloads */
  router_reset_status_download_failures();
  router_reset_descriptor_download_failures();
  if (!net_is_disabled())
    update_networkstatus_downloads(time(NULL));

  /* We'll retry routerstatus downloads in about 10 seconds; no need to
   * force a retry there. */

  if (server_mode(options)) {
    /* Maybe we've been given a new ed25519 key or certificate?
     */
    time_t now = approx_time();
    int new_signing_key = load_ed_keys(options, now);
    if (new_signing_key < 0 ||
        generate_ed_link_cert(options, now, new_signing_key > 0)) {
      log_warn(LD_OR, "Problem reloading Ed25519 keys; still using old keys.");
    }

    /* Update cpuworker and dnsworker processes, so they get up-to-date
     * configuration options. */
    cpuworkers_rotate_keyinfo();
    dns_reset();
  }
  return 0;
}

/** Libevent callback: invoked when we get a signal.
 */
static void
signal_callback(evutil_socket_t fd, short events, void *arg)
{
  const int *sigptr = arg;
  const int sig = *sigptr;
  (void)fd;
  (void)events;

  update_current_time(time(NULL));
  process_signal(sig);
}

/** Do the work of acting on a signal received in <b>sig</b> */
static void
process_signal(int sig)
{
  switch (sig)
    {
    case SIGTERM:
      log_notice(LD_GENERAL,"Catching signal TERM, exiting cleanly.");
      tor_shutdown_event_loop_and_exit(0);
      break;
    case SIGINT:
      if (!server_mode(get_options())) { /* do it now */
        log_notice(LD_GENERAL,"Interrupt: exiting cleanly.");
        tor_shutdown_event_loop_and_exit(0);
        return;
      }
#ifdef HAVE_SYSTEMD
      sd_notify(0, "STOPPING=1");
#endif
      hibernate_begin_shutdown();
      break;
#ifdef SIGPIPE
    case SIGPIPE:
      log_debug(LD_GENERAL,"Caught SIGPIPE. Ignoring.");
      break;
#endif
    case SIGUSR1:
      /* prefer to log it at INFO, but make sure we always see it */
      dumpstats(get_min_log_level()<LOG_INFO ? get_min_log_level() : LOG_INFO);
      control_event_signal(sig);
      break;
    case SIGUSR2:
      switch_logs_debug();
      log_debug(LD_GENERAL,"Caught USR2, going to loglevel debug. "
                "Send HUP to change back.");
      control_event_signal(sig);
      break;
    case SIGHUP:
#ifdef HAVE_SYSTEMD
      sd_notify(0, "RELOADING=1");
#endif
      if (do_hup() < 0) {
        log_warn(LD_CONFIG,"Restart failed (config error?). Exiting.");
        tor_shutdown_event_loop_and_exit(1);
        return;
      }
#ifdef HAVE_SYSTEMD
      sd_notify(0, "READY=1");
#endif
      control_event_signal(sig);
      break;
#ifdef SIGCHLD
    case SIGCHLD:
      notify_pending_waitpid_callbacks();
      break;
#endif
    case SIGNEWNYM: {
      do_signewnym(time(NULL));
      break;
    }
    case SIGCLEARDNSCACHE:
      addressmap_clear_transient();
      control_event_signal(sig);
      break;
    case SIGHEARTBEAT:
      log_heartbeat(time(NULL));
      control_event_signal(sig);
      break;
  }
}

/**
 * Write current memory usage information to the log.
 */
static void
dumpmemusage(int severity)
{
  connection_dump_buffer_mem_stats(severity);
  tor_log(severity, LD_GENERAL, "In rephist: %"PRIu64" used by %d Tors.",
      (rephist_total_alloc), rephist_total_num);
  dump_routerlist_mem_usage(severity);
  dump_cell_pool_usage(severity);
  dump_dns_mem_usage(severity);
  tor_log_mallinfo(severity);
}

/** Write all statistics to the log, with log level <b>severity</b>. Called
 * in response to a SIGUSR1. */
static void
dumpstats(int severity)
{
  time_t now = time(NULL);
  time_t elapsed;
  size_t rbuf_cap, wbuf_cap, rbuf_len, wbuf_len;

  tor_log(severity, LD_GENERAL, "Dumping stats:");

  SMARTLIST_FOREACH_BEGIN(get_connection_array(), connection_t *, conn) {
    int i = conn_sl_idx;
    tor_log(severity, LD_GENERAL,
        "Conn %d (socket %d) type %d (%s), state %d (%s), created %d secs ago",
        i, (int)conn->s, conn->type, conn_type_to_string(conn->type),
        conn->state, conn_state_to_string(conn->type, conn->state),
        (int)(now - conn->timestamp_created));
    if (!connection_is_listener(conn)) {
      tor_log(severity,LD_GENERAL,
          "Conn %d is to %s:%d.", i,
          safe_str_client(conn->address),
          conn->port);
      tor_log(severity,LD_GENERAL,
          "Conn %d: %d bytes waiting on inbuf (len %d, last read %d secs ago)",
          i,
          (int)connection_get_inbuf_len(conn),
          (int)buf_allocation(conn->inbuf),
          (int)(now - conn->timestamp_last_read_allowed));
      tor_log(severity,LD_GENERAL,
          "Conn %d: %d bytes waiting on outbuf "
          "(len %d, last written %d secs ago)",i,
          (int)connection_get_outbuf_len(conn),
          (int)buf_allocation(conn->outbuf),
          (int)(now - conn->timestamp_last_write_allowed));
      if (conn->type == CONN_TYPE_OR) {
        or_connection_t *or_conn = TO_OR_CONN(conn);
        if (or_conn->tls) {
          if (tor_tls_get_buffer_sizes(or_conn->tls, &rbuf_cap, &rbuf_len,
                                       &wbuf_cap, &wbuf_len) == 0) {
            tor_log(severity, LD_GENERAL,
                "Conn %d: %d/%d bytes used on OpenSSL read buffer; "
                "%d/%d bytes used on write buffer.",
                i, (int)rbuf_len, (int)rbuf_cap, (int)wbuf_len, (int)wbuf_cap);
          }
        }
      }
    }
    circuit_dump_by_conn(conn, severity); /* dump info about all the circuits
                                           * using this conn */
  } SMARTLIST_FOREACH_END(conn);

  channel_dumpstats(severity);
  channel_listener_dumpstats(severity);

  tor_log(severity, LD_NET,
      "Cells processed: %"PRIu64" padding\n"
      "                 %"PRIu64" create\n"
      "                 %"PRIu64" created\n"
      "                 %"PRIu64" relay\n"
      "                        (%"PRIu64" relayed)\n"
      "                        (%"PRIu64" delivered)\n"
      "                 %"PRIu64" destroy",
      (stats_n_padding_cells_processed),
      (stats_n_create_cells_processed),
      (stats_n_created_cells_processed),
      (stats_n_relay_cells_processed),
      (stats_n_relay_cells_relayed),
      (stats_n_relay_cells_delivered),
      (stats_n_destroy_cells_processed));
  if (stats_n_data_cells_packaged)
    tor_log(severity,LD_NET,"Average packaged cell fullness: %2.3f%%",
        100*(((double)stats_n_data_bytes_packaged) /
             ((double)stats_n_data_cells_packaged*RELAY_PAYLOAD_SIZE)) );
  if (stats_n_data_cells_received)
    tor_log(severity,LD_NET,"Average delivered cell fullness: %2.3f%%",
        100*(((double)stats_n_data_bytes_received) /
             ((double)stats_n_data_cells_received*RELAY_PAYLOAD_SIZE)) );

  cpuworker_log_onionskin_overhead(severity, ONION_HANDSHAKE_TYPE_TAP, "TAP");
  cpuworker_log_onionskin_overhead(severity, ONION_HANDSHAKE_TYPE_NTOR,"ntor");

  if (now - time_of_process_start >= 0)
    elapsed = now - time_of_process_start;
  else
    elapsed = 0;

  if (elapsed) {
    tor_log(severity, LD_NET,
        "Average bandwidth: %"PRIu64"/%d = %d bytes/sec reading",
        (get_bytes_read()),
        (int)elapsed,
        (int) (get_bytes_read()/elapsed));
    tor_log(severity, LD_NET,
        "Average bandwidth: %"PRIu64"/%d = %d bytes/sec writing",
        (get_bytes_written()),
        (int)elapsed,
        (int) (get_bytes_written()/elapsed));
  }

  tor_log(severity, LD_NET, "--------------- Dumping memory information:");
  dumpmemusage(severity);

  rep_hist_dump_stats(now,severity);
  rend_service_dump_stats(severity);
}

/** Called by exit() as we shut down the process.
 */
static void
exit_function(void)
{
  /* NOTE: If we ever daemonize, this gets called immediately.  That's
   * okay for now, because we only use this on Windows.  */
#ifdef _WIN32
  WSACleanup();
#endif
}

#ifdef _WIN32
#define UNIX_ONLY 0
#else
#define UNIX_ONLY 1
#endif

static struct {
  /** A numeric code for this signal. Must match the signal value if
   * try_to_register is true. */
  int signal_value;
  /** True if we should try to register this signal with libevent and catch
   * corresponding posix signals. False otherwise. */
  int try_to_register;
  /** Pointer to hold the event object constructed for this signal. */
  struct event *signal_event;
} signal_handlers[] = {
#ifdef SIGINT
  { SIGINT, UNIX_ONLY, NULL }, /* do a controlled slow shutdown */
#endif
#ifdef SIGTERM
  { SIGTERM, UNIX_ONLY, NULL }, /* to terminate now */
#endif
#ifdef SIGPIPE
  { SIGPIPE, UNIX_ONLY, NULL }, /* otherwise SIGPIPE kills us */
#endif
#ifdef SIGUSR1
  { SIGUSR1, UNIX_ONLY, NULL }, /* dump stats */
#endif
#ifdef SIGUSR2
  { SIGUSR2, UNIX_ONLY, NULL }, /* go to loglevel debug */
#endif
#ifdef SIGHUP
  { SIGHUP, UNIX_ONLY, NULL }, /* to reload config, retry conns, etc */
#endif
#ifdef SIGXFSZ
  { SIGXFSZ, UNIX_ONLY, NULL }, /* handle file-too-big resource exhaustion */
#endif
#ifdef SIGCHLD
  { SIGCHLD, UNIX_ONLY, NULL }, /* handle dns/cpu workers that exit */
#endif
  /* These are controller-only */
  { SIGNEWNYM, 0, NULL },
  { SIGCLEARDNSCACHE, 0, NULL },
  { SIGHEARTBEAT, 0, NULL },
  { -1, -1, NULL }
};

/** Set up the signal handler events for this process, and register them
 * with libevent if appropriate. */
void
handle_signals(void)
{
  int i;
  const int enabled = !get_options()->DisableSignalHandlers;

  for (i = 0; signal_handlers[i].signal_value >= 0; ++i) {
    /* Signal handlers are only registered with libevent if they need to catch
     * real POSIX signals.  We construct these signal handler events in either
     * case, though, so that controllers can activate them with the SIGNAL
     * command.
     */
    if (enabled && signal_handlers[i].try_to_register) {
      signal_handlers[i].signal_event =
        tor_evsignal_new(tor_libevent_get_base(),
                         signal_handlers[i].signal_value,
                         signal_callback,
                         &signal_handlers[i].signal_value);
      if (event_add(signal_handlers[i].signal_event, NULL))
        log_warn(LD_BUG, "Error from libevent when adding "
                 "event for signal %d",
                 signal_handlers[i].signal_value);
    } else {
      signal_handlers[i].signal_event =
        tor_event_new(tor_libevent_get_base(), -1,
                      EV_SIGNAL, signal_callback,
                      &signal_handlers[i].signal_value);
    }
  }
}

/* Cause the signal handler for signal_num to be called in the event loop. */
void
activate_signal(int signal_num)
{
  int i;
  for (i = 0; signal_handlers[i].signal_value >= 0; ++i) {
    if (signal_handlers[i].signal_value == signal_num) {
      event_active(signal_handlers[i].signal_event, EV_SIGNAL, 1);
      return;
    }
  }
}

/** Main entry point for the Tor command-line client.  Return 0 on "success",
 * negative on "failure", and positive on "success and exit".
 */
int
tor_init(int argc, char *argv[])
{
  char progname[256];
  int quiet = 0;

  time_of_process_start = time(NULL);
  tor_init_connection_lists();
  /* Have the log set up with our application name. */
  tor_snprintf(progname, sizeof(progname), "Tor %s", get_version());
  log_set_application_name(progname);

  /* Set up the crypto nice and early */
  if (crypto_early_init() < 0) {
    log_err(LD_GENERAL, "Unable to initialize the crypto subsystem!");
    return -1;
  }

  /* Initialize the history structures. */
  rep_hist_init();
  /* Initialize the service cache. */
  rend_cache_init();
  addressmap_init(); /* Init the client dns cache. Do it always, since it's
                      * cheap. */
  /* Initialize the HS subsystem. */
  hs_init();

  {
  /* We search for the "quiet" option first, since it decides whether we
   * will log anything at all to the command line. */
    config_line_t *opts = NULL, *cmdline_opts = NULL;
    const config_line_t *cl;
    (void) config_parse_commandline(argc, argv, 1, &opts, &cmdline_opts);
    for (cl = cmdline_opts; cl; cl = cl->next) {
      if (!strcmp(cl->key, "--hush"))
        quiet = 1;
      if (!strcmp(cl->key, "--quiet") ||
          !strcmp(cl->key, "--dump-config"))
        quiet = 2;
      /* The following options imply --hush */
      if (!strcmp(cl->key, "--version") || !strcmp(cl->key, "--digests") ||
          !strcmp(cl->key, "--list-torrc-options") ||
          !strcmp(cl->key, "--library-versions") ||
          !strcmp(cl->key, "--hash-password") ||
          !strcmp(cl->key, "-h") || !strcmp(cl->key, "--help")) {
        if (quiet < 1)
          quiet = 1;
      }
    }
    config_free_lines(opts);
    config_free_lines(cmdline_opts);
  }

 /* give it somewhere to log to initially */
  switch (quiet) {
    case 2:
      /* no initial logging */
      break;
    case 1:
      add_temp_log(LOG_WARN);
      break;
    default:
      add_temp_log(LOG_NOTICE);
  }
  quiet_level = quiet;

  {
    const char *version = get_version();

    log_notice(LD_GENERAL, "Tor %s running on %s with Libevent %s, "
               "%s %s, Zlib %s, Liblzma %s, and Libzstd %s.", version,
               get_uname(),
               tor_libevent_get_version_str(),
               crypto_get_library_name(),
               crypto_get_library_version_string(),
               tor_compress_supports_method(ZLIB_METHOD) ?
                 tor_compress_version_str(ZLIB_METHOD) : "N/A",
               tor_compress_supports_method(LZMA_METHOD) ?
                 tor_compress_version_str(LZMA_METHOD) : "N/A",
               tor_compress_supports_method(ZSTD_METHOD) ?
                 tor_compress_version_str(ZSTD_METHOD) : "N/A");

    log_notice(LD_GENERAL, "Tor can't help you if you use it wrong! "
               "Learn how to be safe at "
               "https://www.torproject.org/download/download#warning");

    if (strstr(version, "alpha") || strstr(version, "beta"))
      log_notice(LD_GENERAL, "This version is not a stable Tor release. "
                 "Expect more bugs than usual.");

    tor_compress_log_init_warnings();
  }

#ifdef HAVE_RUST
  rust_log_welcome_string();
#endif /* defined(HAVE_RUST) */

  if (network_init()<0) {
    log_err(LD_BUG,"Error initializing network; exiting.");
    return -1;
  }
  atexit(exit_function);

  int init_rv = options_init_from_torrc(argc,argv);
  if (init_rv < 0) {
    log_err(LD_CONFIG,"Reading config failed--see warnings above.");
    return -1;
  } else if (init_rv > 0) {
    // We succeeded, and should exit anyway -- probably the user just said
    // "--version" or something like that.
    return 1;
  }

  /* The options are now initialised */
  const or_options_t *options = get_options();

  /* Initialize channelpadding parameters to defaults until we get
   * a consensus */
  channelpadding_new_consensus_params(NULL);

  /* Initialize predicted ports list after loading options */
  predicted_ports_init();

#ifndef _WIN32
  if (geteuid()==0)
    log_warn(LD_GENERAL,"You are running Tor as root. You don't need to, "
             "and you probably shouldn't.");
#endif

  if (crypto_global_init(options->HardwareAccel,
                         options->AccelName,
                         options->AccelDir)) {
    log_err(LD_BUG, "Unable to initialize OpenSSL. Exiting.");
    return -1;
  }
  stream_choice_seed_weak_rng();
  if (tor_init_libevent_rng() < 0) {
    log_warn(LD_NET, "Problem initializing libevent RNG.");
  }

  /* Scan/clean unparseable descriptors; after reading config */
  routerparse_init();

  return 0;
}

/** A lockfile structure, used to prevent two Tors from messing with the
 * data directory at once.  If this variable is non-NULL, we're holding
 * the lockfile. */
static tor_lockfile_t *lockfile = NULL;

/** Try to grab the lock file described in <b>options</b>, if we do not
 * already have it.  If <b>err_if_locked</b> is true, warn if somebody else is
 * holding the lock, and exit if we can't get it after waiting.  Otherwise,
 * return -1 if we can't get the lockfile.  Return 0 on success.
 */
int
try_locking(const or_options_t *options, int err_if_locked)
{
  if (lockfile)
    return 0;
  else {
    char *fname = options_get_datadir_fname(options, "lock");
    int already_locked = 0;
    tor_lockfile_t *lf = tor_lockfile_lock(fname, 0, &already_locked);
    tor_free(fname);
    if (!lf) {
      if (err_if_locked && already_locked) {
        int r;
        log_warn(LD_GENERAL, "It looks like another Tor process is running "
                 "with the same data directory.  Waiting 5 seconds to see "
                 "if it goes away.");
#ifndef _WIN32
        sleep(5);
#else
        Sleep(5000);
#endif
        r = try_locking(options, 0);
        if (r<0) {
          log_err(LD_GENERAL, "No, it's still there.  Exiting.");
          return -1;
        }
        return r;
      }
      return -1;
    }
    lockfile = lf;
    return 0;
  }
}

/** Return true iff we've successfully acquired the lock file. */
int
have_lockfile(void)
{
  return lockfile != NULL;
}

/** If we have successfully acquired the lock file, release it. */
void
release_lockfile(void)
{
  if (lockfile) {
    tor_lockfile_unlock(lockfile);
    lockfile = NULL;
  }
}

/** Free all memory that we might have allocated somewhere.
 * If <b>postfork</b>, we are a worker process and we want to free
 * only the parts of memory that we won't touch. If !<b>postfork</b>,
 * Tor is shutting down and we should free everything.
 *
 * Helps us find the real leaks with sanitizers and the like. Also valgrind
 * should then report 0 reachable in its leak report (in an ideal world --
 * in practice libevent, SSL, libc etc never quite free everything). */
void
tor_free_all(int postfork)
{
  if (!postfork) {
    evdns_shutdown(1);
  }
  geoip_free_all();
  geoip_stats_free_all();
  dirvote_free_all();
  routerlist_free_all();
  networkstatus_free_all();
  addressmap_free_all();
  dirserv_free_fingerprint_list();
  dirserv_free_all();
  dirserv_clear_measured_bw_cache();
  rend_cache_free_all();
  rend_service_authorization_free_all();
  rep_hist_free_all();
  dns_free_all();
  clear_pending_onions();
  circuit_free_all();
  entry_guards_free_all();
  pt_free_all();
  channel_tls_free_all();
  channel_free_all();
  connection_free_all();
  connection_edge_free_all();
  scheduler_free_all();
  nodelist_free_all();
  microdesc_free_all();
  routerparse_free_all();
  ext_orport_free_all();
  control_free_all();
  tor_free_getaddrinfo_cache();
  protover_free_all();
  bridges_free_all();
  consdiffmgr_free_all();
  hs_free_all();
  dos_free_all();
  circuitmux_ewma_free_all();
  accounting_free_all();

  if (!postfork) {
    config_free_all();
    or_state_free_all();
    router_free_all();
    routerkeys_free_all();
    policies_free_all();
  }
  if (!postfork) {
    tor_tls_free_all();
#ifndef _WIN32
    tor_getpwnam(NULL);
#endif
  }
  /* stuff in main.c */

  tor_mainloop_free_all();

  if (!postfork) {
    release_lockfile();
  }
  tor_libevent_free_all();
  /* Stuff in util.c and address.c*/
  if (!postfork) {
    escaped(NULL);
    esc_router_info(NULL);
    clean_up_backtrace_handler();
    logs_free_all(); /* free log strings. do this last so logs keep working. */
  }
}

/**
 * Remove the specified file, and log a warning if the operation fails for
 * any reason other than the file not existing. Ignores NULL filenames.
 */
void
tor_remove_file(const char *filename)
{
  if (filename && tor_unlink(filename) != 0 && errno != ENOENT) {
    log_warn(LD_FS, "Couldn't unlink %s: %s",
               filename, strerror(errno));
  }
}

/** Do whatever cleanup is necessary before shutting Tor down. */
void
tor_cleanup(void)
{
  const or_options_t *options = get_options();
  if (options->command == CMD_RUN_TOR) {
    time_t now = time(NULL);
    /* Remove our pid file. We don't care if there was an error when we
     * unlink, nothing we could do about it anyways. */
    tor_remove_file(options->PidFile);
    /* Remove control port file */
    tor_remove_file(options->ControlPortWriteToFile);
    /* Remove cookie authentication file */
    {
      char *cookie_fname = get_controller_cookie_file_name();
      tor_remove_file(cookie_fname);
      tor_free(cookie_fname);
    }
    /* Remove Extended ORPort cookie authentication file */
    {
      char *cookie_fname = get_ext_or_auth_cookie_file_name();
      tor_remove_file(cookie_fname);
      tor_free(cookie_fname);
    }
    if (accounting_is_enabled(options))
      accounting_record_bandwidth_usage(now, get_or_state());
    or_state_mark_dirty(get_or_state(), 0); /* force an immediate save. */
    or_state_save(now);
    if (authdir_mode(options)) {
      sr_save_and_cleanup();
    }
    if (authdir_mode_tests_reachability(options))
      rep_hist_record_mtbf_data(now, 0);
    keypin_close_journal();
  }

  timers_shutdown();

  tor_free_all(0); /* We could move tor_free_all back into the ifdef below
                      later, if it makes shutdown unacceptably slow.  But for
                      now, leave it here: it's helped us catch bugs in the
                      past. */
  crypto_global_cleanup();
}

/** Read/create keys as needed, and echo our fingerprint to stdout. */
static int
do_list_fingerprint(void)
{
  char buf[FINGERPRINT_LEN+1];
  crypto_pk_t *k;
  const char *nickname = get_options()->Nickname;
  sandbox_disable_getaddrinfo_cache();
  if (!server_mode(get_options())) {
    log_err(LD_GENERAL,
            "Clients don't have long-term identity keys. Exiting.");
    return -1;
  }
  tor_assert(nickname);
  if (init_keys() < 0) {
    log_err(LD_GENERAL,"Error initializing keys; exiting.");
    return -1;
  }
  if (!(k = get_server_identity_key())) {
    log_err(LD_GENERAL,"Error: missing identity key.");
    return -1;
  }
  if (crypto_pk_get_fingerprint(k, buf, 1)<0) {
    log_err(LD_BUG, "Error computing fingerprint");
    return -1;
  }
  printf("%s %s\n", nickname, buf);
  return 0;
}

/** Entry point for password hashing: take the desired password from
 * the command line, and print its salted hash to stdout. **/
static void
do_hash_password(void)
{

  char output[256];
  char key[S2K_RFC2440_SPECIFIER_LEN+DIGEST_LEN];

  crypto_rand(key, S2K_RFC2440_SPECIFIER_LEN-1);
  key[S2K_RFC2440_SPECIFIER_LEN-1] = (uint8_t)96; /* Hash 64 K of data. */
  secret_to_key_rfc2440(key+S2K_RFC2440_SPECIFIER_LEN, DIGEST_LEN,
                get_options()->command_arg, strlen(get_options()->command_arg),
                key);
  base16_encode(output, sizeof(output), key, sizeof(key));
  printf("16:%s\n",output);
}

/** Entry point for configuration dumping: write the configuration to
 * stdout. */
static int
do_dump_config(void)
{
  const or_options_t *options = get_options();
  const char *arg = options->command_arg;
  int how;
  char *opts;

  if (!strcmp(arg, "short")) {
    how = OPTIONS_DUMP_MINIMAL;
  } else if (!strcmp(arg, "non-builtin")) {
    how = OPTIONS_DUMP_DEFAULTS;
  } else if (!strcmp(arg, "full")) {
    how = OPTIONS_DUMP_ALL;
  } else {
    fprintf(stderr, "No valid argument to --dump-config found!\n");
    fprintf(stderr, "Please select 'short', 'non-builtin', or 'full'.\n");

    return -1;
  }

  opts = options_dump(options, how);
  printf("%s", opts);
  tor_free(opts);

  return 0;
}

static void
init_addrinfo(void)
{
  if (! server_mode(get_options()) ||
      (get_options()->Address && strlen(get_options()->Address) > 0)) {
    /* We don't need to seed our own hostname, because we won't be calling
     * resolve_my_address on it.
     */
    return;
  }
  char hname[256];

  // host name to sandbox
  gethostname(hname, sizeof(hname));
  tor_add_addrinfo(hname);
}

static sandbox_cfg_t*
sandbox_init_filter(void)
{
  const or_options_t *options = get_options();
  sandbox_cfg_t *cfg = sandbox_cfg_new();
  int i;

  sandbox_cfg_allow_openat_filename(&cfg,
      get_cachedir_fname("cached-status"));

#define OPEN(name)                              \
  sandbox_cfg_allow_open_filename(&cfg, tor_strdup(name))

#define OPEN_DATADIR(name)                      \
  sandbox_cfg_allow_open_filename(&cfg, get_datadir_fname(name))

#define OPEN_DATADIR2(name, name2)                       \
  sandbox_cfg_allow_open_filename(&cfg, get_datadir_fname2((name), (name2)))

#define OPEN_DATADIR_SUFFIX(name, suffix) do {  \
    OPEN_DATADIR(name);                         \
    OPEN_DATADIR(name suffix);                  \
  } while (0)

#define OPEN_DATADIR2_SUFFIX(name, name2, suffix) do {  \
    OPEN_DATADIR2(name, name2);                         \
    OPEN_DATADIR2(name, name2 suffix);                  \
  } while (0)

#define OPEN_KEY_DIRECTORY() \
  sandbox_cfg_allow_open_filename(&cfg, tor_strdup(options->KeyDirectory))
#define OPEN_CACHEDIR(name)                      \
  sandbox_cfg_allow_open_filename(&cfg, get_cachedir_fname(name))
#define OPEN_CACHEDIR_SUFFIX(name, suffix) do {  \
    OPEN_CACHEDIR(name);                         \
    OPEN_CACHEDIR(name suffix);                  \
  } while (0)
#define OPEN_KEYDIR(name)                      \
  sandbox_cfg_allow_open_filename(&cfg, get_keydir_fname(name))
#define OPEN_KEYDIR_SUFFIX(name, suffix) do {    \
    OPEN_KEYDIR(name);                           \
    OPEN_KEYDIR(name suffix);                    \
  } while (0)

  OPEN(options->DataDirectory);
  OPEN_KEY_DIRECTORY();

  OPEN_CACHEDIR_SUFFIX("cached-certs", ".tmp");
  OPEN_CACHEDIR_SUFFIX("cached-consensus", ".tmp");
  OPEN_CACHEDIR_SUFFIX("unverified-consensus", ".tmp");
  OPEN_CACHEDIR_SUFFIX("unverified-microdesc-consensus", ".tmp");
  OPEN_CACHEDIR_SUFFIX("cached-microdesc-consensus", ".tmp");
  OPEN_CACHEDIR_SUFFIX("cached-microdescs", ".tmp");
  OPEN_CACHEDIR_SUFFIX("cached-microdescs.new", ".tmp");
  OPEN_CACHEDIR_SUFFIX("cached-descriptors", ".tmp");
  OPEN_CACHEDIR_SUFFIX("cached-descriptors.new", ".tmp");
  OPEN_CACHEDIR("cached-descriptors.tmp.tmp");
  OPEN_CACHEDIR_SUFFIX("cached-extrainfo", ".tmp");
  OPEN_CACHEDIR_SUFFIX("cached-extrainfo.new", ".tmp");
  OPEN_CACHEDIR("cached-extrainfo.tmp.tmp");

  OPEN_DATADIR_SUFFIX("state", ".tmp");
  OPEN_DATADIR_SUFFIX("sr-state", ".tmp");
  OPEN_DATADIR_SUFFIX("unparseable-desc", ".tmp");
  OPEN_DATADIR_SUFFIX("v3-status-votes", ".tmp");
  OPEN_DATADIR("key-pinning-journal");
  OPEN("/dev/srandom");
  OPEN("/dev/urandom");
  OPEN("/dev/random");
  OPEN("/etc/hosts");
  OPEN("/proc/meminfo");

  if (options->BridgeAuthoritativeDir)
    OPEN_DATADIR_SUFFIX("networkstatus-bridges", ".tmp");

  if (authdir_mode(options))
    OPEN_DATADIR("approved-routers");

  if (options->ServerDNSResolvConfFile)
    sandbox_cfg_allow_open_filename(&cfg,
                                tor_strdup(options->ServerDNSResolvConfFile));
  else
    sandbox_cfg_allow_open_filename(&cfg, tor_strdup("/etc/resolv.conf"));

  for (i = 0; i < 2; ++i) {
    if (get_torrc_fname(i)) {
      sandbox_cfg_allow_open_filename(&cfg, tor_strdup(get_torrc_fname(i)));
    }
  }

  SMARTLIST_FOREACH(options->FilesOpenedByIncludes, char *, f, {
    OPEN(f);
  });

#define RENAME_SUFFIX(name, suffix)        \
  sandbox_cfg_allow_rename(&cfg,           \
      get_datadir_fname(name suffix),      \
      get_datadir_fname(name))

#define RENAME_SUFFIX2(prefix, name, suffix) \
  sandbox_cfg_allow_rename(&cfg,                                        \
                           get_datadir_fname2(prefix, name suffix),     \
                           get_datadir_fname2(prefix, name))

#define RENAME_CACHEDIR_SUFFIX(name, suffix)        \
  sandbox_cfg_allow_rename(&cfg,           \
      get_cachedir_fname(name suffix),      \
      get_cachedir_fname(name))

#define RENAME_KEYDIR_SUFFIX(name, suffix)    \
  sandbox_cfg_allow_rename(&cfg,           \
      get_keydir_fname(name suffix),      \
      get_keydir_fname(name))

  RENAME_CACHEDIR_SUFFIX("cached-certs", ".tmp");
  RENAME_CACHEDIR_SUFFIX("cached-consensus", ".tmp");
  RENAME_CACHEDIR_SUFFIX("unverified-consensus", ".tmp");
  RENAME_CACHEDIR_SUFFIX("unverified-microdesc-consensus", ".tmp");
  RENAME_CACHEDIR_SUFFIX("cached-microdesc-consensus", ".tmp");
  RENAME_CACHEDIR_SUFFIX("cached-microdescs", ".tmp");
  RENAME_CACHEDIR_SUFFIX("cached-microdescs", ".new");
  RENAME_CACHEDIR_SUFFIX("cached-microdescs.new", ".tmp");
  RENAME_CACHEDIR_SUFFIX("cached-descriptors", ".tmp");
  RENAME_CACHEDIR_SUFFIX("cached-descriptors", ".new");
  RENAME_CACHEDIR_SUFFIX("cached-descriptors.new", ".tmp");
  RENAME_CACHEDIR_SUFFIX("cached-extrainfo", ".tmp");
  RENAME_CACHEDIR_SUFFIX("cached-extrainfo", ".new");
  RENAME_CACHEDIR_SUFFIX("cached-extrainfo.new", ".tmp");

  RENAME_SUFFIX("state", ".tmp");
  RENAME_SUFFIX("sr-state", ".tmp");
  RENAME_SUFFIX("unparseable-desc", ".tmp");
  RENAME_SUFFIX("v3-status-votes", ".tmp");

  if (options->BridgeAuthoritativeDir)
    RENAME_SUFFIX("networkstatus-bridges", ".tmp");

#define STAT_DATADIR(name)                      \
  sandbox_cfg_allow_stat_filename(&cfg, get_datadir_fname(name))

#define STAT_CACHEDIR(name)                                             \
  sandbox_cfg_allow_stat_filename(&cfg, get_cachedir_fname(name))

#define STAT_DATADIR2(name, name2)                                      \
  sandbox_cfg_allow_stat_filename(&cfg, get_datadir_fname2((name), (name2)))

#define STAT_KEY_DIRECTORY() \
  sandbox_cfg_allow_stat_filename(&cfg, tor_strdup(options->KeyDirectory))

  STAT_DATADIR(NULL);
  STAT_DATADIR("lock");
  STAT_DATADIR("state");
  STAT_DATADIR("router-stability");

  STAT_CACHEDIR("cached-extrainfo.new");

  {
    smartlist_t *files = smartlist_new();
    tor_log_get_logfile_names(files);
    SMARTLIST_FOREACH(files, char *, file_name, {
      /* steals reference */
      sandbox_cfg_allow_open_filename(&cfg, file_name);
    });
    smartlist_free(files);
  }

  {
    smartlist_t *files = smartlist_new();
    smartlist_t *dirs = smartlist_new();
    hs_service_lists_fnames_for_sandbox(files, dirs);
    SMARTLIST_FOREACH(files, char *, file_name, {
      char *tmp_name = NULL;
      tor_asprintf(&tmp_name, "%s.tmp", file_name);
      sandbox_cfg_allow_rename(&cfg,
                               tor_strdup(tmp_name), tor_strdup(file_name));
      /* steals references */
      sandbox_cfg_allow_open_filename(&cfg, file_name);
      sandbox_cfg_allow_open_filename(&cfg, tmp_name);
    });
    SMARTLIST_FOREACH(dirs, char *, dir, {
      /* steals reference */
      sandbox_cfg_allow_stat_filename(&cfg, dir);
    });
    smartlist_free(files);
    smartlist_free(dirs);
  }

  {
    char *fname;
    if ((fname = get_controller_cookie_file_name())) {
      sandbox_cfg_allow_open_filename(&cfg, fname);
    }
    if ((fname = get_ext_or_auth_cookie_file_name())) {
      sandbox_cfg_allow_open_filename(&cfg, fname);
    }
  }

  SMARTLIST_FOREACH_BEGIN(get_configured_ports(), port_cfg_t *, port) {
    if (!port->is_unix_addr)
      continue;
    /* When we open an AF_UNIX address, we want permission to open the
     * directory that holds it. */
    char *dirname = tor_strdup(port->unix_addr);
    if (get_parent_directory(dirname) == 0) {
      OPEN(dirname);
    }
    tor_free(dirname);
    sandbox_cfg_allow_chmod_filename(&cfg, tor_strdup(port->unix_addr));
    sandbox_cfg_allow_chown_filename(&cfg, tor_strdup(port->unix_addr));
  } SMARTLIST_FOREACH_END(port);

  if (options->DirPortFrontPage) {
    sandbox_cfg_allow_open_filename(&cfg,
                                    tor_strdup(options->DirPortFrontPage));
  }

  // orport
  if (server_mode(get_options())) {

    OPEN_KEYDIR_SUFFIX("secret_id_key", ".tmp");
    OPEN_KEYDIR_SUFFIX("secret_onion_key", ".tmp");
    OPEN_KEYDIR_SUFFIX("secret_onion_key_ntor", ".tmp");
    OPEN_KEYDIR("secret_id_key.old");
    OPEN_KEYDIR("secret_onion_key.old");
    OPEN_KEYDIR("secret_onion_key_ntor.old");

    OPEN_KEYDIR_SUFFIX("ed25519_master_id_secret_key", ".tmp");
    OPEN_KEYDIR_SUFFIX("ed25519_master_id_secret_key_encrypted", ".tmp");
    OPEN_KEYDIR_SUFFIX("ed25519_master_id_public_key", ".tmp");
    OPEN_KEYDIR_SUFFIX("ed25519_signing_secret_key", ".tmp");
    OPEN_KEYDIR_SUFFIX("ed25519_signing_secret_key_encrypted", ".tmp");
    OPEN_KEYDIR_SUFFIX("ed25519_signing_public_key", ".tmp");
    OPEN_KEYDIR_SUFFIX("ed25519_signing_cert", ".tmp");

    OPEN_DATADIR2_SUFFIX("stats", "bridge-stats", ".tmp");
    OPEN_DATADIR2_SUFFIX("stats", "dirreq-stats", ".tmp");

    OPEN_DATADIR2_SUFFIX("stats", "entry-stats", ".tmp");
    OPEN_DATADIR2_SUFFIX("stats", "exit-stats", ".tmp");
    OPEN_DATADIR2_SUFFIX("stats", "buffer-stats", ".tmp");
    OPEN_DATADIR2_SUFFIX("stats", "conn-stats", ".tmp");
    OPEN_DATADIR2_SUFFIX("stats", "hidserv-stats", ".tmp");

    OPEN_DATADIR("approved-routers");
    OPEN_DATADIR_SUFFIX("fingerprint", ".tmp");
    OPEN_DATADIR_SUFFIX("hashed-fingerprint", ".tmp");
    OPEN_DATADIR_SUFFIX("router-stability", ".tmp");

    OPEN("/etc/resolv.conf");

    RENAME_SUFFIX("fingerprint", ".tmp");
    RENAME_KEYDIR_SUFFIX("secret_onion_key_ntor", ".tmp");

    RENAME_KEYDIR_SUFFIX("secret_id_key", ".tmp");
    RENAME_KEYDIR_SUFFIX("secret_id_key.old", ".tmp");
    RENAME_KEYDIR_SUFFIX("secret_onion_key", ".tmp");
    RENAME_KEYDIR_SUFFIX("secret_onion_key.old", ".tmp");

    RENAME_SUFFIX2("stats", "bridge-stats", ".tmp");
    RENAME_SUFFIX2("stats", "dirreq-stats", ".tmp");
    RENAME_SUFFIX2("stats", "entry-stats", ".tmp");
    RENAME_SUFFIX2("stats", "exit-stats", ".tmp");
    RENAME_SUFFIX2("stats", "buffer-stats", ".tmp");
    RENAME_SUFFIX2("stats", "conn-stats", ".tmp");
    RENAME_SUFFIX2("stats", "hidserv-stats", ".tmp");
    RENAME_SUFFIX("hashed-fingerprint", ".tmp");
    RENAME_SUFFIX("router-stability", ".tmp");

    RENAME_KEYDIR_SUFFIX("ed25519_master_id_secret_key", ".tmp");
    RENAME_KEYDIR_SUFFIX("ed25519_master_id_secret_key_encrypted", ".tmp");
    RENAME_KEYDIR_SUFFIX("ed25519_master_id_public_key", ".tmp");
    RENAME_KEYDIR_SUFFIX("ed25519_signing_secret_key", ".tmp");
    RENAME_KEYDIR_SUFFIX("ed25519_signing_cert", ".tmp");

    sandbox_cfg_allow_rename(&cfg,
             get_keydir_fname("secret_onion_key"),
             get_keydir_fname("secret_onion_key.old"));
    sandbox_cfg_allow_rename(&cfg,
             get_keydir_fname("secret_onion_key_ntor"),
             get_keydir_fname("secret_onion_key_ntor.old"));

    STAT_KEY_DIRECTORY();
    OPEN_DATADIR("stats");
    STAT_DATADIR("stats");
    STAT_DATADIR2("stats", "dirreq-stats");

    consdiffmgr_register_with_sandbox(&cfg);
  }

  init_addrinfo();

  return cfg;
}

int
run_tor_main_loop(void)
{
  handle_signals();
  monotime_init();
  timers_initialize();
  initialize_mainloop_events();

  /* load the private keys, if we're supposed to have them, and set up the
   * TLS context. */
  if (! client_identity_key_is_set()) {
    if (init_keys() < 0) {
      log_err(LD_OR, "Error initializing keys; exiting");
      return -1;
    }
  }

  /* Set up our buckets */
  connection_bucket_init();

  /* initialize the bootstrap status events to know we're starting up */
  control_event_bootstrap(BOOTSTRAP_STATUS_STARTING, 0);

  /* Initialize the keypinning log. */
  if (authdir_mode_v3(get_options())) {
    char *fname = get_datadir_fname("key-pinning-journal");
    int r = 0;
    if (keypin_load_journal(fname)<0) {
      log_err(LD_DIR, "Error loading key-pinning journal: %s",strerror(errno));
      r = -1;
    }
    if (keypin_open_journal(fname)<0) {
      log_err(LD_DIR, "Error opening key-pinning journal: %s",strerror(errno));
      r = -1;
    }
    tor_free(fname);
    if (r)
      return r;
  }
  {
    /* This is the old name for key-pinning-journal.  These got corrupted
     * in a couple of cases by #16530, so we started over. See #16580 for
     * the rationale and for other options we didn't take.  We can remove
     * this code once all the authorities that ran 0.2.7.1-alpha-dev are
     * upgraded.
     */
    char *fname = get_datadir_fname("key-pinning-entries");
    unlink(fname);
    tor_free(fname);
  }

  if (trusted_dirs_reload_certs()) {
    log_warn(LD_DIR,
             "Couldn't load all cached v3 certificates. Starting anyway.");
  }
  if (router_reload_consensus_networkstatus()) {
    return -1;
  }
  /* load the routers file, or assign the defaults. */
  if (router_reload_router_list()) {
    return -1;
  }
  /* load the networkstatuses. (This launches a download for new routers as
   * appropriate.)
   */
  const time_t now = time(NULL);
  directory_info_has_arrived(now, 1, 0);

  if (server_mode(get_options()) || dir_server_mode(get_options())) {
    /* launch cpuworkers. Need to do this *after* we've read the onion key. */
    cpu_init();
  }
  consdiffmgr_enable_background_compression();

  /* Setup shared random protocol subsystem. */
  if (authdir_mode_v3(get_options())) {
    if (sr_init(1) < 0) {
      return -1;
    }
  }

  /* initialize dns resolve map, spawn workers if needed */
  if (dns_init() < 0) {
    if (get_options()->ServerDNSAllowBrokenConfig)
      log_warn(LD_GENERAL, "Couldn't set up any working nameservers. "
               "Network not up yet?  Will try again soon.");
    else {
      log_err(LD_GENERAL,"Error initializing dns subsystem; exiting.  To "
              "retry instead, set the ServerDNSAllowBrokenResolvConf option.");
    }
  }

#ifdef HAVE_SYSTEMD
  {
    const int r = sd_notify(0, "READY=1");
    if (r < 0) {
      log_warn(LD_GENERAL, "Unable to send readiness to systemd: %s",
               strerror(r));
    } else if (r > 0) {
      log_notice(LD_GENERAL, "Signaled readiness to systemd");
    } else {
      log_info(LD_GENERAL, "Systemd NOTIFY_SOCKET not present.");
    }
  }
#endif /* defined(HAVE_SYSTEMD) */

  return do_main_loop();
}

/* Main entry point for the Tor process.  Called from tor_main(), and by
 * anybody embedding Tor. */
int
tor_run_main(const tor_main_configuration_t *tor_cfg)
{
  int result = 0;

#ifdef _WIN32
#ifndef HeapEnableTerminationOnCorruption
#define HeapEnableTerminationOnCorruption 1
#endif
  /* On heap corruption, just give up; don't try to play along. */
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

  /* SetProcessDEPPolicy is only supported on 32-bit Windows.
   * (On 64-bit Windows it always fails, and some compilers don't like the
   * PSETDEP cast.)
   * 32-bit Windows defines _WIN32.
   * 64-bit Windows defines _WIN32 and _WIN64. */
#ifndef _WIN64
  /* Call SetProcessDEPPolicy to permanently enable DEP.
     The function will not resolve on earlier versions of Windows,
     and failure is not dangerous. */
  HMODULE hMod = GetModuleHandleA("Kernel32.dll");
  if (hMod) {
    typedef BOOL (WINAPI *PSETDEP)(DWORD);
    PSETDEP setdeppolicy = (PSETDEP)GetProcAddress(hMod,
                           "SetProcessDEPPolicy");
    if (setdeppolicy) {
      /* PROCESS_DEP_ENABLE | PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION */
      setdeppolicy(3);
    }
  }
#endif /* !defined(_WIN64) */
#endif /* defined(_WIN32) */

  {
    int bt_err = configure_backtrace_handler(get_version());
    if (bt_err < 0) {
      log_warn(LD_BUG, "Unable to install backtrace handler: %s",
               strerror(-bt_err));
    }
  }

#ifdef EVENT_SET_MEM_FUNCTIONS_IMPLEMENTED
  event_set_mem_functions(tor_malloc_, tor_realloc_, tor_free_);
#endif

  init_protocol_warning_severity_level();

  update_approx_time(time(NULL));
  tor_threads_init();
  tor_compress_init();
  init_logging(0);
  monotime_init();

  int argc = tor_cfg->argc + tor_cfg->argc_owned;
  char **argv = tor_calloc(argc, sizeof(char*));
  memcpy(argv, tor_cfg->argv, tor_cfg->argc*sizeof(char*));
  if (tor_cfg->argc_owned)
    memcpy(argv + tor_cfg->argc, tor_cfg->argv_owned,
           tor_cfg->argc_owned*sizeof(char*));

#ifdef NT_SERVICE
  {
     int done = 0;
     result = nt_service_parse_options(argc, argv, &done);
     if (done) {
       goto done;
     }
  }
#endif /* defined(NT_SERVICE) */
  {
    int init_rv = tor_init(argc, argv);
    if (init_rv) {
      tor_free_all(0);
      result = (init_rv < 0) ? -1 : 0;
      goto done;
    }
  }

  if (get_options()->Sandbox && get_options()->command == CMD_RUN_TOR) {
    sandbox_cfg_t* cfg = sandbox_init_filter();

    if (sandbox_init(cfg)) {
      tor_free(argv);
      log_err(LD_BUG,"Failed to create syscall sandbox filter");
      tor_free_all(0);
      return -1;
    }

    // registering libevent rng
#ifdef HAVE_EVUTIL_SECURE_RNG_SET_URANDOM_DEVICE_FILE
    evutil_secure_rng_set_urandom_device_file(
        (char*) sandbox_intern_string("/dev/urandom"));
#endif
  }

  switch (get_options()->command) {
  case CMD_RUN_TOR:
#ifdef NT_SERVICE
    nt_service_set_state(SERVICE_RUNNING);
#endif
    result = run_tor_main_loop();
    break;
  case CMD_KEYGEN:
    result = load_ed_keys(get_options(), time(NULL)) < 0;
    break;
  case CMD_KEY_EXPIRATION:
    init_keys();
    result = log_cert_expiration();
    break;
  case CMD_LIST_FINGERPRINT:
    result = do_list_fingerprint();
    break;
  case CMD_HASH_PASSWORD:
    do_hash_password();
    result = 0;
    break;
  case CMD_VERIFY_CONFIG:
    if (quiet_level == 0)
      printf("Configuration was valid\n");
    result = 0;
    break;
  case CMD_DUMP_CONFIG:
    result = do_dump_config();
    break;
  case CMD_RUN_UNITTESTS: /* only set by test.c */
  default:
    log_warn(LD_BUG,"Illegal command number %d: internal error.",
             get_options()->command);
    result = -1;
  }
  tor_cleanup();
 done:
  tor_free(argv);
  return result;
}
