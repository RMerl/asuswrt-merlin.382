/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file mainloop.c
 * \brief Toplevel module. Handles signals, multiplexes between
 *     connections, implements main loop, and drives scheduled events.
 *
 * For the main loop itself; see run_main_loop_once().  It invokes the rest of
 * Tor mostly through Libevent callbacks.  Libevent callbacks can happen when
 * a timer elapses, a signal is received, a socket is ready to read or write,
 * or an event is manually activated.
 *
 * Most events in Tor are driven from these callbacks:
 *  <ul>
 *   <li>conn_read_callback() and conn_write_callback() here, which are
 *     invoked when a socket is ready to read or write respectively.
 *   <li>signal_callback(), which handles incoming signals.
 *  </ul>
 * Other events are used for specific purposes, or for building more complex
 * control structures.  If you search for usage of tor_libevent_new(), you
 * will find all the events that we construct in Tor.
 *
 * Tor has numerous housekeeping operations that need to happen
 * regularly. They are handled in different ways:
 * <ul>
 *   <li>The most frequent operations are handled after every read or write
 *    event, at the end of connection_handle_read() and
 *    connection_handle_write().
 *
 *   <li>The next most frequent operations happen after each invocation of the
 *     main loop, in run_main_loop_once().
 *
 *   <li>Once per second, we run all of the operations listed in
 *     second_elapsed_callback(), and in its child, run_scheduled_events().
 *
 *   <li>Once-a-second operations are handled in second_elapsed_callback().
 *
 *   <li>More infrequent operations take place based on the periodic event
 *     driver in periodic.c .  These are stored in the periodic_events[]
 *     table.
 * </ul>
 *
 **/

#define MAINLOOP_PRIVATE
#include "core/or/or.h"

#include "app/config/config.h"
#include "app/config/statefile.h"
#include "app/main/ntmain.h"
#include "core/mainloop/connection.h"
#include "core/mainloop/cpuworker.h"
#include "core/mainloop/mainloop.h"
#include "core/mainloop/netstatus.h"
#include "core/mainloop/periodic.h"
#include "core/or/channel.h"
#include "core/or/channelpadding.h"
#include "core/or/channeltls.h"
#include "core/or/circuitbuild.h"
#include "core/or/circuitlist.h"
#include "core/or/circuituse.h"
#include "core/or/connection_edge.h"
#include "core/or/connection_or.h"
#include "core/or/dos.h"
#include "core/or/status.h"
#include "feature/client/addressmap.h"
#include "feature/client/bridges.h"
#include "feature/client/dnsserv.h"
#include "feature/client/entrynodes.h"
#include "feature/client/transports.h"
#include "feature/control/control.h"
#include "feature/dirauth/authmode.h"
#include "feature/dirauth/reachability.h"
#include "feature/dircache/consdiffmgr.h"
#include "feature/dircache/dirserv.h"
#include "feature/dircommon/directory.h"
#include "feature/hibernate/hibernate.h"
#include "feature/hs/hs_cache.h"
#include "feature/hs/hs_client.h"
#include "feature/hs/hs_service.h"
#include "feature/nodelist/microdesc.h"
#include "feature/nodelist/networkstatus.h"
#include "feature/nodelist/nodelist.h"
#include "feature/nodelist/routerlist.h"
#include "feature/relay/dns.h"
#include "feature/relay/routerkeys.h"
#include "feature/relay/routermode.h"
#include "feature/relay/selftest.h"
#include "feature/rend/rendcache.h"
#include "feature/rend/rendservice.h"
#include "feature/stats/geoip_stats.h"
#include "feature/stats/predict_ports.h"
#include "feature/stats/rephist.h"
#include "lib/container/buffers.h"
#include "lib/crypt_ops/crypto_rand.h"
#include "lib/err/backtrace.h"
#include "lib/tls/buffers_tls.h"

#include "lib/net/buffers_net.h"
#include "lib/evloop/compat_libevent.h"

#include <event2/event.h>

#include "feature/dirauth/dirvote.h"
#include "feature/dirauth/authmode.h"

#include "core/or/cell_st.h"
#include "core/or/entry_connection_st.h"
#include "feature/nodelist/networkstatus_st.h"
#include "core/or/or_connection_st.h"
#include "app/config/or_state_st.h"
#include "feature/nodelist/routerinfo_st.h"
#include "core/or/socks_request_st.h"

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

/* Token bucket for all traffic. */
token_bucket_rw_t global_bucket;

/* Token bucket for relayed traffic. */
token_bucket_rw_t global_relayed_bucket;

/* XXX we might want to keep stats about global_relayed_*_bucket too. Or not.*/
/** How many bytes have we read since we started the process? */
static uint64_t stats_n_bytes_read = 0;
/** How many bytes have we written since we started the process? */
static uint64_t stats_n_bytes_written = 0;
/** What time did this process start up? */
time_t time_of_process_start = 0;
/** How many seconds have we been running? */
static long stats_n_seconds_working = 0;
/** How many times have we returned from the main loop successfully? */
static uint64_t stats_n_main_loop_successes = 0;
/** How many times have we received an error from the main loop? */
static uint64_t stats_n_main_loop_errors = 0;
/** How many times have we returned from the main loop with no events. */
static uint64_t stats_n_main_loop_idle = 0;

/** How often will we honor SIGNEWNYM requests? */
#define MAX_SIGNEWNYM_RATE 10
/** When did we last process a SIGNEWNYM request? */
static time_t time_of_last_signewnym = 0;
/** Is there a signewnym request we're currently waiting to handle? */
static int signewnym_is_pending = 0;
/** Mainloop event for the deferred signewnym call. */
static mainloop_event_t *handle_deferred_signewnym_ev = NULL;
/** How many times have we called newnym? */
static unsigned newnym_epoch = 0;

/** Smartlist of all open connections. */
STATIC smartlist_t *connection_array = NULL;
/** List of connections that have been marked for close and need to be freed
 * and removed from connection_array. */
static smartlist_t *closeable_connection_lst = NULL;
/** List of linked connections that are currently reading data into their
 * inbuf from their partner's outbuf. */
static smartlist_t *active_linked_connection_lst = NULL;
/** Flag: Set to true iff we entered the current libevent main loop via
 * <b>loop_once</b>. If so, there's no need to trigger a loopexit in order
 * to handle linked connections. */
static int called_loop_once = 0;
/** Flag: if true, it's time to shut down, so the main loop should exit as
 * soon as possible.
 */
static int main_loop_should_exit = 0;
/** The return value that the main loop should yield when it exits, if
 * main_loop_should_exit is true.
 */
static int main_loop_exit_value = 0;

/** We set this to 1 when we've opened a circuit, so we can print a log
 * entry to inform the user that Tor is working.  We set it to 0 when
 * we think the fact that we once opened a circuit doesn't mean we can do so
 * any longer (a big time jump happened, when we notice our directory is
 * heinously out-of-date, etc.
 */
static int can_complete_circuits = 0;

/** How often do we check for router descriptors that we should download
 * when we have too little directory info? */
#define GREEDY_DESCRIPTOR_RETRY_INTERVAL (10)
/** How often do we check for router descriptors that we should download
 * when we have enough directory info? */
#define LAZY_DESCRIPTOR_RETRY_INTERVAL (60)

static int conn_close_if_marked(int i);
static int run_main_loop_until_done(void);
static void connection_start_reading_from_linked_conn(connection_t *conn);
static int connection_should_read_from_linked_conn(connection_t *conn);
static void conn_read_callback(evutil_socket_t fd, short event, void *_conn);
static void conn_write_callback(evutil_socket_t fd, short event, void *_conn);
static void second_elapsed_callback(periodic_timer_t *timer, void *args);
static void shutdown_did_not_work_callback(evutil_socket_t fd, short event,
                                           void *arg) ATTR_NORETURN;

/****************************************************************************
 *
 * This section contains accessors and other methods on the connection_array
 * variables (which are global within this file and unavailable outside it).
 *
 ****************************************************************************/

/** Return 1 if we have successfully built a circuit, and nothing has changed
 * to make us think that maybe we can't.
 */
int
have_completed_a_circuit(void)
{
  return can_complete_circuits;
}

/** Note that we have successfully built a circuit, so that reachability
 * testing and introduction points and so on may be attempted. */
void
note_that_we_completed_a_circuit(void)
{
  can_complete_circuits = 1;
}

/** Note that something has happened (like a clock jump, or DisableNetwork) to
 * make us think that maybe we can't complete circuits. */
void
note_that_we_maybe_cant_complete_circuits(void)
{
  can_complete_circuits = 0;
}

/** Add <b>conn</b> to the array of connections that we can poll on.  The
 * connection's socket must be set; the connection starts out
 * non-reading and non-writing.
 */
int
connection_add_impl(connection_t *conn, int is_connecting)
{
  tor_assert(conn);
  tor_assert(SOCKET_OK(conn->s) ||
             conn->linked ||
             (conn->type == CONN_TYPE_AP &&
              TO_EDGE_CONN(conn)->is_dns_request));

  tor_assert(conn->conn_array_index == -1); /* can only connection_add once */
  conn->conn_array_index = smartlist_len(connection_array);
  smartlist_add(connection_array, conn);

  (void) is_connecting;

  if (SOCKET_OK(conn->s) || conn->linked) {
    conn->read_event = tor_event_new(tor_libevent_get_base(),
         conn->s, EV_READ|EV_PERSIST, conn_read_callback, conn);
    conn->write_event = tor_event_new(tor_libevent_get_base(),
         conn->s, EV_WRITE|EV_PERSIST, conn_write_callback, conn);
    /* XXXX CHECK FOR NULL RETURN! */
  }

  log_debug(LD_NET,"new conn type %s, socket %d, address %s, n_conns %d.",
            conn_type_to_string(conn->type), (int)conn->s, conn->address,
            smartlist_len(connection_array));

  return 0;
}

/** Tell libevent that we don't care about <b>conn</b> any more. */
void
connection_unregister_events(connection_t *conn)
{
  if (conn->read_event) {
    if (event_del(conn->read_event))
      log_warn(LD_BUG, "Error removing read event for %d", (int)conn->s);
    tor_free(conn->read_event);
  }
  if (conn->write_event) {
    if (event_del(conn->write_event))
      log_warn(LD_BUG, "Error removing write event for %d", (int)conn->s);
    tor_free(conn->write_event);
  }
  if (conn->type == CONN_TYPE_AP_DNS_LISTENER) {
    dnsserv_close_listener(conn);
  }
}

/** Remove the connection from the global list, and remove the
 * corresponding poll entry.  Calling this function will shift the last
 * connection (if any) into the position occupied by conn.
 */
int
connection_remove(connection_t *conn)
{
  int current_index;
  connection_t *tmp;

  tor_assert(conn);

  log_debug(LD_NET,"removing socket %d (type %s), n_conns now %d",
            (int)conn->s, conn_type_to_string(conn->type),
            smartlist_len(connection_array));

  if (conn->type == CONN_TYPE_AP && conn->socket_family == AF_UNIX) {
    log_info(LD_NET, "Closing SOCKS Unix socket connection");
  }

  control_event_conn_bandwidth(conn);

  tor_assert(conn->conn_array_index >= 0);
  current_index = conn->conn_array_index;
  connection_unregister_events(conn); /* This is redundant, but cheap. */
  if (current_index == smartlist_len(connection_array)-1) { /* at the end */
    smartlist_del(connection_array, current_index);
    return 0;
  }

  /* replace this one with the one at the end */
  smartlist_del(connection_array, current_index);
  tmp = smartlist_get(connection_array, current_index);
  tmp->conn_array_index = current_index;

  return 0;
}

/** If <b>conn</b> is an edge conn, remove it from the list
 * of conn's on this circuit. If it's not on an edge,
 * flush and send destroys for all circuits on this conn.
 *
 * Remove it from connection_array (if applicable) and
 * from closeable_connection_list.
 *
 * Then free it.
 */
static void
connection_unlink(connection_t *conn)
{
  connection_about_to_close_connection(conn);
  if (conn->conn_array_index >= 0) {
    connection_remove(conn);
  }
  if (conn->linked_conn) {
    conn->linked_conn->linked_conn = NULL;
    if (! conn->linked_conn->marked_for_close &&
        conn->linked_conn->reading_from_linked_conn)
      connection_start_reading(conn->linked_conn);
    conn->linked_conn = NULL;
  }
  smartlist_remove(closeable_connection_lst, conn);
  smartlist_remove(active_linked_connection_lst, conn);
  if (conn->type == CONN_TYPE_EXIT) {
    assert_connection_edge_not_dns_pending(TO_EDGE_CONN(conn));
  }
  if (conn->type == CONN_TYPE_OR) {
    if (!tor_digest_is_zero(TO_OR_CONN(conn)->identity_digest))
      connection_or_clear_identity(TO_OR_CONN(conn));
    /* connection_unlink() can only get called if the connection
     * was already on the closeable list, and it got there by
     * connection_mark_for_close(), which was called from
     * connection_or_close_normally() or
     * connection_or_close_for_error(), so the channel should
     * already be in CHANNEL_STATE_CLOSING, and then the
     * connection_about_to_close_connection() goes to
     * connection_or_about_to_close(), which calls channel_closed()
     * to notify the channel_t layer, and closed the channel, so
     * nothing more to do here to deal with the channel associated
     * with an orconn.
     */
  }
  connection_free(conn);
}

/** Event that invokes schedule_active_linked_connections_cb. */
static mainloop_event_t *schedule_active_linked_connections_event = NULL;

/**
 * Callback: used to activate read events for all linked connections, so
 * libevent knows to call their read callbacks.  This callback run as a
 * postloop event, so that the events _it_ activates don't happen until
 * Libevent has a chance to check for other events.
 */
static void
schedule_active_linked_connections_cb(mainloop_event_t *event, void *arg)
{
  (void)event;
  (void)arg;

  /* All active linked conns should get their read events activated,
   * so that libevent knows to run their callbacks. */
  SMARTLIST_FOREACH(active_linked_connection_lst, connection_t *, conn,
                    event_active(conn->read_event, EV_READ, 1));

  /* Reactivate the event if we still have connections in the active list.
   *
   * A linked connection doesn't get woken up by I/O but rather artificially
   * by this event callback. It has directory data spooled in it and it is
   * sent incrementally by small chunks unless spool_eagerly is true. For that
   * to happen, we need to induce the activation of the read event so it can
   * be flushed. */
  if (smartlist_len(active_linked_connection_lst)) {
    mainloop_event_activate(schedule_active_linked_connections_event);
  }
}

/** Initialize the global connection list, closeable connection list,
 * and active connection list. */
void
tor_init_connection_lists(void)
{
  if (!connection_array)
    connection_array = smartlist_new();
  if (!closeable_connection_lst)
    closeable_connection_lst = smartlist_new();
  if (!active_linked_connection_lst)
    active_linked_connection_lst = smartlist_new();
}

/** Schedule <b>conn</b> to be closed. **/
void
add_connection_to_closeable_list(connection_t *conn)
{
  tor_assert(!smartlist_contains(closeable_connection_lst, conn));
  tor_assert(conn->marked_for_close);
  assert_connection_ok(conn, time(NULL));
  smartlist_add(closeable_connection_lst, conn);
  mainloop_schedule_postloop_cleanup();
}

/** Return 1 if conn is on the closeable list, else return 0. */
int
connection_is_on_closeable_list(connection_t *conn)
{
  return smartlist_contains(closeable_connection_lst, conn);
}

/** Return true iff conn is in the current poll array. */
int
connection_in_array(connection_t *conn)
{
  return smartlist_contains(connection_array, conn);
}

/** Set <b>*array</b> to an array of all connections. <b>*array</b> must not
 * be modified.
 */
MOCK_IMPL(smartlist_t *,
get_connection_array, (void))
{
  if (!connection_array)
    connection_array = smartlist_new();
  return connection_array;
}

/**
 * Return the amount of network traffic read, in bytes, over the life of this
 * process.
 */
MOCK_IMPL(uint64_t,
get_bytes_read,(void))
{
  return stats_n_bytes_read;
}

/**
 * Return the amount of network traffic read, in bytes, over the life of this
 * process.
 */
MOCK_IMPL(uint64_t,
get_bytes_written,(void))
{
  return stats_n_bytes_written;
}

/**
 * Increment the amount of network traffic read and written, over the life of
 * this process.
 */
void
stats_increment_bytes_read_and_written(uint64_t r, uint64_t w)
{
  stats_n_bytes_read += r;
  stats_n_bytes_written += w;
}

/** Set the event mask on <b>conn</b> to <b>events</b>.  (The event
 * mask is a bitmask whose bits are READ_EVENT and WRITE_EVENT)
 */
void
connection_watch_events(connection_t *conn, watchable_events_t events)
{
  if (events & READ_EVENT)
    connection_start_reading(conn);
  else
    connection_stop_reading(conn);

  if (events & WRITE_EVENT)
    connection_start_writing(conn);
  else
    connection_stop_writing(conn);
}

/** Return true iff <b>conn</b> is listening for read events. */
int
connection_is_reading(connection_t *conn)
{
  tor_assert(conn);

  return conn->reading_from_linked_conn ||
    (conn->read_event && event_pending(conn->read_event, EV_READ, NULL));
}

/** Reset our main loop counters. */
void
reset_main_loop_counters(void)
{
  stats_n_main_loop_successes = 0;
  stats_n_main_loop_errors = 0;
  stats_n_main_loop_idle = 0;
}

/** Increment the main loop success counter. */
static void
increment_main_loop_success_count(void)
{
  ++stats_n_main_loop_successes;
}

/** Get the main loop success counter. */
uint64_t
get_main_loop_success_count(void)
{
  return stats_n_main_loop_successes;
}

/** Increment the main loop error counter. */
static void
increment_main_loop_error_count(void)
{
  ++stats_n_main_loop_errors;
}

/** Get the main loop error counter. */
uint64_t
get_main_loop_error_count(void)
{
  return stats_n_main_loop_errors;
}

/** Increment the main loop idle counter. */
static void
increment_main_loop_idle_count(void)
{
  ++stats_n_main_loop_idle;
}

/** Get the main loop idle counter. */
uint64_t
get_main_loop_idle_count(void)
{
  return stats_n_main_loop_idle;
}

/** Check whether <b>conn</b> is correct in having (or not having) a
 * read/write event (passed in <b>ev</b>). On success, return 0. On failure,
 * log a warning and return -1. */
static int
connection_check_event(connection_t *conn, struct event *ev)
{
  int bad;

  if (conn->type == CONN_TYPE_AP && TO_EDGE_CONN(conn)->is_dns_request) {
    /* DNS requests which we launch through the dnsserv.c module do not have
     * any underlying socket or any underlying linked connection, so they
     * shouldn't have any attached events either.
     */
    bad = ev != NULL;
  } else {
    /* Everything else should have an underlying socket, or a linked
     * connection (which is also tracked with a read_event/write_event pair).
     */
    bad = ev == NULL;
  }

  if (bad) {
    log_warn(LD_BUG, "Event missing on connection %p [%s;%s]. "
             "socket=%d. linked=%d. "
             "is_dns_request=%d. Marked_for_close=%s:%d",
             conn,
             conn_type_to_string(conn->type),
             conn_state_to_string(conn->type, conn->state),
             (int)conn->s, (int)conn->linked,
             (conn->type == CONN_TYPE_AP &&
                               TO_EDGE_CONN(conn)->is_dns_request),
             conn->marked_for_close_file ? conn->marked_for_close_file : "-",
             conn->marked_for_close
             );
    log_backtrace(LOG_WARN, LD_BUG, "Backtrace attached.");
    return -1;
  }
  return 0;
}

/** Tell the main loop to stop notifying <b>conn</b> of any read events. */
MOCK_IMPL(void,
connection_stop_reading,(connection_t *conn))
{
  tor_assert(conn);

  if (connection_check_event(conn, conn->read_event) < 0) {
    return;
  }

  if (conn->linked) {
    conn->reading_from_linked_conn = 0;
    connection_stop_reading_from_linked_conn(conn);
  } else {
    if (event_del(conn->read_event))
      log_warn(LD_NET, "Error from libevent setting read event state for %d "
               "to unwatched: %s",
               (int)conn->s,
               tor_socket_strerror(tor_socket_errno(conn->s)));
  }
}

/** Tell the main loop to start notifying <b>conn</b> of any read events. */
MOCK_IMPL(void,
connection_start_reading,(connection_t *conn))
{
  tor_assert(conn);

  if (connection_check_event(conn, conn->read_event) < 0) {
    return;
  }

  if (conn->linked) {
    conn->reading_from_linked_conn = 1;
    if (connection_should_read_from_linked_conn(conn))
      connection_start_reading_from_linked_conn(conn);
  } else {
    if (event_add(conn->read_event, NULL))
      log_warn(LD_NET, "Error from libevent setting read event state for %d "
               "to watched: %s",
               (int)conn->s,
               tor_socket_strerror(tor_socket_errno(conn->s)));
  }
}

/** Return true iff <b>conn</b> is listening for write events. */
int
connection_is_writing(connection_t *conn)
{
  tor_assert(conn);

  return conn->writing_to_linked_conn ||
    (conn->write_event && event_pending(conn->write_event, EV_WRITE, NULL));
}

/** Tell the main loop to stop notifying <b>conn</b> of any write events. */
MOCK_IMPL(void,
connection_stop_writing,(connection_t *conn))
{
  tor_assert(conn);

  if (connection_check_event(conn, conn->write_event) < 0) {
    return;
  }

  if (conn->linked) {
    conn->writing_to_linked_conn = 0;
    if (conn->linked_conn)
      connection_stop_reading_from_linked_conn(conn->linked_conn);
  } else {
    if (event_del(conn->write_event))
      log_warn(LD_NET, "Error from libevent setting write event state for %d "
               "to unwatched: %s",
               (int)conn->s,
               tor_socket_strerror(tor_socket_errno(conn->s)));
  }
}

/** Tell the main loop to start notifying <b>conn</b> of any write events. */
MOCK_IMPL(void,
connection_start_writing,(connection_t *conn))
{
  tor_assert(conn);

  if (connection_check_event(conn, conn->write_event) < 0) {
    return;
  }

  if (conn->linked) {
    conn->writing_to_linked_conn = 1;
    if (conn->linked_conn &&
        connection_should_read_from_linked_conn(conn->linked_conn))
      connection_start_reading_from_linked_conn(conn->linked_conn);
  } else {
    if (event_add(conn->write_event, NULL))
      log_warn(LD_NET, "Error from libevent setting write event state for %d "
               "to watched: %s",
               (int)conn->s,
               tor_socket_strerror(tor_socket_errno(conn->s)));
  }
}

/** Return true iff <b>conn</b> is linked conn, and reading from the conn
 * linked to it would be good and feasible.  (Reading is "feasible" if the
 * other conn exists and has data in its outbuf, and is "good" if we have our
 * reading_from_linked_conn flag set and the other conn has its
 * writing_to_linked_conn flag set.)*/
static int
connection_should_read_from_linked_conn(connection_t *conn)
{
  if (conn->linked && conn->reading_from_linked_conn) {
    if (! conn->linked_conn ||
        (conn->linked_conn->writing_to_linked_conn &&
         buf_datalen(conn->linked_conn->outbuf)))
      return 1;
  }
  return 0;
}

/** Event to run 'shutdown did not work callback'. */
static struct event *shutdown_did_not_work_event = NULL;

/** Failsafe measure that should never actually be necessary: If
 * tor_shutdown_event_loop_and_exit() somehow doesn't successfully exit the
 * event loop, then this callback will kill Tor with an assertion failure
 * seconds later
 */
static void
shutdown_did_not_work_callback(evutil_socket_t fd, short event, void *arg)
{
  // LCOV_EXCL_START
  (void) fd;
  (void) event;
  (void) arg;
  tor_assert_unreached();
  // LCOV_EXCL_STOP
}

#ifdef ENABLE_RESTART_DEBUGGING
static struct event *tor_shutdown_event_loop_for_restart_event = NULL;
static void
tor_shutdown_event_loop_for_restart_cb(
                      evutil_socket_t fd, short event, void *arg)
{
  (void)fd;
  (void)event;
  (void)arg;
  tor_event_free(tor_shutdown_event_loop_for_restart_event);
  tor_shutdown_event_loop_and_exit(0);
}
#endif

/**
 * After finishing the current callback (if any), shut down the main loop,
 * clean up the process, and exit with <b>exitcode</b>.
 */
void
tor_shutdown_event_loop_and_exit(int exitcode)
{
  if (main_loop_should_exit)
    return; /* Ignore multiple calls to this function. */

  main_loop_should_exit = 1;
  main_loop_exit_value = exitcode;

  /* Die with an assertion failure in ten seconds, if for some reason we don't
   * exit normally. */
  /* XXXX We should consider this code if it's never used. */
  struct timeval ten_seconds = { 10, 0 };
  shutdown_did_not_work_event = tor_evtimer_new(
                  tor_libevent_get_base(),
                  shutdown_did_not_work_callback, NULL);
  event_add(shutdown_did_not_work_event, &ten_seconds);

  /* Unlike exit_loop_after_delay(), exit_loop_after_callback
   * prevents other callbacks from running. */
  tor_libevent_exit_loop_after_callback(tor_libevent_get_base());
}

/** Return true iff tor_shutdown_event_loop_and_exit() has been called. */
int
tor_event_loop_shutdown_is_pending(void)
{
  return main_loop_should_exit;
}

/** Helper: Tell the main loop to begin reading bytes into <b>conn</b> from
 * its linked connection, if it is not doing so already.  Called by
 * connection_start_reading and connection_start_writing as appropriate. */
static void
connection_start_reading_from_linked_conn(connection_t *conn)
{
  tor_assert(conn);
  tor_assert(conn->linked == 1);

  if (!conn->active_on_link) {
    conn->active_on_link = 1;
    smartlist_add(active_linked_connection_lst, conn);
    mainloop_event_activate(schedule_active_linked_connections_event);
  } else {
    tor_assert(smartlist_contains(active_linked_connection_lst, conn));
  }
}

/** Tell the main loop to stop reading bytes into <b>conn</b> from its linked
 * connection, if is currently doing so.  Called by connection_stop_reading,
 * connection_stop_writing, and connection_read. */
void
connection_stop_reading_from_linked_conn(connection_t *conn)
{
  tor_assert(conn);
  tor_assert(conn->linked == 1);

  if (conn->active_on_link) {
    conn->active_on_link = 0;
    /* FFFF We could keep an index here so we can smartlist_del
     * cleanly.  On the other hand, this doesn't show up on profiles,
     * so let's leave it alone for now. */
    smartlist_remove(active_linked_connection_lst, conn);
  } else {
    tor_assert(!smartlist_contains(active_linked_connection_lst, conn));
  }
}

/** Close all connections that have been scheduled to get closed. */
STATIC void
close_closeable_connections(void)
{
  int i;
  for (i = 0; i < smartlist_len(closeable_connection_lst); ) {
    connection_t *conn = smartlist_get(closeable_connection_lst, i);
    if (conn->conn_array_index < 0) {
      connection_unlink(conn); /* blow it away right now */
    } else {
      if (!conn_close_if_marked(conn->conn_array_index))
        ++i;
    }
  }
}

/** Count moribund connections for the OOS handler */
MOCK_IMPL(int,
connection_count_moribund, (void))
{
  int moribund = 0;

  /*
   * Count things we'll try to kill when close_closeable_connections()
   * runs next.
   */
  SMARTLIST_FOREACH_BEGIN(closeable_connection_lst, connection_t *, conn) {
    if (SOCKET_OK(conn->s) && connection_is_moribund(conn)) ++moribund;
  } SMARTLIST_FOREACH_END(conn);

  return moribund;
}

/** Libevent callback: this gets invoked when (connection_t*)<b>conn</b> has
 * some data to read. */
static void
conn_read_callback(evutil_socket_t fd, short event, void *_conn)
{
  connection_t *conn = _conn;
  (void)fd;
  (void)event;

  log_debug(LD_NET,"socket %d wants to read.",(int)conn->s);

  /* assert_connection_ok(conn, time(NULL)); */

  if (connection_handle_read(conn) < 0) {
    if (!conn->marked_for_close) {
#ifndef _WIN32
      log_warn(LD_BUG,"Unhandled error on read for %s connection "
               "(fd %d); removing",
               conn_type_to_string(conn->type), (int)conn->s);
      tor_fragile_assert();
#endif /* !defined(_WIN32) */
      if (CONN_IS_EDGE(conn))
        connection_edge_end_errno(TO_EDGE_CONN(conn));
      connection_mark_for_close(conn);
    }
  }
  assert_connection_ok(conn, time(NULL));

  if (smartlist_len(closeable_connection_lst))
    close_closeable_connections();
}

/** Libevent callback: this gets invoked when (connection_t*)<b>conn</b> has
 * some data to write. */
static void
conn_write_callback(evutil_socket_t fd, short events, void *_conn)
{
  connection_t *conn = _conn;
  (void)fd;
  (void)events;

  LOG_FN_CONN(conn, (LOG_DEBUG, LD_NET, "socket %d wants to write.",
                     (int)conn->s));

  /* assert_connection_ok(conn, time(NULL)); */

  if (connection_handle_write(conn, 0) < 0) {
    if (!conn->marked_for_close) {
      /* this connection is broken. remove it. */
      log_fn(LOG_WARN,LD_BUG,
             "unhandled error on write for %s connection (fd %d); removing",
             conn_type_to_string(conn->type), (int)conn->s);
      tor_fragile_assert();
      if (CONN_IS_EDGE(conn)) {
        /* otherwise we cry wolf about duplicate close */
        edge_connection_t *edge_conn = TO_EDGE_CONN(conn);
        if (!edge_conn->end_reason)
          edge_conn->end_reason = END_STREAM_REASON_INTERNAL;
        edge_conn->edge_has_sent_end = 1;
      }
      connection_close_immediate(conn); /* So we don't try to flush. */
      connection_mark_for_close(conn);
    }
  }
  assert_connection_ok(conn, time(NULL));

  if (smartlist_len(closeable_connection_lst))
    close_closeable_connections();
}

/** If the connection at connection_array[i] is marked for close, then:
 *    - If it has data that it wants to flush, try to flush it.
 *    - If it _still_ has data to flush, and conn->hold_open_until_flushed is
 *      true, then leave the connection open and return.
 *    - Otherwise, remove the connection from connection_array and from
 *      all other lists, close it, and free it.
 * Returns 1 if the connection was closed, 0 otherwise.
 */
static int
conn_close_if_marked(int i)
{
  connection_t *conn;
  int retval;
  time_t now;

  conn = smartlist_get(connection_array, i);
  if (!conn->marked_for_close)
    return 0; /* nothing to see here, move along */
  now = time(NULL);
  assert_connection_ok(conn, now);
  /* assert_all_pending_dns_resolves_ok(); */

  log_debug(LD_NET,"Cleaning up connection (fd "TOR_SOCKET_T_FORMAT").",
            conn->s);

  /* If the connection we are about to close was trying to connect to
  a proxy server and failed, the client won't be able to use that
  proxy. We should warn the user about this. */
  if (conn->proxy_state == PROXY_INFANT)
    log_failed_proxy_connection(conn);

  if ((SOCKET_OK(conn->s) || conn->linked_conn) &&
      connection_wants_to_flush(conn)) {
    /* s == -1 means it's an incomplete edge connection, or that the socket
     * has already been closed as unflushable. */
    ssize_t sz = connection_bucket_write_limit(conn, now);
    if (!conn->hold_open_until_flushed)
      log_info(LD_NET,
               "Conn (addr %s, fd %d, type %s, state %d) marked, but wants "
               "to flush %d bytes. (Marked at %s:%d)",
               escaped_safe_str_client(conn->address),
               (int)conn->s, conn_type_to_string(conn->type), conn->state,
               (int)conn->outbuf_flushlen,
                conn->marked_for_close_file, conn->marked_for_close);
    if (conn->linked_conn) {
      retval = buf_move_to_buf(conn->linked_conn->inbuf, conn->outbuf,
                               &conn->outbuf_flushlen);
      if (retval >= 0) {
        /* The linked conn will notice that it has data when it notices that
         * we're gone. */
        connection_start_reading_from_linked_conn(conn->linked_conn);
      }
      log_debug(LD_GENERAL, "Flushed last %d bytes from a linked conn; "
               "%d left; flushlen %d; wants-to-flush==%d", retval,
                (int)connection_get_outbuf_len(conn),
                (int)conn->outbuf_flushlen,
                connection_wants_to_flush(conn));
    } else if (connection_speaks_cells(conn)) {
      if (conn->state == OR_CONN_STATE_OPEN) {
        retval = buf_flush_to_tls(conn->outbuf, TO_OR_CONN(conn)->tls, sz,
                               &conn->outbuf_flushlen);
      } else
        retval = -1; /* never flush non-open broken tls connections */
    } else {
      retval = buf_flush_to_socket(conn->outbuf, conn->s, sz,
                                   &conn->outbuf_flushlen);
    }
    if (retval >= 0 && /* Technically, we could survive things like
                          TLS_WANT_WRITE here. But don't bother for now. */
        conn->hold_open_until_flushed && connection_wants_to_flush(conn)) {
      if (retval > 0) {
        LOG_FN_CONN(conn, (LOG_INFO,LD_NET,
                           "Holding conn (fd %d) open for more flushing.",
                           (int)conn->s));
        conn->timestamp_last_write_allowed = now; /* reset so we can flush
                                                   * more */
      } else if (sz == 0) {
        /* Also, retval==0.  If we get here, we didn't want to write anything
         * (because of rate-limiting) and we didn't. */

        /* Connection must flush before closing, but it's being rate-limited.
         * Let's remove from Libevent, and mark it as blocked on bandwidth
         * so it will be re-added on next token bucket refill. Prevents
         * busy Libevent loops where we keep ending up here and returning
         * 0 until we are no longer blocked on bandwidth.
         */
        connection_consider_empty_write_buckets(conn);
        /* Make sure that consider_empty_buckets really disabled the
         * connection: */
        if (BUG(connection_is_writing(conn))) {
          connection_write_bw_exhausted(conn, true);
        }

        /* The connection is being held due to write rate limit and thus will
         * flush its data later. We need to stop reading because this
         * connection is about to be closed once flushed. It should not
         * process anything more coming in at this stage. */
        connection_stop_reading(conn);
      }
      return 0;
    }
    if (connection_wants_to_flush(conn)) {
      log_fn(LOG_INFO, LD_NET, "We stalled too much while trying to write %d "
             "bytes to address %s.  If this happens a lot, either "
             "something is wrong with your network connection, or "
             "something is wrong with theirs. "
             "(fd %d, type %s, state %d, marked at %s:%d).",
             (int)connection_get_outbuf_len(conn),
             escaped_safe_str_client(conn->address),
             (int)conn->s, conn_type_to_string(conn->type), conn->state,
             conn->marked_for_close_file,
             conn->marked_for_close);
    }
  }

  connection_unlink(conn); /* unlink, remove, free */
  return 1;
}

/** Implementation for directory_all_unreachable.  This is done in a callback,
 * since otherwise it would complicate Tor's control-flow graph beyond all
 * reason.
 */
static void
directory_all_unreachable_cb(mainloop_event_t *event, void *arg)
{
  (void)event;
  (void)arg;

  connection_t *conn;

  while ((conn = connection_get_by_type_state(CONN_TYPE_AP,
                                              AP_CONN_STATE_CIRCUIT_WAIT))) {
    entry_connection_t *entry_conn = TO_ENTRY_CONN(conn);
    log_notice(LD_NET,
               "Is your network connection down? "
               "Failing connection to '%s:%d'.",
               safe_str_client(entry_conn->socks_request->address),
               entry_conn->socks_request->port);
    connection_mark_unattached_ap(entry_conn,
                                  END_STREAM_REASON_NET_UNREACHABLE);
  }
  control_event_general_error("DIR_ALL_UNREACHABLE");
}

static mainloop_event_t *directory_all_unreachable_cb_event = NULL;

/** We've just tried every dirserver we know about, and none of
 * them were reachable. Assume the network is down. Change state
 * so next time an application connection arrives we'll delay it
 * and try another directory fetch. Kill off all the circuit_wait
 * streams that are waiting now, since they will all timeout anyway.
 */
void
directory_all_unreachable(time_t now)
{
  (void)now;

  reset_uptime(); /* reset it */

  if (!directory_all_unreachable_cb_event) {
    directory_all_unreachable_cb_event =
      mainloop_event_new(directory_all_unreachable_cb, NULL);
    tor_assert(directory_all_unreachable_cb_event);
  }

  mainloop_event_activate(directory_all_unreachable_cb_event);
}

/** This function is called whenever we successfully pull down some new
 * network statuses or server descriptors. */
void
directory_info_has_arrived(time_t now, int from_cache, int suppress_logs)
{
  const or_options_t *options = get_options();

  /* if we have enough dir info, then update our guard status with
   * whatever we just learned. */
  int invalidate_circs = guards_update_all();

  if (invalidate_circs) {
    circuit_mark_all_unused_circs();
    circuit_mark_all_dirty_circs_as_unusable();
  }

  if (!router_have_minimum_dir_info()) {
    int quiet = suppress_logs || from_cache ||
                directory_too_idle_to_fetch_descriptors(options, now);
    tor_log(quiet ? LOG_INFO : LOG_NOTICE, LD_DIR,
        "I learned some more directory information, but not enough to "
        "build a circuit: %s", get_dir_info_status_string());
    update_all_descriptor_downloads(now);
    return;
  } else {
    if (directory_fetches_from_authorities(options)) {
      update_all_descriptor_downloads(now);
    }

    /* Don't even bother trying to get extrainfo until the rest of our
     * directory info is up-to-date */
    if (options->DownloadExtraInfo)
      update_extrainfo_downloads(now);
  }

  if (server_mode(options) && !net_is_disabled() && !from_cache &&
      (have_completed_a_circuit() || !any_predicted_circuits(now)))
   router_do_reachability_checks(1, 1);
}

/** Perform regular maintenance tasks for a single connection.  This
 * function gets run once per second per connection by run_scheduled_events.
 */
static void
run_connection_housekeeping(int i, time_t now)
{
  cell_t cell;
  connection_t *conn = smartlist_get(connection_array, i);
  const or_options_t *options = get_options();
  or_connection_t *or_conn;
  channel_t *chan = NULL;
  int have_any_circuits;
  int past_keepalive =
    now >= conn->timestamp_last_write_allowed + options->KeepalivePeriod;

  if (conn->outbuf && !connection_get_outbuf_len(conn) &&
      conn->type == CONN_TYPE_OR)
    TO_OR_CONN(conn)->timestamp_lastempty = now;

  if (conn->marked_for_close) {
    /* nothing to do here */
    return;
  }

  /* Expire any directory connections that haven't been active (sent
   * if a server or received if a client) for 5 min */
  if (conn->type == CONN_TYPE_DIR &&
      ((DIR_CONN_IS_SERVER(conn) &&
        conn->timestamp_last_write_allowed
            + options->TestingDirConnectionMaxStall < now) ||
       (!DIR_CONN_IS_SERVER(conn) &&
        conn->timestamp_last_read_allowed
            + options->TestingDirConnectionMaxStall < now))) {
    log_info(LD_DIR,"Expiring wedged directory conn (fd %d, purpose %d)",
             (int)conn->s, conn->purpose);
    /* This check is temporary; it's to let us know whether we should consider
     * parsing partial serverdesc responses. */
    if (conn->purpose == DIR_PURPOSE_FETCH_SERVERDESC &&
        connection_get_inbuf_len(conn) >= 1024) {
      log_info(LD_DIR,"Trying to extract information from wedged server desc "
               "download.");
      connection_dir_reached_eof(TO_DIR_CONN(conn));
    } else {
      connection_mark_for_close(conn);
    }
    return;
  }

  if (!connection_speaks_cells(conn))
    return; /* we're all done here, the rest is just for OR conns */

  /* If we haven't flushed to an OR connection for a while, then either nuke
     the connection or send a keepalive, depending. */

  or_conn = TO_OR_CONN(conn);
  tor_assert(conn->outbuf);

  chan = TLS_CHAN_TO_BASE(or_conn->chan);
  tor_assert(chan);

  if (channel_num_circuits(chan) != 0) {
    have_any_circuits = 1;
    chan->timestamp_last_had_circuits = now;
  } else {
    have_any_circuits = 0;
  }

  if (channel_is_bad_for_new_circs(TLS_CHAN_TO_BASE(or_conn->chan)) &&
      ! have_any_circuits) {
    /* It's bad for new circuits, and has no unmarked circuits on it:
     * mark it now. */
    log_info(LD_OR,
             "Expiring non-used OR connection to fd %d (%s:%d) [Too old].",
             (int)conn->s, conn->address, conn->port);
    if (conn->state == OR_CONN_STATE_CONNECTING)
      connection_or_connect_failed(TO_OR_CONN(conn),
                                   END_OR_CONN_REASON_TIMEOUT,
                                   "Tor gave up on the connection");
    connection_or_close_normally(TO_OR_CONN(conn), 1);
  } else if (!connection_state_is_open(conn)) {
    if (past_keepalive) {
      /* We never managed to actually get this connection open and happy. */
      log_info(LD_OR,"Expiring non-open OR connection to fd %d (%s:%d).",
               (int)conn->s,conn->address, conn->port);
      connection_or_close_normally(TO_OR_CONN(conn), 0);
    }
  } else if (we_are_hibernating() &&
             ! have_any_circuits &&
             !connection_get_outbuf_len(conn)) {
    /* We're hibernating or shutting down, there's no circuits, and nothing to
     * flush.*/
    log_info(LD_OR,"Expiring non-used OR connection to fd %d (%s:%d) "
             "[Hibernating or exiting].",
             (int)conn->s,conn->address, conn->port);
    connection_or_close_normally(TO_OR_CONN(conn), 1);
  } else if (!have_any_circuits &&
             now - or_conn->idle_timeout >=
                                         chan->timestamp_last_had_circuits) {
    log_info(LD_OR,"Expiring non-used OR connection %"PRIu64" to fd %d "
             "(%s:%d) [no circuits for %d; timeout %d; %scanonical].",
             (chan->global_identifier),
             (int)conn->s, conn->address, conn->port,
             (int)(now - chan->timestamp_last_had_circuits),
             or_conn->idle_timeout,
             or_conn->is_canonical ? "" : "non");
    connection_or_close_normally(TO_OR_CONN(conn), 0);
  } else if (
      now >= or_conn->timestamp_lastempty + options->KeepalivePeriod*10 &&
      now >=
          conn->timestamp_last_write_allowed + options->KeepalivePeriod*10) {
    log_fn(LOG_PROTOCOL_WARN,LD_PROTOCOL,
           "Expiring stuck OR connection to fd %d (%s:%d). (%d bytes to "
           "flush; %d seconds since last write)",
           (int)conn->s, conn->address, conn->port,
           (int)connection_get_outbuf_len(conn),
           (int)(now-conn->timestamp_last_write_allowed));
    connection_or_close_normally(TO_OR_CONN(conn), 0);
  } else if (past_keepalive && !connection_get_outbuf_len(conn)) {
    /* send a padding cell */
    log_fn(LOG_DEBUG,LD_OR,"Sending keepalive to (%s:%d)",
           conn->address, conn->port);
    memset(&cell,0,sizeof(cell_t));
    cell.command = CELL_PADDING;
    connection_or_write_cell_to_buf(&cell, or_conn);
  } else {
    channelpadding_decide_to_pad_channel(chan);
  }
}

/** Honor a NEWNYM request: make future requests unlinkable to past
 * requests. */
static void
signewnym_impl(time_t now)
{
  const or_options_t *options = get_options();
  if (!proxy_mode(options)) {
    log_info(LD_CONTROL, "Ignoring SIGNAL NEWNYM because client functionality "
             "is disabled.");
    return;
  }

  circuit_mark_all_dirty_circs_as_unusable();
  addressmap_clear_transient();
  hs_client_purge_state();
  time_of_last_signewnym = now;
  signewnym_is_pending = 0;

  ++newnym_epoch;

  control_event_signal(SIGNEWNYM);
}

/** Callback: run a deferred signewnym. */
static void
handle_deferred_signewnym_cb(mainloop_event_t *event, void *arg)
{
  (void)event;
  (void)arg;
  log_info(LD_CONTROL, "Honoring delayed NEWNYM request");
  do_signewnym(time(NULL));
}

/** Either perform a signewnym or schedule one, depending on rate limiting. */
void
do_signewnym(time_t now)
{
  if (time_of_last_signewnym + MAX_SIGNEWNYM_RATE > now) {
    const time_t delay_sec =
      time_of_last_signewnym + MAX_SIGNEWNYM_RATE - now;
    if (! signewnym_is_pending) {
      signewnym_is_pending = 1;
      if (!handle_deferred_signewnym_ev) {
        handle_deferred_signewnym_ev =
          mainloop_event_postloop_new(handle_deferred_signewnym_cb, NULL);
      }
      const struct timeval delay_tv = { delay_sec, 0 };
      mainloop_event_schedule(handle_deferred_signewnym_ev, &delay_tv);
    }
    log_notice(LD_CONTROL,
               "Rate limiting NEWNYM request: delaying by %d second(s)",
               (int)(delay_sec));
  } else {
    signewnym_impl(now);
  }
}

/** Return the number of times that signewnym has been called. */
unsigned
get_signewnym_epoch(void)
{
  return newnym_epoch;
}

/** True iff we have initialized all the members of <b>periodic_events</b>.
 * Used to prevent double-initialization. */
static int periodic_events_initialized = 0;

/* Declare all the timer callback functions... */
#undef CALLBACK
#define CALLBACK(name) \
  static int name ## _callback(time_t, const or_options_t *)
CALLBACK(add_entropy);
CALLBACK(check_authority_cert);
CALLBACK(check_canonical_channels);
CALLBACK(check_descriptor);
CALLBACK(check_dns_honesty);
CALLBACK(check_ed_keys);
CALLBACK(check_expired_networkstatus);
CALLBACK(check_for_reachability_bw);
CALLBACK(check_onion_keys_expiry_time);
CALLBACK(clean_caches);
CALLBACK(clean_consdiffmgr);
CALLBACK(dirvote);
CALLBACK(downrate_stability);
CALLBACK(expire_old_ciruits_serverside);
CALLBACK(fetch_networkstatus);
CALLBACK(heartbeat);
CALLBACK(hs_service);
CALLBACK(launch_descriptor_fetches);
CALLBACK(launch_reachability_tests);
CALLBACK(reachability_warnings);
CALLBACK(record_bridge_stats);
CALLBACK(rend_cache_failure_clean);
CALLBACK(reset_padding_counts);
CALLBACK(retry_dns);
CALLBACK(retry_listeners);
CALLBACK(rotate_onion_key);
CALLBACK(rotate_x509_certificate);
CALLBACK(save_stability);
CALLBACK(save_state);
CALLBACK(write_bridge_ns);
CALLBACK(write_stats_file);

#undef CALLBACK

/* Now we declare an array of periodic_event_item_t for each periodic event */
#define CALLBACK(name, r, f) PERIODIC_EVENT(name, r, f)

STATIC periodic_event_item_t periodic_events[] = {
  /* Everyone needs to run those. */
  CALLBACK(add_entropy, PERIODIC_EVENT_ROLE_ALL, 0),
  CALLBACK(check_expired_networkstatus, PERIODIC_EVENT_ROLE_ALL, 0),
  CALLBACK(clean_caches, PERIODIC_EVENT_ROLE_ALL, 0),
  CALLBACK(fetch_networkstatus, PERIODIC_EVENT_ROLE_ALL,
           PERIODIC_EVENT_FLAG_NEED_NET),
  CALLBACK(heartbeat, PERIODIC_EVENT_ROLE_ALL, 0),
  CALLBACK(launch_descriptor_fetches, PERIODIC_EVENT_ROLE_ALL,
           PERIODIC_EVENT_FLAG_NEED_NET),
  CALLBACK(reset_padding_counts, PERIODIC_EVENT_ROLE_ALL, 0),
  CALLBACK(retry_listeners, PERIODIC_EVENT_ROLE_ALL,
           PERIODIC_EVENT_FLAG_NEED_NET),
  CALLBACK(save_state, PERIODIC_EVENT_ROLE_ALL, 0),
  CALLBACK(rotate_x509_certificate, PERIODIC_EVENT_ROLE_ALL, 0),
  CALLBACK(write_stats_file, PERIODIC_EVENT_ROLE_ALL, 0),

  /* Routers (bridge and relay) only. */
  CALLBACK(check_descriptor, PERIODIC_EVENT_ROLE_ROUTER,
           PERIODIC_EVENT_FLAG_NEED_NET),
  CALLBACK(check_ed_keys, PERIODIC_EVENT_ROLE_ROUTER, 0),
  CALLBACK(check_for_reachability_bw, PERIODIC_EVENT_ROLE_ROUTER,
           PERIODIC_EVENT_FLAG_NEED_NET),
  CALLBACK(check_onion_keys_expiry_time, PERIODIC_EVENT_ROLE_ROUTER, 0),
  CALLBACK(expire_old_ciruits_serverside, PERIODIC_EVENT_ROLE_ROUTER,
           PERIODIC_EVENT_FLAG_NEED_NET),
  CALLBACK(reachability_warnings, PERIODIC_EVENT_ROLE_ROUTER,
           PERIODIC_EVENT_FLAG_NEED_NET),
  CALLBACK(retry_dns, PERIODIC_EVENT_ROLE_ROUTER, 0),
  CALLBACK(rotate_onion_key, PERIODIC_EVENT_ROLE_ROUTER, 0),

  /* Authorities (bridge and directory) only. */
  CALLBACK(downrate_stability, PERIODIC_EVENT_ROLE_AUTHORITIES, 0),
  CALLBACK(launch_reachability_tests, PERIODIC_EVENT_ROLE_AUTHORITIES,
           PERIODIC_EVENT_FLAG_NEED_NET),
  CALLBACK(save_stability, PERIODIC_EVENT_ROLE_AUTHORITIES, 0),

  /* Directory authority only. */
  CALLBACK(check_authority_cert, PERIODIC_EVENT_ROLE_DIRAUTH, 0),
  CALLBACK(dirvote, PERIODIC_EVENT_ROLE_DIRAUTH, PERIODIC_EVENT_FLAG_NEED_NET),

  /* Relay only. */
  CALLBACK(check_canonical_channels, PERIODIC_EVENT_ROLE_RELAY,
           PERIODIC_EVENT_FLAG_NEED_NET),
  CALLBACK(check_dns_honesty, PERIODIC_EVENT_ROLE_RELAY,
           PERIODIC_EVENT_FLAG_NEED_NET),

  /* Hidden Service service only. */
  CALLBACK(hs_service, PERIODIC_EVENT_ROLE_HS_SERVICE,
           PERIODIC_EVENT_FLAG_NEED_NET),

  /* Bridge only. */
  CALLBACK(record_bridge_stats, PERIODIC_EVENT_ROLE_BRIDGE, 0),

  /* Client only. */
  CALLBACK(rend_cache_failure_clean, PERIODIC_EVENT_ROLE_CLIENT, 0),

  /* Bridge Authority only. */
  CALLBACK(write_bridge_ns, PERIODIC_EVENT_ROLE_BRIDGEAUTH, 0),

  /* Directory server only. */
  CALLBACK(clean_consdiffmgr, PERIODIC_EVENT_ROLE_DIRSERVER, 0),

  END_OF_PERIODIC_EVENTS
};
#undef CALLBACK

/* These are pointers to members of periodic_events[] that are used to
 * implement particular callbacks.  We keep them separate here so that we
 * can access them by name.  We also keep them inside periodic_events[]
 * so that we can implement "reset all timers" in a reasonable way. */
static periodic_event_item_t *check_descriptor_event=NULL;
static periodic_event_item_t *dirvote_event=NULL;
static periodic_event_item_t *fetch_networkstatus_event=NULL;
static periodic_event_item_t *launch_descriptor_fetches_event=NULL;
static periodic_event_item_t *check_dns_honesty_event=NULL;
static periodic_event_item_t *save_state_event=NULL;

/** Reset all the periodic events so we'll do all our actions again as if we
 * just started up.
 * Useful if our clock just moved back a long time from the future,
 * so we don't wait until that future arrives again before acting.
 */
void
reset_all_main_loop_timers(void)
{
  int i;
  for (i = 0; periodic_events[i].name; ++i) {
    periodic_event_reschedule(&periodic_events[i]);
  }
}

/** Return the member of periodic_events[] whose name is <b>name</b>.
 * Return NULL if no such event is found.
 */
static periodic_event_item_t *
find_periodic_event(const char *name)
{
  int i;
  for (i = 0; periodic_events[i].name; ++i) {
    if (strcmp(name, periodic_events[i].name) == 0)
      return &periodic_events[i];
  }
  return NULL;
}

/** Return a bitmask of the roles this tor instance is configured for using
 * the given options. */
STATIC int
get_my_roles(const or_options_t *options)
{
  tor_assert(options);

  int roles = 0;
  int is_bridge = options->BridgeRelay;
  int is_relay = server_mode(options);
  int is_dirauth = authdir_mode_v3(options);
  int is_bridgeauth = authdir_mode_bridge(options);
  int is_hidden_service = !!hs_service_get_num_services() ||
                          !!rend_num_services();
  int is_dirserver = dir_server_mode(options);
  /* We also consider tor to have the role of a client if the ControlPort is
   * set because a lot of things can be done over the control port which
   * requires tor to have basic functionnalities. */
  int is_client = options_any_client_port_set(options) ||
                  options->ControlPort_set ||
                  options->OwningControllerFD != UINT64_MAX;

  if (is_bridge) roles |= PERIODIC_EVENT_ROLE_BRIDGE;
  if (is_client) roles |= PERIODIC_EVENT_ROLE_CLIENT;
  if (is_relay) roles |= PERIODIC_EVENT_ROLE_RELAY;
  if (is_dirauth) roles |= PERIODIC_EVENT_ROLE_DIRAUTH;
  if (is_bridgeauth) roles |= PERIODIC_EVENT_ROLE_BRIDGEAUTH;
  if (is_hidden_service) roles |= PERIODIC_EVENT_ROLE_HS_SERVICE;
  if (is_dirserver) roles |= PERIODIC_EVENT_ROLE_DIRSERVER;

  return roles;
}

/** Event to run initialize_periodic_events_cb */
static struct event *initialize_periodic_events_event = NULL;

/** Helper, run one second after setup:
 * Initializes all members of periodic_events and starts them running.
 *
 * (We do this one second after setup for backward-compatibility reasons;
 * it might not actually be necessary.) */
static void
initialize_periodic_events_cb(evutil_socket_t fd, short events, void *data)
{
  (void) fd;
  (void) events;
  (void) data;

  tor_event_free(initialize_periodic_events_event);

  rescan_periodic_events(get_options());
}

/** Set up all the members of periodic_events[], and configure them all to be
 * launched from a callback. */
STATIC void
initialize_periodic_events(void)
{
  if (periodic_events_initialized)
    return;

  periodic_events_initialized = 1;

  /* Set up all periodic events. We'll launch them by roles. */
  int i;
  for (i = 0; periodic_events[i].name; ++i) {
    periodic_event_setup(&periodic_events[i]);
  }

#define NAMED_CALLBACK(name) \
  STMT_BEGIN name ## _event = find_periodic_event( #name ); STMT_END

  NAMED_CALLBACK(check_descriptor);
  NAMED_CALLBACK(dirvote);
  NAMED_CALLBACK(fetch_networkstatus);
  NAMED_CALLBACK(launch_descriptor_fetches);
  NAMED_CALLBACK(check_dns_honesty);
  NAMED_CALLBACK(save_state);

  struct timeval one_second = { 1, 0 };
  initialize_periodic_events_event = tor_evtimer_new(
                  tor_libevent_get_base(),
                  initialize_periodic_events_cb, NULL);
  event_add(initialize_periodic_events_event, &one_second);
}

STATIC void
teardown_periodic_events(void)
{
  int i;
  for (i = 0; periodic_events[i].name; ++i) {
    periodic_event_destroy(&periodic_events[i]);
  }
  periodic_events_initialized = 0;
}

/** Do a pass at all our periodic events, disable those we don't need anymore
 * and enable those we need now using the given options. */
void
rescan_periodic_events(const or_options_t *options)
{
  tor_assert(options);

  /* Avoid scanning the event list if we haven't initialized it yet. This is
   * particularly useful for unit tests in order to avoid initializing main
   * loop events everytime. */
  if (!periodic_events_initialized) {
    return;
  }

  int roles = get_my_roles(options);

  for (int i = 0; periodic_events[i].name; ++i) {
    periodic_event_item_t *item = &periodic_events[i];

    int enable = !!(item->roles & roles);

    /* Handle the event flags. */
    if (net_is_disabled() &&
        (item->flags & PERIODIC_EVENT_FLAG_NEED_NET)) {
      enable = 0;
    }

    /* Enable the event if needed. It is safe to enable an event that was
     * already enabled. Same goes for disabling it. */
    if (enable) {
      log_debug(LD_GENERAL, "Launching periodic event %s", item->name);
      periodic_event_enable(item);
    } else {
      log_debug(LD_GENERAL, "Disabling periodic event %s", item->name);
      periodic_event_disable(item);
    }
  }
}

/* We just got new options globally set, see if we need to enabled or disable
 * periodic events. */
void
periodic_events_on_new_options(const or_options_t *options)
{
  /* Only if we've already initialized the events, rescan the list which will
   * enable or disable events depending on our roles. This will be called at
   * bootup and we don't want this function to initialize the events because
   * they aren't set up at this stage. */
  if (periodic_events_initialized) {
    rescan_periodic_events(options);
  }
}

/**
 * Update our schedule so that we'll check whether we need to update our
 * descriptor immediately, rather than after up to CHECK_DESCRIPTOR_INTERVAL
 * seconds.
 */
void
reschedule_descriptor_update_check(void)
{
  if (check_descriptor_event) {
    periodic_event_reschedule(check_descriptor_event);
  }
}

/**
 * Update our schedule so that we'll check whether we need to fetch directory
 * info immediately.
 */
void
reschedule_directory_downloads(void)
{
  tor_assert(fetch_networkstatus_event);
  tor_assert(launch_descriptor_fetches_event);

  periodic_event_reschedule(fetch_networkstatus_event);
  periodic_event_reschedule(launch_descriptor_fetches_event);
}

/** Mainloop callback: clean up circuits, channels, and connections
 * that are pending close. */
static void
postloop_cleanup_cb(mainloop_event_t *ev, void *arg)
{
  (void)ev;
  (void)arg;
  circuit_close_all_marked();
  close_closeable_connections();
  channel_run_cleanup();
  channel_listener_run_cleanup();
}

/** Event to run postloop_cleanup_cb */
static mainloop_event_t *postloop_cleanup_ev=NULL;

/** Schedule a post-loop event to clean up marked channels, connections, and
 * circuits. */
void
mainloop_schedule_postloop_cleanup(void)
{
  if (PREDICT_UNLIKELY(postloop_cleanup_ev == NULL)) {
    // (It's possible that we can get here if we decide to close a connection
    // in the earliest stages of our configuration, before we create events.)
    return;
  }
  mainloop_event_activate(postloop_cleanup_ev);
}

#define LONGEST_TIMER_PERIOD (30 * 86400)
/** Helper: Return the number of seconds between <b>now</b> and <b>next</b>,
 * clipped to the range [1 second, LONGEST_TIMER_PERIOD]. */
static inline int
safe_timer_diff(time_t now, time_t next)
{
  if (next > now) {
    /* There were no computers at signed TIME_MIN (1902 on 32-bit systems),
     * and nothing that could run Tor. It's a bug if 'next' is around then.
     * On 64-bit systems with signed TIME_MIN, TIME_MIN is before the Big
     * Bang. We cannot extrapolate past a singularity, but there was probably
     * nothing that could run Tor then, either.
     **/
    tor_assert(next > TIME_MIN + LONGEST_TIMER_PERIOD);

    if (next - LONGEST_TIMER_PERIOD > now)
      return LONGEST_TIMER_PERIOD;
    return (int)(next - now);
  } else {
    return 1;
  }
}

/** Perform regular maintenance tasks.  This function gets run once per
 * second by second_elapsed_callback().
 */
static void
run_scheduled_events(time_t now)
{
  const or_options_t *options = get_options();

  /* 0. See if we've been asked to shut down and our timeout has
   * expired; or if our bandwidth limits are exhausted and we
   * should hibernate; or if it's time to wake up from hibernation.
   */
  consider_hibernation(now);

  /* Maybe enough time elapsed for us to reconsider a circuit. */
  circuit_upgrade_circuits_from_guard_wait();

  if (options->UseBridges && !net_is_disabled()) {
    /* Note: this check uses net_is_disabled(), not should_delay_dir_fetches()
     * -- the latter is only for fetching consensus-derived directory info. */
    fetch_bridge_descriptors(options, now);
  }

  if (accounting_is_enabled(options)) {
    accounting_run_housekeeping(now);
  }

  /* 3a. Every second, we examine pending circuits and prune the
   *    ones which have been pending for more than a few seconds.
   *    We do this before step 4, so it can try building more if
   *    it's not comfortable with the number of available circuits.
   */
  /* (If our circuit build timeout can ever become lower than a second (which
   * it can't, currently), we should do this more often.) */
  circuit_expire_building();
  circuit_expire_waiting_for_better_guard();

  /* 3b. Also look at pending streams and prune the ones that 'began'
   *     a long time ago but haven't gotten a 'connected' yet.
   *     Do this before step 4, so we can put them back into pending
   *     state to be picked up by the new circuit.
   */
  connection_ap_expire_beginning();

  /* 3c. And expire connections that we've held open for too long.
   */
  connection_expire_held_open();

  /* 4. Every second, we try a new circuit if there are no valid
   *    circuits. Every NewCircuitPeriod seconds, we expire circuits
   *    that became dirty more than MaxCircuitDirtiness seconds ago,
   *    and we make a new circ if there are no clean circuits.
   */
  const int have_dir_info = router_have_minimum_dir_info();
  if (have_dir_info && !net_is_disabled()) {
    circuit_build_needed_circs(now);
  } else {
    circuit_expire_old_circs_as_needed(now);
  }

  /* 5. We do housekeeping for each connection... */
  channel_update_bad_for_new_circs(NULL, 0);
  int i;
  for (i=0;i<smartlist_len(connection_array);i++) {
    run_connection_housekeeping(i, now);
  }

  /* 11b. check pending unconfigured managed proxies */
  if (!net_is_disabled() && pt_proxies_configuration_pending())
    pt_configure_remaining_proxies();
}

/* Periodic callback: rotate the onion keys after the period defined by the
 * "onion-key-rotation-days" consensus parameter, shut down and restart all
 * cpuworkers, and update our descriptor if necessary.
 */
static int
rotate_onion_key_callback(time_t now, const or_options_t *options)
{
  if (server_mode(options)) {
    int onion_key_lifetime = get_onion_key_lifetime();
    time_t rotation_time = get_onion_key_set_at()+onion_key_lifetime;
    if (rotation_time > now) {
      return ONION_KEY_CONSENSUS_CHECK_INTERVAL;
    }

    log_info(LD_GENERAL,"Rotating onion key.");
    rotate_onion_key();
    cpuworkers_rotate_keyinfo();
    if (router_rebuild_descriptor(1)<0) {
      log_info(LD_CONFIG, "Couldn't rebuild router descriptor");
    }
    if (advertised_server_mode() && !net_is_disabled())
      router_upload_dir_desc_to_dirservers(0);
    return ONION_KEY_CONSENSUS_CHECK_INTERVAL;
  }
  return PERIODIC_EVENT_NO_UPDATE;
}

/* Period callback: Check if our old onion keys are still valid after the
 * period of time defined by the consensus parameter
 * "onion-key-grace-period-days", otherwise expire them by setting them to
 * NULL.
 */
static int
check_onion_keys_expiry_time_callback(time_t now, const or_options_t *options)
{
  if (server_mode(options)) {
    int onion_key_grace_period = get_onion_key_grace_period();
    time_t expiry_time = get_onion_key_set_at()+onion_key_grace_period;
    if (expiry_time > now) {
      return ONION_KEY_CONSENSUS_CHECK_INTERVAL;
    }

    log_info(LD_GENERAL, "Expiring old onion keys.");
    expire_old_onion_keys();
    cpuworkers_rotate_keyinfo();
    return ONION_KEY_CONSENSUS_CHECK_INTERVAL;
  }

  return PERIODIC_EVENT_NO_UPDATE;
}

/* Periodic callback: Every 30 seconds, check whether it's time to make new
 * Ed25519 subkeys.
 */
static int
check_ed_keys_callback(time_t now, const or_options_t *options)
{
  if (server_mode(options)) {
    if (should_make_new_ed_keys(options, now)) {
      int new_signing_key = load_ed_keys(options, now);
      if (new_signing_key < 0 ||
          generate_ed_link_cert(options, now, new_signing_key > 0)) {
        log_err(LD_OR, "Unable to update Ed25519 keys!  Exiting.");
        tor_shutdown_event_loop_and_exit(1);
      }
    }
    return 30;
  }
  return PERIODIC_EVENT_NO_UPDATE;
}

/**
 * Periodic callback: Every {LAZY,GREEDY}_DESCRIPTOR_RETRY_INTERVAL,
 * see about fetching descriptors, microdescriptors, and extrainfo
 * documents.
 */
static int
launch_descriptor_fetches_callback(time_t now, const or_options_t *options)
{
  if (should_delay_dir_fetches(options, NULL))
      return PERIODIC_EVENT_NO_UPDATE;

  update_all_descriptor_downloads(now);
  update_extrainfo_downloads(now);
  if (router_have_minimum_dir_info())
    return LAZY_DESCRIPTOR_RETRY_INTERVAL;
  else
    return GREEDY_DESCRIPTOR_RETRY_INTERVAL;
}

/**
 * Periodic event: Rotate our X.509 certificates and TLS keys once every
 * MAX_SSL_KEY_LIFETIME_INTERNAL.
 */
static int
rotate_x509_certificate_callback(time_t now, const or_options_t *options)
{
  static int first = 1;
  (void)now;
  (void)options;
  if (first) {
    first = 0;
    return MAX_SSL_KEY_LIFETIME_INTERNAL;
  }

  /* 1b. Every MAX_SSL_KEY_LIFETIME_INTERNAL seconds, we change our
   * TLS context. */
  log_info(LD_GENERAL,"Rotating tls context.");
  if (router_initialize_tls_context() < 0) {
    log_err(LD_BUG, "Error reinitializing TLS context");
    tor_assert_unreached();
  }
  if (generate_ed_link_cert(options, now, 1)) {
    log_err(LD_OR, "Unable to update Ed25519->TLS link certificate for "
            "new TLS context.");
    tor_assert_unreached();
  }

  /* We also make sure to rotate the TLS connections themselves if they've
   * been up for too long -- but that's done via is_bad_for_new_circs in
   * run_connection_housekeeping() above. */
  return MAX_SSL_KEY_LIFETIME_INTERNAL;
}

/**
 * Periodic callback: once an hour, grab some more entropy from the
 * kernel and feed it to our CSPRNG.
 **/
static int
add_entropy_callback(time_t now, const or_options_t *options)
{
  (void)now;
  (void)options;
  /* We already seeded once, so don't die on failure. */
  if (crypto_seed_rng() < 0) {
    log_warn(LD_GENERAL, "Tried to re-seed RNG, but failed. We already "
             "seeded once, though, so we won't exit here.");
  }

  /** How often do we add more entropy to OpenSSL's RNG pool? */
#define ENTROPY_INTERVAL (60*60)
  return ENTROPY_INTERVAL;
}

/**
 * Periodic callback: if we're an authority, make sure we test
 * the routers on the network for reachability.
 */
static int
launch_reachability_tests_callback(time_t now, const or_options_t *options)
{
  if (authdir_mode_tests_reachability(options) &&
      !net_is_disabled()) {
    /* try to determine reachability of the other Tor relays */
    dirserv_test_reachability(now);
  }
  return REACHABILITY_TEST_INTERVAL;
}

/**
 * Periodic callback: if we're an authority, discount the stability
 * information (and other rephist information) that's older.
 */
static int
downrate_stability_callback(time_t now, const or_options_t *options)
{
  (void)options;
  /* 1d. Periodically, we discount older stability information so that new
   * stability info counts more, and save the stability information to disk as
   * appropriate. */
  time_t next = rep_hist_downrate_old_runs(now);
  return safe_timer_diff(now, next);
}

/**
 * Periodic callback: if we're an authority, record our measured stability
 * information from rephist in an mtbf file.
 */
static int
save_stability_callback(time_t now, const or_options_t *options)
{
  if (authdir_mode_tests_reachability(options)) {
    if (rep_hist_record_mtbf_data(now, 1)<0) {
      log_warn(LD_GENERAL, "Couldn't store mtbf data.");
    }
  }
#define SAVE_STABILITY_INTERVAL (30*60)
  return SAVE_STABILITY_INTERVAL;
}

/**
 * Periodic callback: if we're an authority, check on our authority
 * certificate (the one that authenticates our authority signing key).
 */
static int
check_authority_cert_callback(time_t now, const or_options_t *options)
{
  (void)now;
  (void)options;
  /* 1e. Periodically, if we're a v3 authority, we check whether our cert is
   * close to expiring and warn the admin if it is. */
  v3_authority_check_key_expiry();
#define CHECK_V3_CERTIFICATE_INTERVAL (5*60)
  return CHECK_V3_CERTIFICATE_INTERVAL;
}

/**
 * Scheduled callback: Run directory-authority voting functionality.
 *
 * The schedule is a bit complicated here, so dirvote_act() manages the
 * schedule itself.
 **/
static int
dirvote_callback(time_t now, const or_options_t *options)
{
  if (!authdir_mode_v3(options)) {
    tor_assert_nonfatal_unreached();
    return 3600;
  }

  time_t next = dirvote_act(options, now);
  if (BUG(next == TIME_MAX)) {
    /* This shouldn't be returned unless we called dirvote_act() without
     * being an authority.  If it happens, maybe our configuration will
     * fix itself in an hour or so? */
    return 3600;
  }
  return safe_timer_diff(now, next);
}

/** Reschedule the directory-authority voting event.  Run this whenever the
 * schedule has changed. */
void
reschedule_dirvote(const or_options_t *options)
{
  if (periodic_events_initialized && authdir_mode_v3(options)) {
    periodic_event_reschedule(dirvote_event);
  }
}

/**
 * Periodic callback: If our consensus is too old, recalculate whether
 * we can actually use it.
 */
static int
check_expired_networkstatus_callback(time_t now, const or_options_t *options)
{
  (void)options;
  /* Check whether our networkstatus has expired. */
  networkstatus_t *ns = networkstatus_get_latest_consensus();
  /*XXXX RD: This value needs to be the same as REASONABLY_LIVE_TIME in
   * networkstatus_get_reasonably_live_consensus(), but that value is way
   * way too high.  Arma: is the bridge issue there resolved yet? -NM */
#define NS_EXPIRY_SLOP (24*60*60)
  if (ns && ns->valid_until < (now - NS_EXPIRY_SLOP) &&
      router_have_minimum_dir_info()) {
    router_dir_info_changed();
  }
#define CHECK_EXPIRED_NS_INTERVAL (2*60)
  return CHECK_EXPIRED_NS_INTERVAL;
}

/**
 * Scheduled callback: Save the state file to disk if appropriate.
 */
static int
save_state_callback(time_t now, const or_options_t *options)
{
  (void) options;
  (void) or_state_save(now); // only saves if appropriate
  const time_t next_write = get_or_state()->next_write;
  if (next_write == TIME_MAX) {
    return 86400;
  }
  return safe_timer_diff(now, next_write);
}

/** Reschedule the event for saving the state file.
 *
 * Run this when the state becomes dirty. */
void
reschedule_or_state_save(void)
{
  if (save_state_event == NULL) {
    /* This can happen early on during startup. */
    return;
  }
  periodic_event_reschedule(save_state_event);
}

/**
 * Periodic callback: Write statistics to disk if appropriate.
 */
static int
write_stats_file_callback(time_t now, const or_options_t *options)
{
  /* 1g. Check whether we should write statistics to disk.
   */
#define CHECK_WRITE_STATS_INTERVAL (60*60)
  time_t next_time_to_write_stats_files = now + CHECK_WRITE_STATS_INTERVAL;
  if (options->CellStatistics) {
    time_t next_write =
      rep_hist_buffer_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }
  if (options->DirReqStatistics) {
    time_t next_write = geoip_dirreq_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }
  if (options->EntryStatistics) {
    time_t next_write = geoip_entry_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }
  if (options->HiddenServiceStatistics) {
    time_t next_write = rep_hist_hs_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }
  if (options->ExitPortStatistics) {
    time_t next_write = rep_hist_exit_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }
  if (options->ConnDirectionStatistics) {
    time_t next_write = rep_hist_conn_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }
  if (options->BridgeAuthoritativeDir) {
    time_t next_write = rep_hist_desc_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }

  return safe_timer_diff(now, next_time_to_write_stats_files);
}

#define CHANNEL_CHECK_INTERVAL (60*60)
static int
check_canonical_channels_callback(time_t now, const or_options_t *options)
{
  (void)now;
  if (public_server_mode(options))
    channel_check_for_duplicates();

  return CHANNEL_CHECK_INTERVAL;
}

static int
reset_padding_counts_callback(time_t now, const or_options_t *options)
{
  if (options->PaddingStatistics) {
    rep_hist_prep_published_padding_counts(now);
  }

  rep_hist_reset_padding_counts();
  return REPHIST_CELL_PADDING_COUNTS_INTERVAL;
}

static int should_init_bridge_stats = 1;

/**
 * Periodic callback: Write bridge statistics to disk if appropriate.
 */
static int
record_bridge_stats_callback(time_t now, const or_options_t *options)
{
  /* 1h. Check whether we should write bridge statistics to disk.
   */
  if (should_record_bridge_info(options)) {
    if (should_init_bridge_stats) {
      /* (Re-)initialize bridge statistics. */
        geoip_bridge_stats_init(now);
        should_init_bridge_stats = 0;
        return WRITE_STATS_INTERVAL;
    } else {
      /* Possibly write bridge statistics to disk and ask when to write
       * them next time. */
      time_t next = geoip_bridge_stats_write(now);
      return safe_timer_diff(now, next);
    }
  } else if (!should_init_bridge_stats) {
    /* Bridge mode was turned off. Ensure that stats are re-initialized
     * next time bridge mode is turned on. */
    should_init_bridge_stats = 1;
  }
  return PERIODIC_EVENT_NO_UPDATE;
}

/**
 * Periodic callback: Clean in-memory caches every once in a while
 */
static int
clean_caches_callback(time_t now, const or_options_t *options)
{
  /* Remove old information from rephist and the rend cache. */
  rep_history_clean(now - options->RephistTrackTime);
  rend_cache_clean(now, REND_CACHE_TYPE_SERVICE);
  hs_cache_clean_as_client(now);
  hs_cache_clean_as_dir(now);
  microdesc_cache_rebuild(NULL, 0);
#define CLEAN_CACHES_INTERVAL (30*60)
  return CLEAN_CACHES_INTERVAL;
}

/**
 * Periodic callback: Clean the cache of failed hidden service lookups
 * frequently.
 */
static int
rend_cache_failure_clean_callback(time_t now, const or_options_t *options)
{
  (void)options;
  /* We don't keep entries that are more than five minutes old so we try to
   * clean it as soon as we can since we want to make sure the client waits
   * as little as possible for reachability reasons. */
  rend_cache_failure_clean(now);
  hs_cache_client_intro_state_clean(now);
  return 30;
}

/**
 * Periodic callback: If we're a server and initializing dns failed, retry.
 */
static int
retry_dns_callback(time_t now, const or_options_t *options)
{
  (void)now;
#define RETRY_DNS_INTERVAL (10*60)
  if (server_mode(options) && has_dns_init_failed())
    dns_init();
  return RETRY_DNS_INTERVAL;
}

/** Periodic callback: consider rebuilding or and re-uploading our descriptor
 * (if we've passed our internal checks). */
static int
check_descriptor_callback(time_t now, const or_options_t *options)
{
/** How often do we check whether part of our router info has changed in a
 * way that would require an upload? That includes checking whether our IP
 * address has changed. */
#define CHECK_DESCRIPTOR_INTERVAL (60)

  (void)options;

  /* 2b. Once per minute, regenerate and upload the descriptor if the old
   * one is inaccurate. */
  if (!net_is_disabled()) {
    check_descriptor_bandwidth_changed(now);
    check_descriptor_ipaddress_changed(now);
    mark_my_descriptor_dirty_if_too_old(now);
    consider_publishable_server(0);
    /* If any networkstatus documents are no longer recent, we need to
     * update all the descriptors' running status. */
    /* Remove dead routers. */
    /* XXXX This doesn't belong here, but it was here in the pre-
     * XXXX refactoring code. */
    routerlist_remove_old_routers();
  }

  return CHECK_DESCRIPTOR_INTERVAL;
}

/**
 * Periodic callback: check whether we're reachable (as a relay), and
 * whether our bandwidth has changed enough that we need to
 * publish a new descriptor.
 */
static int
check_for_reachability_bw_callback(time_t now, const or_options_t *options)
{
  /* XXXX This whole thing was stuck in the middle of what is now
   * XXXX check_descriptor_callback.  I'm not sure it's right. */

  static int dirport_reachability_count = 0;
  /* also, check religiously for reachability, if it's within the first
   * 20 minutes of our uptime. */
  if (server_mode(options) &&
      (have_completed_a_circuit() || !any_predicted_circuits(now)) &&
      !net_is_disabled()) {
    if (get_uptime() < TIMEOUT_UNTIL_UNREACHABILITY_COMPLAINT) {
      router_do_reachability_checks(1, dirport_reachability_count==0);
      if (++dirport_reachability_count > 5)
        dirport_reachability_count = 0;
      return 1;
    } else {
      /* If we haven't checked for 12 hours and our bandwidth estimate is
       * low, do another bandwidth test. This is especially important for
       * bridges, since they might go long periods without much use. */
      const routerinfo_t *me = router_get_my_routerinfo();
      static int first_time = 1;
      if (!first_time && me &&
          me->bandwidthcapacity < me->bandwidthrate &&
          me->bandwidthcapacity < 51200) {
        reset_bandwidth_test();
      }
      first_time = 0;
#define BANDWIDTH_RECHECK_INTERVAL (12*60*60)
      return BANDWIDTH_RECHECK_INTERVAL;
    }
  }
  return CHECK_DESCRIPTOR_INTERVAL;
}

/**
 * Periodic event: once a minute, (or every second if TestingTorNetwork, or
 * during client bootstrap), check whether we want to download any
 * networkstatus documents. */
static int
fetch_networkstatus_callback(time_t now, const or_options_t *options)
{
  /* How often do we check whether we should download network status
   * documents? */
  const int we_are_bootstrapping = networkstatus_consensus_is_bootstrapping(
                                                                        now);
  const int prefer_mirrors = !directory_fetches_from_authorities(
                                                              get_options());
  int networkstatus_dl_check_interval = 60;
  /* check more often when testing, or when bootstrapping from mirrors
   * (connection limits prevent too many connections being made) */
  if (options->TestingTorNetwork
      || (we_are_bootstrapping && prefer_mirrors)) {
    networkstatus_dl_check_interval = 1;
  }

  if (should_delay_dir_fetches(options, NULL))
    return PERIODIC_EVENT_NO_UPDATE;

  update_networkstatus_downloads(now);
  return networkstatus_dl_check_interval;
}

/**
 * Periodic callback: Every 60 seconds, we relaunch listeners if any died. */
static int
retry_listeners_callback(time_t now, const or_options_t *options)
{
  (void)now;
  (void)options;
  if (!net_is_disabled()) {
    retry_all_listeners(NULL, 0);
    return 60;
  }
  return PERIODIC_EVENT_NO_UPDATE;
}

/**
 * Periodic callback: as a server, see if we have any old unused circuits
 * that should be expired */
static int
expire_old_ciruits_serverside_callback(time_t now, const or_options_t *options)
{
  (void)options;
  /* every 11 seconds, so not usually the same second as other such events */
  circuit_expire_old_circuits_serverside(now);
  return 11;
}

/**
 * Callback: Send warnings if Tor doesn't find its ports reachable.
 */
static int
reachability_warnings_callback(time_t now, const or_options_t *options)
{
  (void) now;

  if (get_uptime() < TIMEOUT_UNTIL_UNREACHABILITY_COMPLAINT) {
    return (int)(TIMEOUT_UNTIL_UNREACHABILITY_COMPLAINT - get_uptime());
  }

  if (server_mode(options) &&
      !net_is_disabled() &&
      have_completed_a_circuit()) {
    /* every 20 minutes, check and complain if necessary */
    const routerinfo_t *me = router_get_my_routerinfo();
    if (me && !check_whether_orport_reachable(options)) {
      char *address = tor_dup_ip(me->addr);
      log_warn(LD_CONFIG,"Your server (%s:%d) has not managed to confirm that "
               "its ORPort is reachable. Relays do not publish descriptors "
               "until their ORPort and DirPort are reachable. Please check "
               "your firewalls, ports, address, /etc/hosts file, etc.",
               address, me->or_port);
      control_event_server_status(LOG_WARN,
                                  "REACHABILITY_FAILED ORADDRESS=%s:%d",
                                  address, me->or_port);
      tor_free(address);
    }

    if (me && !check_whether_dirport_reachable(options)) {
      char *address = tor_dup_ip(me->addr);
      log_warn(LD_CONFIG,
               "Your server (%s:%d) has not managed to confirm that its "
               "DirPort is reachable. Relays do not publish descriptors "
               "until their ORPort and DirPort are reachable. Please check "
               "your firewalls, ports, address, /etc/hosts file, etc.",
               address, me->dir_port);
      control_event_server_status(LOG_WARN,
                                  "REACHABILITY_FAILED DIRADDRESS=%s:%d",
                                  address, me->dir_port);
      tor_free(address);
    }
  }

  return TIMEOUT_UNTIL_UNREACHABILITY_COMPLAINT;
}

static int dns_honesty_first_time = 1;

/**
 * Periodic event: if we're an exit, see if our DNS server is telling us
 * obvious lies.
 */
static int
check_dns_honesty_callback(time_t now, const or_options_t *options)
{
  (void)now;
  /* 9. and if we're an exit node, check whether our DNS is telling stories
   * to us. */
  if (net_is_disabled() ||
      ! public_server_mode(options) ||
      router_my_exit_policy_is_reject_star())
    return PERIODIC_EVENT_NO_UPDATE;

  if (dns_honesty_first_time) {
    /* Don't launch right when we start */
    dns_honesty_first_time = 0;
    return crypto_rand_int_range(60, 180);
  }

  dns_launch_correctness_checks();
  return 12*3600 + crypto_rand_int(12*3600);
}

/**
 * Periodic callback: if we're the bridge authority, write a networkstatus
 * file to disk.
 */
static int
write_bridge_ns_callback(time_t now, const or_options_t *options)
{
  /* 10. write bridge networkstatus file to disk */
  if (options->BridgeAuthoritativeDir) {
    networkstatus_dump_bridge_status_to_file(now);
#define BRIDGE_STATUSFILE_INTERVAL (30*60)
     return BRIDGE_STATUSFILE_INTERVAL;
  }
  return PERIODIC_EVENT_NO_UPDATE;
}

static int heartbeat_callback_first_time = 1;

/**
 * Periodic callback: write the heartbeat message in the logs.
 *
 * If writing the heartbeat message to the logs fails for some reason, retry
 * again after <b>MIN_HEARTBEAT_PERIOD</b> seconds.
 */
static int
heartbeat_callback(time_t now, const or_options_t *options)
{
  /* Check if heartbeat is disabled */
  if (!options->HeartbeatPeriod) {
    return PERIODIC_EVENT_NO_UPDATE;
  }

  /* Skip the first one. */
  if (heartbeat_callback_first_time) {
    heartbeat_callback_first_time = 0;
    return options->HeartbeatPeriod;
  }

  /* Write the heartbeat message */
  if (log_heartbeat(now) == 0) {
    return options->HeartbeatPeriod;
  } else {
    /* If we couldn't write the heartbeat log message, try again in the minimum
     * interval of time. */
    return MIN_HEARTBEAT_PERIOD;
  }
}

#define CDM_CLEAN_CALLBACK_INTERVAL 600
static int
clean_consdiffmgr_callback(time_t now, const or_options_t *options)
{
  (void)now;
  if (dir_server_mode(options)) {
    consdiffmgr_cleanup();
  }
  return CDM_CLEAN_CALLBACK_INTERVAL;
}

/*
 * Periodic callback: Run scheduled events for HS service. This is called
 * every second.
 */
static int
hs_service_callback(time_t now, const or_options_t *options)
{
  (void) options;

  /* We need to at least be able to build circuits and that we actually have
   * a working network. */
  if (!have_completed_a_circuit() || net_is_disabled() ||
      networkstatus_get_live_consensus(now) == NULL) {
    goto end;
  }

  hs_service_run_scheduled_events(now);

 end:
  /* Every 1 second. */
  return 1;
}

/** Timer: used to invoke second_elapsed_callback() once per second. */
static periodic_timer_t *second_timer = NULL;

/**
 * Enable or disable the per-second timer as appropriate, creating it if
 * necessary.
 */
void
reschedule_per_second_timer(void)
{
  struct timeval one_second;
  one_second.tv_sec = 1;
  one_second.tv_usec = 0;

  if (! second_timer) {
    second_timer = periodic_timer_new(tor_libevent_get_base(),
                                      &one_second,
                                      second_elapsed_callback,
                                      NULL);
    tor_assert(second_timer);
  }

  const bool run_per_second_events =
    control_any_per_second_event_enabled() || ! net_is_completely_disabled();

  if (run_per_second_events) {
    periodic_timer_launch(second_timer, &one_second);
  } else {
    periodic_timer_disable(second_timer);
  }
}

/** Last time that update_current_time was called. */
static time_t current_second = 0;
/** Last time that update_current_time updated current_second. */
static monotime_coarse_t current_second_last_changed;

/**
 * Set the current time to "now", which should be the value returned by
 * time().  Check for clock jumps and track the total number of seconds we
 * have been running.
 */
void
update_current_time(time_t now)
{
  if (PREDICT_LIKELY(now == current_second)) {
    /* We call this function a lot.  Most frequently, the current second
     * will not have changed, so we just return. */
    return;
  }

  const time_t seconds_elapsed = current_second ? (now - current_second) : 0;

  /* Check the wall clock against the monotonic clock, so we can
   * better tell idleness from clock jumps and/or other shenanigans. */
  monotime_coarse_t last_updated;
  memcpy(&last_updated, &current_second_last_changed, sizeof(last_updated));
  monotime_coarse_get(&current_second_last_changed);

  /** How much clock jumping do we tolerate? */
#define NUM_JUMPED_SECONDS_BEFORE_WARN 100

  /** How much idleness do we tolerate? */
#define NUM_IDLE_SECONDS_BEFORE_WARN 3600

  if (seconds_elapsed < -NUM_JUMPED_SECONDS_BEFORE_WARN) {
    // moving back in time is always a bad sign.
    circuit_note_clock_jumped(seconds_elapsed, false);
  } else if (seconds_elapsed >= NUM_JUMPED_SECONDS_BEFORE_WARN) {
    /* Compare the monotonic clock to the result of time(). */
    const int32_t monotime_msec_passed =
      monotime_coarse_diff_msec32(&last_updated,
                                  &current_second_last_changed);
    const int monotime_sec_passed = monotime_msec_passed / 1000;
    const int discrepancy = monotime_sec_passed - (int)seconds_elapsed;
    /* If the monotonic clock deviates from time(NULL), we have a couple of
     * possibilities.  On some systems, this means we have been suspended or
     * sleeping.  Everywhere, it can mean that the wall-clock time has
     * been changed -- for example, with settimeofday().
     *
     * On the other hand, if the monotonic time matches with the wall-clock
     * time, we've probably just been idle for a while, with no events firing.
     * we tolerate much more of that.
     */
    const bool clock_jumped = abs(discrepancy) > 2;

    if (clock_jumped || seconds_elapsed >= NUM_IDLE_SECONDS_BEFORE_WARN) {
      circuit_note_clock_jumped(seconds_elapsed, ! clock_jumped);
    }
  } else if (seconds_elapsed > 0) {
    stats_n_seconds_working += seconds_elapsed;
  }

  update_approx_time(now);
  current_second = now;
}

/** Libevent callback: invoked once every second. */
static void
second_elapsed_callback(periodic_timer_t *timer, void *arg)
{
  /* XXXX This could be sensibly refactored into multiple callbacks, and we
   * could use Libevent's timers for this rather than checking the current
   * time against a bunch of timeouts every second. */
  time_t now;
  (void)timer;
  (void)arg;

  now = time(NULL);

  /* We don't need to do this once-per-second any more: time-updating is
   * only in this callback _because it is a callback_. It should be fine
   * to disable this callback, and the time will still get updated.
   */
  update_current_time(now);

  /* Maybe some controller events are ready to fire */
  control_per_second_events();

  run_scheduled_events(now);
}

#ifdef HAVE_SYSTEMD_209
static periodic_timer_t *systemd_watchdog_timer = NULL;

/** Libevent callback: invoked to reset systemd watchdog. */
static void
systemd_watchdog_callback(periodic_timer_t *timer, void *arg)
{
  (void)timer;
  (void)arg;
  sd_notify(0, "WATCHDOG=1");
}
#endif /* defined(HAVE_SYSTEMD_209) */

#define UPTIME_CUTOFF_FOR_NEW_BANDWIDTH_TEST (6*60*60)

/** Called when our IP address seems to have changed. <b>at_interface</b>
 * should be true if we detected a change in our interface, and false if we
 * detected a change in our published address. */
void
ip_address_changed(int at_interface)
{
  const or_options_t *options = get_options();
  int server = server_mode(options);
  int exit_reject_interfaces = (server && options->ExitRelay
                                && options->ExitPolicyRejectLocalInterfaces);

  if (at_interface) {
    if (! server) {
      /* Okay, change our keys. */
      if (init_keys_client() < 0)
        log_warn(LD_GENERAL, "Unable to rotate keys after IP change!");
    }
  } else {
    if (server) {
      if (get_uptime() > UPTIME_CUTOFF_FOR_NEW_BANDWIDTH_TEST)
        reset_bandwidth_test();
      reset_uptime();
      router_reset_reachability();
    }
  }

  /* Exit relays incorporate interface addresses in their exit policies when
   * ExitPolicyRejectLocalInterfaces is set */
  if (exit_reject_interfaces || (server && !at_interface)) {
    mark_my_descriptor_dirty("IP address changed");
  }

  dns_servers_relaunch_checks();
}

/** Forget what we've learned about the correctness of our DNS servers, and
 * start learning again. */
void
dns_servers_relaunch_checks(void)
{
  if (server_mode(get_options())) {
    dns_reset_correctness_checks();
    if (periodic_events_initialized) {
      tor_assert(check_dns_honesty_event);
      periodic_event_reschedule(check_dns_honesty_event);
    }
  }
}

/** Initialize some mainloop_event_t objects that we require. */
void
initialize_mainloop_events(void)
{
  initialize_periodic_events();

  if (!schedule_active_linked_connections_event) {
    schedule_active_linked_connections_event =
      mainloop_event_postloop_new(schedule_active_linked_connections_cb, NULL);
  }
  if (!postloop_cleanup_ev) {
    postloop_cleanup_ev =
      mainloop_event_postloop_new(postloop_cleanup_cb, NULL);
  }
}

/** Tor main loop. */
int
do_main_loop(void)
{
  /* initialize the periodic events first, so that code that depends on the
   * events being present does not assert.
   */
  initialize_periodic_events();
  initialize_mainloop_events();

  /* set up once-a-second callback. */
  reschedule_per_second_timer();

#ifdef HAVE_SYSTEMD_209
  uint64_t watchdog_delay;
  /* set up systemd watchdog notification. */
  if (sd_watchdog_enabled(1, &watchdog_delay) > 0) {
    if (! systemd_watchdog_timer) {
      struct timeval watchdog;
      /* The manager will "act on" us if we don't send them a notification
       * every 'watchdog_delay' microseconds.  So, send notifications twice
       * that often.  */
      watchdog_delay /= 2;
      watchdog.tv_sec = watchdog_delay  / 1000000;
      watchdog.tv_usec = watchdog_delay % 1000000;

      systemd_watchdog_timer = periodic_timer_new(tor_libevent_get_base(),
                                                  &watchdog,
                                                  systemd_watchdog_callback,
                                                  NULL);
      tor_assert(systemd_watchdog_timer);
    }
  }
#endif /* defined(HAVE_SYSTEMD_209) */

  main_loop_should_exit = 0;
  main_loop_exit_value = 0;

#ifdef ENABLE_RESTART_DEBUGGING
  {
    static int first_time = 1;

    if (first_time && getenv("TOR_DEBUG_RESTART")) {
      first_time = 0;
      const char *sec_str = getenv("TOR_DEBUG_RESTART_AFTER_SECONDS");
      long sec;
      int sec_ok=0;
      if (sec_str &&
          (sec = tor_parse_long(sec_str, 10, 0, INT_MAX, &sec_ok, NULL)) &&
          sec_ok) {
        /* Okay, we parsed the seconds. */
      } else {
        sec = 5;
      }
      struct timeval restart_after = { (time_t) sec, 0 };
      tor_shutdown_event_loop_for_restart_event =
        tor_evtimer_new(tor_libevent_get_base(),
                        tor_shutdown_event_loop_for_restart_cb, NULL);
      event_add(tor_shutdown_event_loop_for_restart_event, &restart_after);
    }
  }
#endif

  return run_main_loop_until_done();
}

#ifndef _WIN32
/** Rate-limiter for EINVAL-type libevent warnings. */
static ratelim_t libevent_error_ratelim = RATELIM_INIT(10);
#endif

/**
 * Run the main loop a single time. Return 0 for "exit"; -1 for "exit with
 * error", and 1 for "run this again."
 */
static int
run_main_loop_once(void)
{
  int loop_result;

  if (nt_service_is_stopping())
    return 0;

  if (main_loop_should_exit)
    return 0;

#ifndef _WIN32
  /* Make it easier to tell whether libevent failure is our fault or not. */
  errno = 0;
#endif

  if (get_options()->MainloopStats) {
    /* We always enforce that EVLOOP_ONCE is passed to event_base_loop() if we
     * are collecting main loop statistics. */
    called_loop_once = 1;
  } else {
    called_loop_once = 0;
  }

  /* Make sure we know (about) what time it is. */
  update_approx_time(time(NULL));

  /* Here it is: the main loop.  Here we tell Libevent to poll until we have
   * an event, or the second ends, or until we have some active linked
   * connections to trigger events for.  Libevent will wait till one
   * of these happens, then run all the appropriate callbacks. */
  loop_result = tor_libevent_run_event_loop(tor_libevent_get_base(),
                                            called_loop_once);

  if (get_options()->MainloopStats) {
    /* Update our main loop counters. */
    if (loop_result == 0) {
      // The call was successful.
      increment_main_loop_success_count();
    } else if (loop_result == -1) {
      // The call was erroneous.
      increment_main_loop_error_count();
    } else if (loop_result == 1) {
      // The call didn't have any active or pending events
      // to handle.
      increment_main_loop_idle_count();
    }
  }

  /* Oh, the loop failed.  That might be an error that we need to
   * catch, but more likely, it's just an interrupted poll() call or something,
   * and we should try again. */
  if (loop_result < 0) {
    int e = tor_socket_errno(-1);
    /* let the program survive things like ^z */
    if (e != EINTR && !ERRNO_IS_EINPROGRESS(e)) {
      log_err(LD_NET,"libevent call with %s failed: %s [%d]",
              tor_libevent_get_method(), tor_socket_strerror(e), e);
      return -1;
#ifndef _WIN32
    } else if (e == EINVAL) {
      log_fn_ratelim(&libevent_error_ratelim, LOG_WARN, LD_NET,
                     "EINVAL from libevent: should you upgrade libevent?");
      if (libevent_error_ratelim.n_calls_since_last_time > 8) {
        log_err(LD_NET, "Too many libevent errors, too fast: dying");
        return -1;
      }
#endif /* !defined(_WIN32) */
    } else {
      tor_assert_nonfatal_once(! ERRNO_IS_EINPROGRESS(e));
      log_debug(LD_NET,"libevent call interrupted.");
      /* You can't trust the results of this poll(). Go back to the
       * top of the big for loop. */
      return 1;
    }
  }

  if (main_loop_should_exit)
    return 0;

  return 1;
}

/** Run the run_main_loop_once() function until it declares itself done,
 * and return its final return value.
 *
 * Shadow won't invoke this function, so don't fill it up with things.
 */
static int
run_main_loop_until_done(void)
{
  int loop_result = 1;
  do {
    loop_result = run_main_loop_once();
  } while (loop_result == 1);

  if (main_loop_should_exit)
    return main_loop_exit_value;
  else
    return loop_result;
}

/** Returns Tor's uptime. */
MOCK_IMPL(long,
get_uptime,(void))
{
  return stats_n_seconds_working;
}

/** Reset Tor's uptime. */
MOCK_IMPL(void,
reset_uptime,(void))
{
  stats_n_seconds_working = 0;
}

void
tor_mainloop_free_all(void)
{
  smartlist_free(connection_array);
  smartlist_free(closeable_connection_lst);
  smartlist_free(active_linked_connection_lst);
  periodic_timer_free(second_timer);
  teardown_periodic_events();
  tor_event_free(shutdown_did_not_work_event);
  tor_event_free(initialize_periodic_events_event);
  mainloop_event_free(directory_all_unreachable_cb_event);
  mainloop_event_free(schedule_active_linked_connections_event);
  mainloop_event_free(postloop_cleanup_ev);
  mainloop_event_free(handle_deferred_signewnym_ev);

#ifdef HAVE_SYSTEMD_209
  periodic_timer_free(systemd_watchdog_timer);
#endif

  stats_n_bytes_read = stats_n_bytes_written = 0;

  memset(&global_bucket, 0, sizeof(global_bucket));
  memset(&global_relayed_bucket, 0, sizeof(global_relayed_bucket));
  time_of_process_start = 0;
  time_of_last_signewnym = 0;
  signewnym_is_pending = 0;
  newnym_epoch = 0;
  called_loop_once = 0;
  main_loop_should_exit = 0;
  main_loop_exit_value = 0;
  can_complete_circuits = 0;
  quiet_level = 0;
  should_init_bridge_stats = 1;
  dns_honesty_first_time = 1;
  heartbeat_callback_first_time = 1;
  current_second = 0;
  memset(&current_second_last_changed, 0,
         sizeof(current_second_last_changed));
}
