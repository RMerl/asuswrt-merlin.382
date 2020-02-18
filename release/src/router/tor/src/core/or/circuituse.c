/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file circuituse.c
 * \brief Launch the right sort of circuits and attach the right streams to
 * them.
 *
 * As distinct from circuitlist.c, which manages lookups to find circuits, and
 * circuitbuild.c, which handles the logistics of circuit construction, this
 * module keeps track of which streams can be attached to which circuits (in
 * circuit_get_best()), and attaches streams to circuits (with
 * circuit_try_attaching_streams(), connection_ap_handshake_attach_circuit(),
 * and connection_ap_handshake_attach_chosen_circuit() ).
 *
 * This module also makes sure that we are building circuits for all of the
 * predicted ports, using circuit_remove_handled_ports(),
 * circuit_stream_is_being_handled(), and circuit_build_needed_cirs().  It
 * handles launching circuits for specific targets using
 * circuit_launch_by_extend_info().
 *
 * This is also where we handle expiring circuits that have been around for
 * too long without actually completing, along with the circuit_build_timeout
 * logic in circuitstats.c.
 **/

#include "core/or/or.h"
#include "app/config/config.h"
#include "core/mainloop/connection.h"
#include "core/or/channel.h"
#include "core/or/circuitbuild.h"
#include "core/or/circuitlist.h"
#include "core/or/circuitstats.h"
#include "core/or/circuituse.h"
#include "core/or/connection_edge.h"
#include "core/or/policies.h"
#include "feature/client/addressmap.h"
#include "feature/client/bridges.h"
#include "feature/client/circpathbias.h"
#include "feature/client/entrynodes.h"
#include "feature/control/control.h"
#include "feature/dircommon/directory.h"
#include "feature/hs/hs_circuit.h"
#include "feature/hs/hs_client.h"
#include "feature/hs/hs_common.h"
#include "feature/hs/hs_ident.h"
#include "feature/hs/hs_stats.h"
#include "feature/nodelist/describe.h"
#include "feature/nodelist/networkstatus.h"
#include "feature/nodelist/nodelist.h"
#include "feature/nodelist/routerlist.h"
#include "feature/relay/routermode.h"
#include "feature/relay/selftest.h"
#include "feature/rend/rendclient.h"
#include "feature/rend/rendcommon.h"
#include "feature/rend/rendservice.h"
#include "feature/stats/predict_ports.h"
#include "lib/math/fp.h"
#include "lib/time/tvdiff.h"

#include "core/or/cpath_build_state_st.h"
#include "feature/dircommon/dir_connection_st.h"
#include "core/or/entry_connection_st.h"
#include "core/or/extend_info_st.h"
#include "core/or/or_circuit_st.h"
#include "core/or/origin_circuit_st.h"
#include "core/or/socks_request_st.h"

static void circuit_expire_old_circuits_clientside(void);
static void circuit_increment_failure_count(void);

/** Check whether the hidden service destination of the stream at
 *  <b>edge_conn</b> is the same as the destination of the circuit at
 *  <b>origin_circ</b>. */
static int
circuit_matches_with_rend_stream(const edge_connection_t *edge_conn,
                                 const origin_circuit_t *origin_circ)
{
  /* Check if this is a v2 rendezvous circ/stream */
  if ((edge_conn->rend_data && !origin_circ->rend_data) ||
      (!edge_conn->rend_data && origin_circ->rend_data) ||
      (edge_conn->rend_data && origin_circ->rend_data &&
       rend_cmp_service_ids(rend_data_get_address(edge_conn->rend_data),
                            rend_data_get_address(origin_circ->rend_data)))) {
    /* this circ is not for this conn */
    return 0;
  }

  /* Check if this is a v3 rendezvous circ/stream */
  if ((edge_conn->hs_ident && !origin_circ->hs_ident) ||
      (!edge_conn->hs_ident && origin_circ->hs_ident) ||
      (edge_conn->hs_ident && origin_circ->hs_ident &&
       !ed25519_pubkey_eq(&edge_conn->hs_ident->identity_pk,
                          &origin_circ->hs_ident->identity_pk))) {
    /* this circ is not for this conn */
    return 0;
  }

  return 1;
}

/** Return 1 if <b>circ</b> could be returned by circuit_get_best().
 * Else return 0.
 */
static int
circuit_is_acceptable(const origin_circuit_t *origin_circ,
                      const entry_connection_t *conn,
                      int must_be_open, uint8_t purpose,
                      int need_uptime, int need_internal,
                      time_t now)
{
  const circuit_t *circ = TO_CIRCUIT(origin_circ);
  const node_t *exitnode;
  cpath_build_state_t *build_state;
  tor_assert(circ);
  tor_assert(conn);
  tor_assert(conn->socks_request);

  if (must_be_open && (circ->state != CIRCUIT_STATE_OPEN || !circ->n_chan))
    return 0; /* ignore non-open circs */
  if (circ->marked_for_close)
    return 0;

  /* if this circ isn't our purpose, skip. */
  if (purpose == CIRCUIT_PURPOSE_C_REND_JOINED && !must_be_open) {
    if (circ->purpose != CIRCUIT_PURPOSE_C_ESTABLISH_REND &&
        circ->purpose != CIRCUIT_PURPOSE_C_REND_READY &&
        circ->purpose != CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED &&
        circ->purpose != CIRCUIT_PURPOSE_C_REND_JOINED)
      return 0;
  } else if (purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT &&
             !must_be_open) {
    if (circ->purpose != CIRCUIT_PURPOSE_C_INTRODUCING &&
        circ->purpose != CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT)
      return 0;
  } else {
    if (purpose != circ->purpose)
      return 0;
  }

  /* If this is a timed-out hidden service circuit, skip it. */
  if (origin_circ->hs_circ_has_timed_out) {
    return 0;
  }

  if (purpose == CIRCUIT_PURPOSE_C_GENERAL ||
      purpose == CIRCUIT_PURPOSE_C_HSDIR_GET ||
      purpose == CIRCUIT_PURPOSE_S_HSDIR_POST ||
      purpose == CIRCUIT_PURPOSE_HS_VANGUARDS ||
      purpose == CIRCUIT_PURPOSE_C_REND_JOINED) {
    if (circ->timestamp_dirty &&
       circ->timestamp_dirty+get_options()->MaxCircuitDirtiness <= now)
      return 0;
  }

  if (origin_circ->unusable_for_new_conns)
    return 0;

  /* decide if this circ is suitable for this conn */

  /* for rend circs, circ->cpath->prev is not the last router in the
   * circuit, it's the magical extra service hop. so just check the nickname
   * of the one we meant to finish at.
   */
  build_state = origin_circ->build_state;
  exitnode = build_state_get_exit_node(build_state);

  if (need_uptime && !build_state->need_uptime)
    return 0;
  if (need_internal != build_state->is_internal)
    return 0;

  if (purpose == CIRCUIT_PURPOSE_C_GENERAL ||
      purpose == CIRCUIT_PURPOSE_S_HSDIR_POST ||
      purpose == CIRCUIT_PURPOSE_C_HSDIR_GET) {
    tor_addr_t addr;
    const int family = tor_addr_parse(&addr, conn->socks_request->address);
    if (!exitnode && !build_state->onehop_tunnel) {
      log_debug(LD_CIRC,"Not considering circuit with unknown router.");
      return 0; /* this circuit is screwed and doesn't know it yet,
                 * or is a rendezvous circuit. */
    }
    if (build_state->onehop_tunnel) {
      if (!conn->want_onehop) {
        log_debug(LD_CIRC,"Skipping one-hop circuit.");
        return 0;
      }
      tor_assert(conn->chosen_exit_name);
      if (build_state->chosen_exit) {
        char digest[DIGEST_LEN];
        if (hexdigest_to_digest(conn->chosen_exit_name, digest) < 0)
          return 0; /* broken digest, we don't want it */
        if (tor_memneq(digest, build_state->chosen_exit->identity_digest,
                          DIGEST_LEN))
          return 0; /* this is a circuit to somewhere else */
        if (tor_digest_is_zero(digest)) {
          /* we don't know the digest; have to compare addr:port */
          if (family < 0 ||
              !tor_addr_eq(&build_state->chosen_exit->addr, &addr) ||
              build_state->chosen_exit->port != conn->socks_request->port)
            return 0;
        }
      }
    } else {
      if (conn->want_onehop) {
        /* don't use three-hop circuits -- that could hurt our anonymity. */
        return 0;
      }
    }
    if (origin_circ->prepend_policy && family != -1) {
      int r = compare_tor_addr_to_addr_policy(&addr,
                                              conn->socks_request->port,
                                              origin_circ->prepend_policy);
      if (r == ADDR_POLICY_REJECTED)
        return 0;
    }
    if (exitnode && !connection_ap_can_use_exit(conn, exitnode)) {
      /* can't exit from this router */
      return 0;
    }
  } else { /* not general: this might be a rend circuit */
    const edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(conn);
    if (!circuit_matches_with_rend_stream(edge_conn, origin_circ)) {
      return 0;
    }
  }

  if (!connection_edge_compatible_with_circuit(conn, origin_circ)) {
    /* conn needs to be isolated from other conns that have already used
     * origin_circ */
    return 0;
  }

  return 1;
}

/** Return 1 if circuit <b>a</b> is better than circuit <b>b</b> for
 * <b>conn</b>, and return 0 otherwise. Used by circuit_get_best.
 */
static int
circuit_is_better(const origin_circuit_t *oa, const origin_circuit_t *ob,
                  const entry_connection_t *conn)
{
  const circuit_t *a = TO_CIRCUIT(oa);
  const circuit_t *b = TO_CIRCUIT(ob);
  const uint8_t purpose = ENTRY_TO_CONN(conn)->purpose;
  int a_bits, b_bits;

  /* If one of the circuits was allowed to live due to relaxing its timeout,
   * it is definitely worse (it's probably a much slower path). */
  if (oa->relaxed_timeout && !ob->relaxed_timeout)
    return 0; /* ob is better. It's not relaxed. */
  if (!oa->relaxed_timeout && ob->relaxed_timeout)
    return 1; /* oa is better. It's not relaxed. */

  switch (purpose) {
    case CIRCUIT_PURPOSE_S_HSDIR_POST:
    case CIRCUIT_PURPOSE_C_HSDIR_GET:
    case CIRCUIT_PURPOSE_C_GENERAL:
      /* if it's used but less dirty it's best;
       * else if it's more recently created it's best
       */
      if (b->timestamp_dirty) {
        if (a->timestamp_dirty &&
            a->timestamp_dirty > b->timestamp_dirty)
          return 1;
      } else {
        if (a->timestamp_dirty ||
            timercmp(&a->timestamp_began, &b->timestamp_began, OP_GT))
          return 1;
        if (ob->build_state->is_internal)
          /* XXXX++ what the heck is this internal thing doing here. I
           * think we can get rid of it. circuit_is_acceptable() already
           * makes sure that is_internal is exactly what we need it to
           * be. -RD */
          return 1;
      }
      break;
    case CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT:
      /* the closer it is to ack_wait the better it is */
      if (a->purpose > b->purpose)
        return 1;
      break;
    case CIRCUIT_PURPOSE_C_REND_JOINED:
      /* the closer it is to rend_joined the better it is */
      if (a->purpose > b->purpose)
        return 1;
      break;
  }

  /* XXXX Maybe this check should get a higher priority to avoid
   *   using up circuits too rapidly. */

  a_bits = connection_edge_update_circuit_isolation(conn,
                                                    (origin_circuit_t*)oa, 1);
  b_bits = connection_edge_update_circuit_isolation(conn,
                                                    (origin_circuit_t*)ob, 1);
  /* if x_bits < 0, then we have not used x for anything; better not to dirty
   * a connection if we can help it. */
  if (a_bits < 0) {
    return 0;
  } else if (b_bits < 0) {
    return 1;
  }
  a_bits &= ~ oa->isolation_flags_mixed;
  a_bits &= ~ ob->isolation_flags_mixed;
  if (n_bits_set_u8(a_bits) < n_bits_set_u8(b_bits)) {
    /* The fewer new restrictions we need to make on a circuit for stream
     * isolation, the better. */
    return 1;
  }

  return 0;
}

/** Find the best circ that conn can use, preferably one which is
 * dirty. Circ must not be too old.
 *
 * Conn must be defined.
 *
 * If must_be_open, ignore circs not in CIRCUIT_STATE_OPEN.
 *
 * circ_purpose specifies what sort of circuit we must have.
 * It can be C_GENERAL, C_INTRODUCE_ACK_WAIT, or C_REND_JOINED.
 *
 * If it's REND_JOINED and must_be_open==0, then return the closest
 * rendezvous-purposed circuit that you can find.
 *
 * If it's INTRODUCE_ACK_WAIT and must_be_open==0, then return the
 * closest introduce-purposed circuit that you can find.
 */
static origin_circuit_t *
circuit_get_best(const entry_connection_t *conn,
                 int must_be_open, uint8_t purpose,
                 int need_uptime, int need_internal)
{
  origin_circuit_t *best=NULL;
  struct timeval now;
  int intro_going_on_but_too_old = 0;

  tor_assert(conn);

  tor_assert(purpose == CIRCUIT_PURPOSE_C_GENERAL ||
             purpose == CIRCUIT_PURPOSE_HS_VANGUARDS ||
             purpose == CIRCUIT_PURPOSE_C_HSDIR_GET ||
             purpose == CIRCUIT_PURPOSE_S_HSDIR_POST ||
             purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT ||
             purpose == CIRCUIT_PURPOSE_C_REND_JOINED);

  tor_gettimeofday(&now);

  SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *, circ) {
    origin_circuit_t *origin_circ;
    if (!CIRCUIT_IS_ORIGIN(circ))
      continue;
    origin_circ = TO_ORIGIN_CIRCUIT(circ);

    /* Log an info message if we're going to launch a new intro circ in
     * parallel */
    if (purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT &&
        !must_be_open && origin_circ->hs_circ_has_timed_out &&
        !circ->marked_for_close) {
        intro_going_on_but_too_old = 1;
        continue;
    }

    if (!circuit_is_acceptable(origin_circ,conn,must_be_open,purpose,
                               need_uptime,need_internal, (time_t)now.tv_sec))
      continue;

    /* now this is an acceptable circ to hand back. but that doesn't
     * mean it's the *best* circ to hand back. try to decide.
     */
    if (!best || circuit_is_better(origin_circ,best,conn))
      best = origin_circ;
  }
  SMARTLIST_FOREACH_END(circ);

  if (!best && intro_going_on_but_too_old)
    log_info(LD_REND|LD_CIRC, "There is an intro circuit being created "
             "right now, but it has already taken quite a while. Starting "
             "one in parallel.");

  return best;
}

/** Return the number of not-yet-open general-purpose origin circuits. */
static int
count_pending_general_client_circuits(void)
{
  int count = 0;

  SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *, circ) {
    if (circ->marked_for_close ||
        circ->state == CIRCUIT_STATE_OPEN ||
        !CIRCUIT_PURPOSE_COUNTS_TOWARDS_MAXPENDING(circ->purpose) ||
        !CIRCUIT_IS_ORIGIN(circ))
      continue;

    ++count;
  }
  SMARTLIST_FOREACH_END(circ);

  return count;
}

#if 0
/** Check whether, according to the policies in <b>options</b>, the
 * circuit <b>circ</b> makes sense. */
/* XXXX currently only checks Exclude{Exit}Nodes; it should check more.
 * Also, it doesn't have the right definition of an exit circuit. Also,
 * it's never called. */
int
circuit_conforms_to_options(const origin_circuit_t *circ,
                            const or_options_t *options)
{
  const crypt_path_t *cpath, *cpath_next = NULL;

  /* first check if it includes any excluded nodes */
  for (cpath = circ->cpath; cpath_next != circ->cpath; cpath = cpath_next) {
    cpath_next = cpath->next;
    if (routerset_contains_extendinfo(options->ExcludeNodes,
                                      cpath->extend_info))
      return 0;
  }

  /* then consider the final hop */
  if (routerset_contains_extendinfo(options->ExcludeExitNodes,
                                    circ->cpath->prev->extend_info))
    return 0;

  return 1;
}
#endif /* 0 */

/**
 * Close all circuits that start at us, aren't open, and were born
 * at least CircuitBuildTimeout seconds ago.
 *
 * TODO: This function is now partially redundant to
 * circuit_build_times_handle_completed_hop(), but that function only
 * covers circuits up to and including 3 hops that are still actually
 * completing hops. However, circuit_expire_building() also handles longer
 * circuits, as well as circuits that are completely stalled.
 * In the future (after prop247/other path selection revamping), we probably
 * want to eliminate this rats nest in favor of a simpler approach.
 */
void
circuit_expire_building(void)
{
  /* circ_times.timeout_ms and circ_times.close_ms are from
   * circuit_build_times_get_initial_timeout() if we haven't computed
   * custom timeouts yet */
  struct timeval general_cutoff, begindir_cutoff, fourhop_cutoff,
    close_cutoff, extremely_old_cutoff, hs_extremely_old_cutoff,
    cannibalized_cutoff, c_intro_cutoff, s_intro_cutoff, stream_cutoff;
  const or_options_t *options = get_options();
  struct timeval now;
  cpath_build_state_t *build_state;
  int any_opened_circs = 0;

  tor_gettimeofday(&now);

  /* Check to see if we have any opened circuits. If we don't,
   * we want to be more lenient with timeouts, in case the
   * user has relocated and/or changed network connections.
   * See bug #3443. */
  any_opened_circs = circuit_any_opened_circuits();

#define SET_CUTOFF(target, msec) do {                       \
    long ms = tor_lround(msec);                             \
    struct timeval diff;                                    \
    diff.tv_sec = ms / 1000;                                \
    diff.tv_usec = (int)((ms % 1000) * 1000);               \
    timersub(&now, &diff, &target);                         \
  } while (0)

  /**
   * Because circuit build timeout is calculated only based on 3 hop
   * general purpose circuit construction, we need to scale the timeout
   * to make it properly apply to longer circuits, and circuits of
   * certain usage types. The following diagram illustrates how we
   * derive the scaling below. In short, we calculate the number
   * of times our telescoping-based circuit construction causes cells
   * to traverse each link for the circuit purpose types in question,
   * and then assume each link is equivalent.
   *
   * OP --a--> A --b--> B --c--> C
   * OP --a--> A --b--> B --c--> C --d--> D
   *
   * Let h = a = b = c = d
   *
   * Three hops (general_cutoff)
   *   RTTs = 3a + 2b + c
   *   RTTs = 6h
   * Cannibalized:
   *   RTTs = a+b+c+d
   *   RTTs = 4h
   * Four hops:
   *   RTTs = 4a + 3b + 2c + d
   *   RTTs = 10h
   * Client INTRODUCE1+ACK: // XXX: correct?
   *   RTTs = 5a + 4b + 3c + 2d
   *   RTTs = 14h
   * Server intro:
   *   RTTs = 4a + 3b + 2c
   *   RTTs = 9h
   */
  SET_CUTOFF(general_cutoff, get_circuit_build_timeout_ms());
  SET_CUTOFF(begindir_cutoff, get_circuit_build_timeout_ms());

  // TODO: We should probably use route_len_for_purpose() here instead,
  // except that does not count the extra round trip for things like server
  // intros and rends.

  /* > 3hop circs seem to have a 1.0 second delay on their cannibalized
   * 4th hop. */
  SET_CUTOFF(fourhop_cutoff, get_circuit_build_timeout_ms() * (10/6.0) + 1000);

  /* CIRCUIT_PURPOSE_C_ESTABLISH_REND behaves more like a RELAY cell.
   * Use the stream cutoff (more or less). */
  SET_CUTOFF(stream_cutoff, MAX(options->CircuitStreamTimeout,15)*1000 + 1000);

  /* Be lenient with cannibalized circs. They already survived the official
   * CBT, and they're usually not performance-critical. */
  SET_CUTOFF(cannibalized_cutoff,
             MAX(get_circuit_build_close_time_ms()*(4/6.0),
                 options->CircuitStreamTimeout * 1000) + 1000);

  /* Intro circs have an extra round trip (and are also 4 hops long) */
  SET_CUTOFF(c_intro_cutoff, get_circuit_build_timeout_ms() * (14/6.0) + 1000);

  /* Server intro circs have an extra round trip */
  SET_CUTOFF(s_intro_cutoff, get_circuit_build_timeout_ms() * (9/6.0) + 1000);

  SET_CUTOFF(close_cutoff, get_circuit_build_close_time_ms());
  SET_CUTOFF(extremely_old_cutoff, get_circuit_build_close_time_ms()*2 + 1000);

  SET_CUTOFF(hs_extremely_old_cutoff,
             MAX(get_circuit_build_close_time_ms()*2 + 1000,
                 options->SocksTimeout * 1000));

  SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *,victim) {
    struct timeval cutoff;
    if (!CIRCUIT_IS_ORIGIN(victim) || /* didn't originate here */
        victim->marked_for_close)     /* don't mess with marked circs */
      continue;

    /* If we haven't yet started the first hop, it means we don't have
     * any orconns available, and thus have not started counting time yet
     * for this circuit. See circuit_deliver_create_cell() and uses of
     * timestamp_began.
     *
     * Continue to wait in this case. The ORConn should timeout
     * independently and kill us then.
     */
    if (TO_ORIGIN_CIRCUIT(victim)->cpath->state == CPATH_STATE_CLOSED) {
      continue;
    }

    build_state = TO_ORIGIN_CIRCUIT(victim)->build_state;
    if (build_state && build_state->onehop_tunnel)
      cutoff = begindir_cutoff;
    else if (victim->purpose == CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT)
      cutoff = close_cutoff;
    else if (victim->purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT)
      cutoff = c_intro_cutoff;
    else if (victim->purpose == CIRCUIT_PURPOSE_S_ESTABLISH_INTRO)
      cutoff = s_intro_cutoff;
    else if (victim->purpose == CIRCUIT_PURPOSE_C_ESTABLISH_REND)
      cutoff = stream_cutoff;
    else if (victim->purpose == CIRCUIT_PURPOSE_PATH_BIAS_TESTING)
      cutoff = close_cutoff;
    else if (TO_ORIGIN_CIRCUIT(victim)->has_opened &&
             victim->state != CIRCUIT_STATE_OPEN)
      cutoff = cannibalized_cutoff;
    else if (build_state && build_state->desired_path_len >= 4)
      cutoff = fourhop_cutoff;
    else
      cutoff = general_cutoff;

    if (TO_ORIGIN_CIRCUIT(victim)->hs_circ_has_timed_out)
      cutoff = hs_extremely_old_cutoff;

    if (timercmp(&victim->timestamp_began, &cutoff, OP_GT))
      continue; /* it's still young, leave it alone */

    /* We need to double-check the opened state here because
     * we don't want to consider opened 1-hop dircon circuits for
     * deciding when to relax the timeout, but we *do* want to relax
     * those circuits too if nothing else is opened *and* they still
     * aren't either. */
    if (!any_opened_circs && victim->state != CIRCUIT_STATE_OPEN) {
      /* It's still young enough that we wouldn't close it, right? */
      if (timercmp(&victim->timestamp_began, &close_cutoff, OP_GT)) {
        if (!TO_ORIGIN_CIRCUIT(victim)->relaxed_timeout) {
          int first_hop_succeeded = TO_ORIGIN_CIRCUIT(victim)->cpath->state
                                      == CPATH_STATE_OPEN;
          log_info(LD_CIRC,
                 "No circuits are opened. Relaxing timeout for circuit %d "
                 "(a %s %d-hop circuit in state %s with channel state %s).",
                 TO_ORIGIN_CIRCUIT(victim)->global_identifier,
                 circuit_purpose_to_string(victim->purpose),
                 TO_ORIGIN_CIRCUIT(victim)->build_state ?
                   TO_ORIGIN_CIRCUIT(victim)->build_state->desired_path_len :
                   -1,
                 circuit_state_to_string(victim->state),
                 victim->n_chan ?
                    channel_state_to_string(victim->n_chan->state) : "none");

          /* We count the timeout here for CBT, because technically this
           * was a timeout, and the timeout value needs to reset if we
           * see enough of them. Note this means we also need to avoid
           * double-counting below, too. */
          circuit_build_times_count_timeout(get_circuit_build_times_mutable(),
              first_hop_succeeded);
          TO_ORIGIN_CIRCUIT(victim)->relaxed_timeout = 1;
        }
        continue;
      } else {
        static ratelim_t relax_timeout_limit = RATELIM_INIT(3600);
        const double build_close_ms = get_circuit_build_close_time_ms();
        log_fn_ratelim(&relax_timeout_limit, LOG_NOTICE, LD_CIRC,
                 "No circuits are opened. Relaxed timeout for circuit %d "
                 "(a %s %d-hop circuit in state %s with channel state %s) to "
                 "%ldms. However, it appears the circuit has timed out "
                 "anyway.",
                 TO_ORIGIN_CIRCUIT(victim)->global_identifier,
                 circuit_purpose_to_string(victim->purpose),
                 TO_ORIGIN_CIRCUIT(victim)->build_state ?
                   TO_ORIGIN_CIRCUIT(victim)->build_state->desired_path_len :
                   -1,
                 circuit_state_to_string(victim->state),
                 victim->n_chan ?
                    channel_state_to_string(victim->n_chan->state) : "none",
                 (long)build_close_ms);
      }
    }

#if 0
    /* some debug logs, to help track bugs */
    if (victim->purpose >= CIRCUIT_PURPOSE_C_INTRODUCING &&
        victim->purpose <= CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED) {
      if (!victim->timestamp_dirty)
        log_fn(LOG_DEBUG,"Considering %sopen purpose %d to %s (circid %d)."
               "(clean).",
               victim->state == CIRCUIT_STATE_OPEN ? "" : "non",
               victim->purpose, victim->build_state->chosen_exit_name,
               victim->n_circ_id);
      else
        log_fn(LOG_DEBUG,"Considering %sopen purpose %d to %s (circid %d). "
               "%d secs since dirty.",
               victim->state == CIRCUIT_STATE_OPEN ? "" : "non",
               victim->purpose, victim->build_state->chosen_exit_name,
               victim->n_circ_id,
               (int)(now - victim->timestamp_dirty));
    }
#endif /* 0 */

    /* if circ is !open, or if it's open but purpose is a non-finished
     * intro or rend, then mark it for close */
    if (victim->state == CIRCUIT_STATE_OPEN) {
      switch (victim->purpose) {
        default: /* most open circuits can be left alone. */
          continue; /* yes, continue inside a switch refers to the nearest
                     * enclosing loop. C is smart. */
        case CIRCUIT_PURPOSE_S_ESTABLISH_INTRO:
          break; /* too old, need to die */
        case CIRCUIT_PURPOSE_C_REND_READY:
          /* it's a rend_ready circ -- has it already picked a query? */
          /* c_rend_ready circs measure age since timestamp_dirty,
           * because that's set when they switch purposes
           */
          if (TO_ORIGIN_CIRCUIT(victim)->rend_data ||
              TO_ORIGIN_CIRCUIT(victim)->hs_ident ||
              victim->timestamp_dirty > cutoff.tv_sec)
            continue;
          break;
        case CIRCUIT_PURPOSE_PATH_BIAS_TESTING:
          /* Open path bias testing circuits are given a long
           * time to complete the test, but not forever */
          TO_ORIGIN_CIRCUIT(victim)->path_state = PATH_STATE_USE_FAILED;
          break;
        case CIRCUIT_PURPOSE_C_INTRODUCING:
          /* That purpose means that the intro point circuit has been opened
           * successfully but the INTRODUCE1 cell hasn't been sent yet because
           * the client is waiting for the rendezvous point circuit to open.
           * Keep this circuit open while waiting for the rendezvous circuit.
           * We let the circuit idle timeout take care of cleaning this
           * circuit if it never used. */
          continue;
        case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
        case CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED:
        case CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT:
          /* rend and intro circs become dirty each time they
           * make an introduction attempt. so timestamp_dirty
           * will reflect the time since the last attempt.
           */
          if (victim->timestamp_dirty > cutoff.tv_sec)
            continue;
          break;
      }
    } else { /* circuit not open, consider recording failure as timeout */
      int first_hop_succeeded = TO_ORIGIN_CIRCUIT(victim)->cpath &&
            TO_ORIGIN_CIRCUIT(victim)->cpath->state == CPATH_STATE_OPEN;

      if (TO_ORIGIN_CIRCUIT(victim)->p_streams != NULL) {
        log_warn(LD_BUG, "Circuit %d (purpose %d, %s) has timed out, "
                 "yet has attached streams!",
                 TO_ORIGIN_CIRCUIT(victim)->global_identifier,
                 victim->purpose,
                 circuit_purpose_to_string(victim->purpose));
        tor_fragile_assert();
        continue;
      }

      if (circuit_timeout_want_to_count_circ(TO_ORIGIN_CIRCUIT(victim)) &&
          circuit_build_times_enough_to_compute(get_circuit_build_times())) {

        log_info(LD_CIRC,
                 "Deciding to count the timeout for circuit %"PRIu32"\n",
                 TO_ORIGIN_CIRCUIT(victim)->global_identifier);

        /* Circuits are allowed to last longer for measurement.
         * Switch their purpose and wait. */
        if (victim->purpose != CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT) {
          circuit_build_times_mark_circ_as_measurement_only(TO_ORIGIN_CIRCUIT(
                                                            victim));
          continue;
        }

        /*
         * If the circuit build time is much greater than we would have cut
         * it off at, we probably had a suspend event along this codepath,
         * and we should discard the value.
         */
        if (timercmp(&victim->timestamp_began, &extremely_old_cutoff, OP_LT)) {
          log_notice(LD_CIRC,
                     "Extremely large value for circuit build timeout: %lds. "
                     "Assuming clock jump. Purpose %d (%s)",
                     (long)(now.tv_sec - victim->timestamp_began.tv_sec),
                     victim->purpose,
                     circuit_purpose_to_string(victim->purpose));
        } else if (circuit_build_times_count_close(
            get_circuit_build_times_mutable(),
            first_hop_succeeded,
            (time_t)victim->timestamp_created.tv_sec)) {
          circuit_build_times_set_timeout(get_circuit_build_times_mutable());
        }
      }
    }

    /* If this is a hidden service client circuit which is far enough along in
     * connecting to its destination, and we haven't already flagged it as
     * 'timed out', flag it so we'll launch another intro or rend circ, but
     * don't mark it for close yet.
     *
     * (Circs flagged as 'timed out' are given a much longer timeout
     * period above, so we won't close them in the next call to
     * circuit_expire_building.) */
    if (!(TO_ORIGIN_CIRCUIT(victim)->hs_circ_has_timed_out)) {
      switch (victim->purpose) {
      case CIRCUIT_PURPOSE_C_REND_READY:
        /* We only want to spare a rend circ if it has been specified in
         * an INTRODUCE1 cell sent to a hidden service.  A circ's
         * pending_final_cpath field is non-NULL iff it is a rend circ
         * and we have tried to send an INTRODUCE1 cell specifying it.
         * Thus, if the pending_final_cpath field *is* NULL, then we
         * want to not spare it. */
        if (TO_ORIGIN_CIRCUIT(victim)->build_state &&
            TO_ORIGIN_CIRCUIT(victim)->build_state->pending_final_cpath ==
            NULL)
          break;
        /* fallthrough! */
      case CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT:
      case CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED:
        /* If we have reached this line, we want to spare the circ for now. */
        log_info(LD_CIRC,"Marking circ %u (state %d:%s, purpose %d) "
                 "as timed-out HS circ",
                 (unsigned)victim->n_circ_id,
                 victim->state, circuit_state_to_string(victim->state),
                 victim->purpose);
        TO_ORIGIN_CIRCUIT(victim)->hs_circ_has_timed_out = 1;
        continue;
      default:
        break;
      }
    }

    /* If this is a service-side rendezvous circuit which is far
     * enough along in connecting to its destination, consider sparing
     * it. */
    if (!(TO_ORIGIN_CIRCUIT(victim)->hs_circ_has_timed_out) &&
        victim->purpose == CIRCUIT_PURPOSE_S_CONNECT_REND) {
      log_info(LD_CIRC,"Marking circ %u (state %d:%s, purpose %d) "
               "as timed-out HS circ; relaunching rendezvous attempt.",
               (unsigned)victim->n_circ_id,
               victim->state, circuit_state_to_string(victim->state),
               victim->purpose);
      TO_ORIGIN_CIRCUIT(victim)->hs_circ_has_timed_out = 1;
      hs_circ_retry_service_rendezvous_point(TO_ORIGIN_CIRCUIT(victim));
      continue;
    }

    if (victim->n_chan)
      log_info(LD_CIRC,
               "Abandoning circ %u %s:%u (state %d,%d:%s, purpose %d, "
               "len %d)", TO_ORIGIN_CIRCUIT(victim)->global_identifier,
               channel_get_canonical_remote_descr(victim->n_chan),
               (unsigned)victim->n_circ_id,
               TO_ORIGIN_CIRCUIT(victim)->has_opened,
               victim->state, circuit_state_to_string(victim->state),
               victim->purpose,
               TO_ORIGIN_CIRCUIT(victim)->build_state ?
                 TO_ORIGIN_CIRCUIT(victim)->build_state->desired_path_len :
                 -1);
    else
      log_info(LD_CIRC,
               "Abandoning circ %u %u (state %d,%d:%s, purpose %d, len %d)",
               TO_ORIGIN_CIRCUIT(victim)->global_identifier,
               (unsigned)victim->n_circ_id,
               TO_ORIGIN_CIRCUIT(victim)->has_opened,
               victim->state,
               circuit_state_to_string(victim->state), victim->purpose,
               TO_ORIGIN_CIRCUIT(victim)->build_state ?
                 TO_ORIGIN_CIRCUIT(victim)->build_state->desired_path_len :
                 -1);

    circuit_log_path(LOG_INFO,LD_CIRC,TO_ORIGIN_CIRCUIT(victim));
    if (victim->purpose == CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT)
      circuit_mark_for_close(victim, END_CIRC_REASON_MEASUREMENT_EXPIRED);
    else
      circuit_mark_for_close(victim, END_CIRC_REASON_TIMEOUT);

    pathbias_count_timeout(TO_ORIGIN_CIRCUIT(victim));
  } SMARTLIST_FOREACH_END(victim);
}

/**
 * Mark for close all circuits that start here, that were built through a
 * guard we weren't sure if we wanted to use, and that have been waiting
 * around for way too long.
 */
void
circuit_expire_waiting_for_better_guard(void)
{
  SMARTLIST_FOREACH_BEGIN(circuit_get_global_origin_circuit_list(),
                          origin_circuit_t *, circ) {
    if (TO_CIRCUIT(circ)->marked_for_close)
      continue;
    if (circ->guard_state == NULL)
      continue;
    if (entry_guard_state_should_expire(circ->guard_state))
      circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_NONE);
  } SMARTLIST_FOREACH_END(circ);
}

/** For debugging #8387: track when we last called
 * circuit_expire_old_circuits_clientside. */
static time_t last_expired_clientside_circuits = 0;

/**
 * As a diagnostic for bug 8387, log information about how many one-hop
 * circuits we have around that have been there for at least <b>age</b>
 * seconds. Log a few of them. Ignores Single Onion Service intro, it is
 * expected to be long-term one-hop circuits.
 */
void
circuit_log_ancient_one_hop_circuits(int age)
{
#define MAX_ANCIENT_ONEHOP_CIRCUITS_TO_LOG 10
  time_t now = time(NULL);
  time_t cutoff = now - age;
  int n_found = 0;
  smartlist_t *log_these = smartlist_new();
  const or_options_t *options = get_options();

  SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *, circ) {
    const origin_circuit_t *ocirc;
    if (! CIRCUIT_IS_ORIGIN(circ))
      continue;
    if (circ->timestamp_created.tv_sec >= cutoff)
      continue;
    /* Single Onion Services deliberately make long term one-hop intro
     * and rendezvous connections. Don't log the established ones. */
    if (rend_service_allow_non_anonymous_connection(options) &&
        (circ->purpose == CIRCUIT_PURPOSE_S_INTRO ||
         circ->purpose == CIRCUIT_PURPOSE_S_REND_JOINED))
      continue;
    ocirc = CONST_TO_ORIGIN_CIRCUIT(circ);

    if (ocirc->build_state && ocirc->build_state->onehop_tunnel) {
      ++n_found;

      if (smartlist_len(log_these) < MAX_ANCIENT_ONEHOP_CIRCUITS_TO_LOG)
        smartlist_add(log_these, (origin_circuit_t*) ocirc);
    }
  }
  SMARTLIST_FOREACH_END(circ);

  if (n_found == 0)
    goto done;

  log_notice(LD_HEARTBEAT,
             "Diagnostic for issue 8387: Found %d one-hop circuits more "
             "than %d seconds old! Logging %d...",
             n_found, age, smartlist_len(log_these));

  SMARTLIST_FOREACH_BEGIN(log_these, const origin_circuit_t *, ocirc) {
    char created[ISO_TIME_LEN+1];
    int stream_num;
    const edge_connection_t *conn;
    char *dirty = NULL;
    const circuit_t *circ = TO_CIRCUIT(ocirc);

    format_local_iso_time(created,
                          (time_t)circ->timestamp_created.tv_sec);

    if (circ->timestamp_dirty) {
      char dirty_since[ISO_TIME_LEN+1];
      format_local_iso_time(dirty_since, circ->timestamp_dirty);

      tor_asprintf(&dirty, "Dirty since %s (%ld seconds vs %ld-second cutoff)",
                   dirty_since, (long)(now - circ->timestamp_dirty),
                   (long) options->MaxCircuitDirtiness);
    } else {
      dirty = tor_strdup("Not marked dirty");
    }

    log_notice(LD_HEARTBEAT, "  #%d created at %s. %s, %s. %s for close. "
               "Package window: %d. "
               "%s for new conns. %s.",
               ocirc_sl_idx,
               created,
               circuit_state_to_string(circ->state),
               circuit_purpose_to_string(circ->purpose),
               circ->marked_for_close ? "Marked" : "Not marked",
               circ->package_window,
               ocirc->unusable_for_new_conns ? "Not usable" : "usable",
               dirty);
    tor_free(dirty);

    stream_num = 0;
    for (conn = ocirc->p_streams; conn; conn = conn->next_stream) {
      const connection_t *c = TO_CONN(conn);
      char stream_created[ISO_TIME_LEN+1];
      if (++stream_num >= 5)
        break;

      format_local_iso_time(stream_created, c->timestamp_created);

      log_notice(LD_HEARTBEAT, "     Stream#%d created at %s. "
                 "%s conn in state %s. "
                 "It is %slinked and %sreading from a linked connection %p. "
                 "Package window %d. "
                 "%s for close (%s:%d). Hold-open is %sset. "
                 "Has %ssent RELAY_END. %s on circuit.",
                 stream_num,
                 stream_created,
                 conn_type_to_string(c->type),
                 conn_state_to_string(c->type, c->state),
                 c->linked ? "" : "not ",
                 c->reading_from_linked_conn ? "": "not",
                 c->linked_conn,
                 conn->package_window,
                 c->marked_for_close ? "Marked" : "Not marked",
                 c->marked_for_close_file ? c->marked_for_close_file : "--",
                 c->marked_for_close,
                 c->hold_open_until_flushed ? "" : "not ",
                 conn->edge_has_sent_end ? "" : "not ",
                 conn->edge_blocked_on_circ ? "Blocked" : "Not blocked");
      if (! c->linked_conn)
        continue;

      c = c->linked_conn;

      log_notice(LD_HEARTBEAT, "        Linked to %s connection in state %s "
                 "(Purpose %d). %s for close (%s:%d). Hold-open is %sset. ",
                 conn_type_to_string(c->type),
                 conn_state_to_string(c->type, c->state),
                 c->purpose,
                 c->marked_for_close ? "Marked" : "Not marked",
                 c->marked_for_close_file ? c->marked_for_close_file : "--",
                 c->marked_for_close,
                 c->hold_open_until_flushed ? "" : "not ");
    }
  } SMARTLIST_FOREACH_END(ocirc);

  log_notice(LD_HEARTBEAT, "It has been %ld seconds since I last called "
             "circuit_expire_old_circuits_clientside().",
             (long)(now - last_expired_clientside_circuits));

 done:
  smartlist_free(log_these);
}

/** Remove any elements in <b>needed_ports</b> that are handled by an
 * open or in-progress circuit.
 */
void
circuit_remove_handled_ports(smartlist_t *needed_ports)
{
  int i;
  uint16_t *port;

  for (i = 0; i < smartlist_len(needed_ports); ++i) {
    port = smartlist_get(needed_ports, i);
    tor_assert(*port);
    if (circuit_stream_is_being_handled(NULL, *port,
                                        MIN_CIRCUITS_HANDLING_STREAM)) {
      log_debug(LD_CIRC,"Port %d is already being handled; removing.", *port);
      smartlist_del(needed_ports, i--);
      tor_free(port);
    } else {
      log_debug(LD_CIRC,"Port %d is not handled.", *port);
    }
  }
}

/** Return 1 if at least <b>min</b> general-purpose non-internal circuits
 * will have an acceptable exit node for exit stream <b>conn</b> if it
 * is defined, else for "*:port".
 * Else return 0.
 */
int
circuit_stream_is_being_handled(entry_connection_t *conn,
                                uint16_t port, int min)
{
  const node_t *exitnode;
  int num=0;
  time_t now = time(NULL);
  int need_uptime = smartlist_contains_int_as_string(
                                   get_options()->LongLivedPorts,
                                   conn ? conn->socks_request->port : port);

  SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *, circ) {
    if (CIRCUIT_IS_ORIGIN(circ) &&
        !circ->marked_for_close &&
        circ->purpose == CIRCUIT_PURPOSE_C_GENERAL &&
        (!circ->timestamp_dirty ||
         circ->timestamp_dirty + get_options()->MaxCircuitDirtiness > now)) {
      origin_circuit_t *origin_circ = TO_ORIGIN_CIRCUIT(circ);
      cpath_build_state_t *build_state = origin_circ->build_state;
      if (build_state->is_internal || build_state->onehop_tunnel)
        continue;
      if (origin_circ->unusable_for_new_conns)
        continue;
      if (origin_circ->isolation_values_set &&
          (conn == NULL ||
           !connection_edge_compatible_with_circuit(conn, origin_circ)))
        continue;

      exitnode = build_state_get_exit_node(build_state);
      if (exitnode && (!need_uptime || build_state->need_uptime)) {
        int ok;
        if (conn) {
          ok = connection_ap_can_use_exit(conn, exitnode);
        } else {
          addr_policy_result_t r;
          r = compare_tor_addr_to_node_policy(NULL, port, exitnode);
          ok = r != ADDR_POLICY_REJECTED && r != ADDR_POLICY_PROBABLY_REJECTED;
        }
        if (ok) {
          if (++num >= min)
            return 1;
        }
      }
    }
  }
  SMARTLIST_FOREACH_END(circ);
  return 0;
}

/** Don't keep more than this many unused open circuits around. */
#define MAX_UNUSED_OPEN_CIRCUITS 14

/* Return true if a circuit is available for use, meaning that it is open,
 * clean, usable for new multi-hop connections, and a general purpose origin
 * circuit.
 * Accept any kind of circuit, return false if the above conditions are not
 * met. */
STATIC int
circuit_is_available_for_use(const circuit_t *circ)
{
  const origin_circuit_t *origin_circ;
  cpath_build_state_t *build_state;

  if (!CIRCUIT_IS_ORIGIN(circ))
    return 0; /* We first filter out only origin circuits before doing the
                 following checks. */
  if (circ->marked_for_close)
    return 0; /* Don't mess with marked circs */
  if (circ->timestamp_dirty)
    return 0; /* Only count clean circs */
  if (circ->purpose != CIRCUIT_PURPOSE_C_GENERAL &&
      circ->purpose != CIRCUIT_PURPOSE_HS_VANGUARDS)
    return 0; /* We only pay attention to general purpose circuits.
                 General purpose circuits are always origin circuits. */

  origin_circ = CONST_TO_ORIGIN_CIRCUIT(circ);
  if (origin_circ->unusable_for_new_conns)
    return 0;

  build_state = origin_circ->build_state;
  if (build_state->onehop_tunnel)
    return 0;

  return 1;
}

/* Return true if we need any more exit circuits.
 * needs_uptime and needs_capacity are set only if we need more exit circuits.
 * Check if we know of a port that's been requested recently and no circuit
 * is currently available that can handle it. */
STATIC int
needs_exit_circuits(time_t now, int *needs_uptime, int *needs_capacity)
{
  return (!circuit_all_predicted_ports_handled(now, needs_uptime,
                                               needs_capacity) &&
          router_have_consensus_path() == CONSENSUS_PATH_EXIT);
}

/* Hidden services need at least this many internal circuits */
#define SUFFICIENT_UPTIME_INTERNAL_HS_SERVERS 3

/* Return true if we need any more hidden service server circuits.
 * HS servers only need an internal circuit. */
STATIC int
needs_hs_server_circuits(time_t now, int num_uptime_internal)
{
  if (!rend_num_services() && !hs_service_get_num_services()) {
    /* No services, we don't need anything. */
    goto no_need;
  }

  if (num_uptime_internal >= SUFFICIENT_UPTIME_INTERNAL_HS_SERVERS) {
    /* We have sufficient amount of internal circuit. */
    goto no_need;
  }

  if (router_have_consensus_path() == CONSENSUS_PATH_UNKNOWN) {
    /* Consensus hasn't been checked or might be invalid so requesting
     * internal circuits is not wise. */
    goto no_need;
  }

  /* At this point, we need a certain amount of circuits and we will most
   * likely use them for rendezvous so we note down the use of internal
   * circuit for our prediction for circuit needing uptime and capacity. */
  rep_hist_note_used_internal(now, 1, 1);

  return 1;
 no_need:
  return 0;
}

/* We need at least this many internal circuits for hidden service clients */
#define SUFFICIENT_INTERNAL_HS_CLIENTS 3

/* We need at least this much uptime for internal circuits for hidden service
 * clients */
#define SUFFICIENT_UPTIME_INTERNAL_HS_CLIENTS 2

/* Return true if we need any more hidden service client circuits.
 * HS clients only need an internal circuit. */
STATIC int
needs_hs_client_circuits(time_t now, int *needs_uptime, int *needs_capacity,
    int num_internal, int num_uptime_internal)
{
  int used_internal_recently = rep_hist_get_predicted_internal(now,
                                                               needs_uptime,
                                                               needs_capacity);
  int requires_uptime = num_uptime_internal <
                        SUFFICIENT_UPTIME_INTERNAL_HS_CLIENTS &&
                        needs_uptime;

  return (used_internal_recently &&
         (requires_uptime || num_internal < SUFFICIENT_INTERNAL_HS_CLIENTS) &&
          router_have_consensus_path() != CONSENSUS_PATH_UNKNOWN);
}

/* This is how many circuits can be opened concurrently during the cbt learning
 * phase. This number cannot exceed the tor-wide MAX_UNUSED_OPEN_CIRCUITS. */
#define DFLT_CBT_UNUSED_OPEN_CIRCS (10)
#define MIN_CBT_UNUSED_OPEN_CIRCS 0
#define MAX_CBT_UNUSED_OPEN_CIRCS MAX_UNUSED_OPEN_CIRCUITS

/* Return true if we need more circuits for a good build timeout.
 * XXXX make the assumption that build timeout streams should be
 * created whenever we can build internal circuits. */
STATIC int
needs_circuits_for_build(int num)
{
  if (router_have_consensus_path() != CONSENSUS_PATH_UNKNOWN) {
    if (num < networkstatus_get_param(NULL, "cbtmaxopencircs",
                              DFLT_CBT_UNUSED_OPEN_CIRCS,
                              MIN_CBT_UNUSED_OPEN_CIRCS,
                              MAX_CBT_UNUSED_OPEN_CIRCS) &&
        !circuit_build_times_disabled(get_options()) &&
        circuit_build_times_needs_circuits_now(get_circuit_build_times())) {
      return 1;
    }
  }
  return 0;
}

/**
 * Launch the appropriate type of predicted circuit for hidden
 * services, depending on our options.
 */
static void
circuit_launch_predicted_hs_circ(int flags)
{
  /* K.I.S.S. implementation of bug #23101: If we are using
   * vanguards or pinned middles, pre-build a specific purpose
   * for HS circs. */
  if (circuit_should_use_vanguards(CIRCUIT_PURPOSE_HS_VANGUARDS)) {
    circuit_launch(CIRCUIT_PURPOSE_HS_VANGUARDS, flags);
  } else {
    /* If no vanguards, then no HS-specific prebuilt circuits are needed.
     * Normal GENERAL circs are fine */
    circuit_launch(CIRCUIT_PURPOSE_C_GENERAL, flags);
  }
}

/** Determine how many circuits we have open that are clean,
 * Make sure it's enough for all the upcoming behaviors we predict we'll have.
 * But put an upper bound on the total number of circuits.
 */
static void
circuit_predict_and_launch_new(void)
{
  int num=0, num_internal=0, num_uptime_internal=0;
  int hidserv_needs_uptime=0, hidserv_needs_capacity=1;
  int port_needs_uptime=0, port_needs_capacity=1;
  time_t now = time(NULL);
  int flags = 0;

  /* Count how many of each type of circuit we currently have. */
  SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *, circ) {
    if (!circuit_is_available_for_use(circ))
      continue;

    num++;

    cpath_build_state_t *build_state = TO_ORIGIN_CIRCUIT(circ)->build_state;
    if (build_state->is_internal)
      num_internal++;
    if (build_state->need_uptime && build_state->is_internal)
      num_uptime_internal++;
  }
  SMARTLIST_FOREACH_END(circ);

  /* If that's enough, then stop now. */
  if (num >= MAX_UNUSED_OPEN_CIRCUITS)
    return;

  if (needs_exit_circuits(now, &port_needs_uptime, &port_needs_capacity)) {
    if (port_needs_uptime)
      flags |= CIRCLAUNCH_NEED_UPTIME;
    if (port_needs_capacity)
      flags |= CIRCLAUNCH_NEED_CAPACITY;

    log_info(LD_CIRC,
             "Have %d clean circs (%d internal), need another exit circ.",
             num, num_internal);
    circuit_launch(CIRCUIT_PURPOSE_C_GENERAL, flags);
    return;
  }

  if (needs_hs_server_circuits(now, num_uptime_internal)) {
    flags = (CIRCLAUNCH_NEED_CAPACITY | CIRCLAUNCH_NEED_UPTIME |
             CIRCLAUNCH_IS_INTERNAL);

    log_info(LD_CIRC,
             "Have %d clean circs (%d internal), need another internal "
             "circ for my hidden service.",
             num, num_internal);
    circuit_launch_predicted_hs_circ(flags);
    return;
  }

  if (needs_hs_client_circuits(now, &hidserv_needs_uptime,
                               &hidserv_needs_capacity,
                               num_internal, num_uptime_internal))
  {
    if (hidserv_needs_uptime)
      flags |= CIRCLAUNCH_NEED_UPTIME;
    if (hidserv_needs_capacity)
      flags |= CIRCLAUNCH_NEED_CAPACITY;
    flags |= CIRCLAUNCH_IS_INTERNAL;

    log_info(LD_CIRC,
             "Have %d clean circs (%d uptime-internal, %d internal), need"
             " another hidden service circ.",
             num, num_uptime_internal, num_internal);

    circuit_launch_predicted_hs_circ(flags);
    return;
  }

  if (needs_circuits_for_build(num)) {
    flags = CIRCLAUNCH_NEED_CAPACITY;
    /* if there are no exits in the consensus, make timeout
     * circuits internal */
    if (router_have_consensus_path() == CONSENSUS_PATH_INTERNAL)
      flags |= CIRCLAUNCH_IS_INTERNAL;

      log_info(LD_CIRC,
               "Have %d clean circs need another buildtime test circ.", num);
      circuit_launch(CIRCUIT_PURPOSE_C_GENERAL, flags);
      return;
  }
}

/** Build a new test circuit every 5 minutes */
#define TESTING_CIRCUIT_INTERVAL 300

/** This function is called once a second, if router_have_minimum_dir_info()
 * is true. Its job is to make sure all services we offer have enough circuits
 * available. Some services just want enough circuits for current tasks,
 * whereas others want a minimum set of idle circuits hanging around.
 */
void
circuit_build_needed_circs(time_t now)
{
  const or_options_t *options = get_options();

  /* launch a new circ for any pending streams that need one
   * XXXX make the assumption that (some) AP streams (i.e. HS clients)
   * don't require an exit circuit, review in #13814.
   * This allows HSs to function in a consensus without exits. */
  if (router_have_consensus_path() != CONSENSUS_PATH_UNKNOWN)
    connection_ap_rescan_and_attach_pending();

  circuit_expire_old_circs_as_needed(now);

  if (!options->DisablePredictedCircuits)
    circuit_predict_and_launch_new();
}

/**
 * Called once a second either directly or from
 * circuit_build_needed_circs(). As appropriate (once per NewCircuitPeriod)
 * resets failure counts and expires old circuits.
 */
void
circuit_expire_old_circs_as_needed(time_t now)
{
  static time_t time_to_expire_and_reset = 0;

  if (time_to_expire_and_reset < now) {
    circuit_reset_failure_count(1);
    time_to_expire_and_reset = now + get_options()->NewCircuitPeriod;
    if (proxy_mode(get_options()))
      addressmap_clean(now);
    circuit_expire_old_circuits_clientside();

#if 0 /* disable for now, until predict-and-launch-new can cull leftovers */

    /* If we ever re-enable, this has to move into
     * circuit_build_needed_circs */

    circ = circuit_get_youngest_clean_open(CIRCUIT_PURPOSE_C_GENERAL);
    if (get_options()->RunTesting &&
        circ &&
        circ->timestamp_began.tv_sec + TESTING_CIRCUIT_INTERVAL < now) {
      log_fn(LOG_INFO,"Creating a new testing circuit.");
      circuit_launch(CIRCUIT_PURPOSE_C_GENERAL, 0);
    }
#endif /* 0 */
  }
}

/** If the stream <b>conn</b> is a member of any of the linked
 * lists of <b>circ</b>, then remove it from the list.
 */
void
circuit_detach_stream(circuit_t *circ, edge_connection_t *conn)
{
  edge_connection_t *prevconn;

  tor_assert(circ);
  tor_assert(conn);

  if (conn->base_.type == CONN_TYPE_AP) {
    entry_connection_t *entry_conn = EDGE_TO_ENTRY_CONN(conn);
    entry_conn->may_use_optimistic_data = 0;
  }
  conn->cpath_layer = NULL; /* don't keep a stale pointer */
  conn->on_circuit = NULL;

  if (CIRCUIT_IS_ORIGIN(circ)) {
    origin_circuit_t *origin_circ = TO_ORIGIN_CIRCUIT(circ);
    int removed = 0;
    if (conn == origin_circ->p_streams) {
      origin_circ->p_streams = conn->next_stream;
      removed = 1;
    } else {
      for (prevconn = origin_circ->p_streams;
           prevconn && prevconn->next_stream && prevconn->next_stream != conn;
           prevconn = prevconn->next_stream)
        ;
      if (prevconn && prevconn->next_stream) {
        prevconn->next_stream = conn->next_stream;
        removed = 1;
      }
    }
    if (removed) {
      log_debug(LD_APP, "Removing stream %d from circ %u",
                conn->stream_id, (unsigned)circ->n_circ_id);

      /* If the stream was removed, and it was a rend stream, decrement the
       * number of streams on the circuit associated with the rend service.
       */
      if (circ->purpose == CIRCUIT_PURPOSE_S_REND_JOINED) {
        hs_dec_rdv_stream_counter(origin_circ);
      }
      return;
    }
  } else {
    or_circuit_t *or_circ = TO_OR_CIRCUIT(circ);
    if (conn == or_circ->n_streams) {
      or_circ->n_streams = conn->next_stream;
      return;
    }
    if (conn == or_circ->resolving_streams) {
      or_circ->resolving_streams = conn->next_stream;
      return;
    }

    for (prevconn = or_circ->n_streams;
         prevconn && prevconn->next_stream && prevconn->next_stream != conn;
         prevconn = prevconn->next_stream)
      ;
    if (prevconn && prevconn->next_stream) {
      prevconn->next_stream = conn->next_stream;
      return;
    }

    for (prevconn = or_circ->resolving_streams;
         prevconn && prevconn->next_stream && prevconn->next_stream != conn;
         prevconn = prevconn->next_stream)
      ;
    if (prevconn && prevconn->next_stream) {
      prevconn->next_stream = conn->next_stream;
      return;
    }
  }

  log_warn(LD_BUG,"Edge connection not in circuit's list.");
  /* Don't give an error here; it's harmless. */
  tor_fragile_assert();
}

/** Find each circuit that has been unused for too long, or dirty
 * for too long and has no streams on it: mark it for close.
 */
static void
circuit_expire_old_circuits_clientside(void)
{
  struct timeval cutoff, now;

  tor_gettimeofday(&now);
  last_expired_clientside_circuits = now.tv_sec;

  SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *, circ) {
    if (circ->marked_for_close || !CIRCUIT_IS_ORIGIN(circ))
      continue;

    cutoff = now;
    cutoff.tv_sec -= TO_ORIGIN_CIRCUIT(circ)->circuit_idle_timeout;

    /* If the circuit has been dirty for too long, and there are no streams
     * on it, mark it for close.
     */
    if (circ->timestamp_dirty &&
        circ->timestamp_dirty + get_options()->MaxCircuitDirtiness <
          now.tv_sec &&
        !TO_ORIGIN_CIRCUIT(circ)->p_streams /* nothing attached */ ) {
      log_debug(LD_CIRC, "Closing n_circ_id %u (dirty %ld sec ago, "
                "purpose %d)",
                (unsigned)circ->n_circ_id,
                (long)(now.tv_sec - circ->timestamp_dirty),
                circ->purpose);
      /* Don't do this magic for testing circuits. Their death is governed
       * by circuit_expire_building */
      if (circ->purpose != CIRCUIT_PURPOSE_PATH_BIAS_TESTING)
        circuit_mark_for_close(circ, END_CIRC_REASON_FINISHED);
    } else if (!circ->timestamp_dirty && circ->state == CIRCUIT_STATE_OPEN) {
      if (timercmp(&circ->timestamp_began, &cutoff, OP_LT)) {
        if (circ->purpose == CIRCUIT_PURPOSE_C_GENERAL ||
                circ->purpose == CIRCUIT_PURPOSE_C_HSDIR_GET ||
                circ->purpose == CIRCUIT_PURPOSE_S_HSDIR_POST ||
                circ->purpose == CIRCUIT_PURPOSE_HS_VANGUARDS ||
                circ->purpose == CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT ||
                circ->purpose == CIRCUIT_PURPOSE_S_ESTABLISH_INTRO ||
                circ->purpose == CIRCUIT_PURPOSE_TESTING ||
                (circ->purpose >= CIRCUIT_PURPOSE_C_INTRODUCING &&
                circ->purpose <= CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED) ||
                circ->purpose == CIRCUIT_PURPOSE_S_CONNECT_REND) {
          log_info(LD_CIRC,
                    "Closing circuit %"PRIu32
                    " that has been unused for %ld msec.",
                   TO_ORIGIN_CIRCUIT(circ)->global_identifier,
                   tv_mdiff(&circ->timestamp_began, &now));
          circuit_mark_for_close(circ, END_CIRC_REASON_FINISHED);
        } else if (!TO_ORIGIN_CIRCUIT(circ)->is_ancient) {
          /* Server-side rend joined circuits can end up really old, because
           * they are reused by clients for longer than normal. The client
           * controls their lifespan. (They never become dirty, because
           * connection_exit_begin_conn() never marks anything as dirty.)
           * Similarly, server-side intro circuits last a long time. */
          if (circ->purpose != CIRCUIT_PURPOSE_S_REND_JOINED &&
              circ->purpose != CIRCUIT_PURPOSE_S_INTRO) {
            log_notice(LD_CIRC,
                       "Ancient non-dirty circuit %d is still around after "
                       "%ld milliseconds. Purpose: %d (%s)",
                       TO_ORIGIN_CIRCUIT(circ)->global_identifier,
                       tv_mdiff(&circ->timestamp_began, &now),
                       circ->purpose,
                       circuit_purpose_to_string(circ->purpose));
            TO_ORIGIN_CIRCUIT(circ)->is_ancient = 1;
          }
        }
      }
    }
  } SMARTLIST_FOREACH_END(circ);
}

/** How long do we wait before killing circuits with the properties
 * described below?
 *
 * Probably we could choose a number here as low as 5 to 10 seconds,
 * since these circs are used for begindir, and a) generally you either
 * ask another begindir question right after or you don't for a long time,
 * b) clients at least through 0.2.1.x choose from the whole set of
 * directory mirrors at each choice, and c) re-establishing a one-hop
 * circuit via create-fast is a light operation assuming the TLS conn is
 * still there.
 *
 * I expect "b" to go away one day when we move to using directory
 * guards, but I think "a" and "c" are good enough reasons that a low
 * number is safe even then.
 */
#define IDLE_ONE_HOP_CIRC_TIMEOUT 60

/** Find each non-origin circuit that has been unused for too long,
 * has no streams on it, came from a client, and ends here: mark it
 * for close.
 */
void
circuit_expire_old_circuits_serverside(time_t now)
{
  or_circuit_t *or_circ;
  time_t cutoff = now - IDLE_ONE_HOP_CIRC_TIMEOUT;

  SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *, circ) {
    if (circ->marked_for_close || CIRCUIT_IS_ORIGIN(circ))
      continue;
    or_circ = TO_OR_CIRCUIT(circ);
    /* If the circuit has been idle for too long, and there are no streams
     * on it, and it ends here, and it used a create_fast, mark it for close.
     */
    if (or_circ->p_chan && channel_is_client(or_circ->p_chan) &&
        !circ->n_chan &&
        !or_circ->n_streams && !or_circ->resolving_streams &&
        channel_when_last_xmit(or_circ->p_chan) <= cutoff) {
      log_info(LD_CIRC, "Closing circ_id %u (empty %d secs ago)",
               (unsigned)or_circ->p_circ_id,
               (int)(now - channel_when_last_xmit(or_circ->p_chan)));
      circuit_mark_for_close(circ, END_CIRC_REASON_FINISHED);
    }
  }
  SMARTLIST_FOREACH_END(circ);
}

/** Number of testing circuits we want open before testing our bandwidth. */
#define NUM_PARALLEL_TESTING_CIRCS 4

/** True iff we've ever had enough testing circuits open to test our
 * bandwidth. */
static int have_performed_bandwidth_test = 0;

/** Reset have_performed_bandwidth_test, so we'll start building
 * testing circuits again so we can exercise our bandwidth. */
void
reset_bandwidth_test(void)
{
  have_performed_bandwidth_test = 0;
}

/** Return 1 if we've already exercised our bandwidth, or if we
 * have fewer than NUM_PARALLEL_TESTING_CIRCS testing circuits
 * established or on the way. Else return 0.
 */
int
circuit_enough_testing_circs(void)
{
  int num = 0;

  if (have_performed_bandwidth_test)
    return 1;

  SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *, circ) {
    if (!circ->marked_for_close && CIRCUIT_IS_ORIGIN(circ) &&
        circ->purpose == CIRCUIT_PURPOSE_TESTING &&
        circ->state == CIRCUIT_STATE_OPEN)
      num++;
  }
  SMARTLIST_FOREACH_END(circ);
  return num >= NUM_PARALLEL_TESTING_CIRCS;
}

/** A testing circuit has completed. Take whatever stats we want.
 * Noticing reachability is taken care of in onionskin_answer(),
 * so there's no need to record anything here. But if we still want
 * to do the bandwidth test, and we now have enough testing circuits
 * open, do it.
 */
static void
circuit_testing_opened(origin_circuit_t *circ)
{
  if (have_performed_bandwidth_test ||
      !check_whether_orport_reachable(get_options())) {
    /* either we've already done everything we want with testing circuits,
     * or this testing circuit became open due to a fluke, e.g. we picked
     * a last hop where we already had the connection open due to an
     * outgoing local circuit. */
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_AT_ORIGIN);
  } else if (circuit_enough_testing_circs()) {
    router_perform_bandwidth_test(NUM_PARALLEL_TESTING_CIRCS, time(NULL));
    have_performed_bandwidth_test = 1;
  } else
    router_do_reachability_checks(1, 0);
}

/** A testing circuit has failed to build. Take whatever stats we want. */
static void
circuit_testing_failed(origin_circuit_t *circ, int at_last_hop)
{
  const or_options_t *options = get_options();
  if (server_mode(options) && check_whether_orport_reachable(options))
    return;

  log_info(LD_GENERAL,
           "Our testing circuit (to see if your ORPort is reachable) "
           "has failed. I'll try again later.");

  /* These aren't used yet. */
  (void)circ;
  (void)at_last_hop;
}

/** The circuit <b>circ</b> has just become open. Take the next
 * step: for rendezvous circuits, we pass circ to the appropriate
 * function in rendclient or rendservice. For general circuits, we
 * call connection_ap_attach_pending, which looks for pending streams
 * that could use circ.
 */
void
circuit_has_opened(origin_circuit_t *circ)
{
  control_event_circuit_status(circ, CIRC_EVENT_BUILT, 0);

  /* Remember that this circuit has finished building. Now if we start
   * it building again later (e.g. by extending it), we will know not
   * to consider its build time. */
  circ->has_opened = 1;

  switch (TO_CIRCUIT(circ)->purpose) {
    case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
      hs_client_circuit_has_opened(circ);
      /* Start building an intro circ if we don't have one yet. */
      connection_ap_attach_pending(1);
      /* This isn't a call to circuit_try_attaching_streams because a
       * circuit in _C_ESTABLISH_REND state isn't connected to its
       * hidden service yet, thus we can't attach streams to it yet,
       * thus circuit_try_attaching_streams would always clear the
       * circuit's isolation state.  circuit_try_attaching_streams is
       * called later, when the rend circ enters _C_REND_JOINED
       * state. */
      break;
    case CIRCUIT_PURPOSE_C_INTRODUCING:
      hs_client_circuit_has_opened(circ);
      break;
    case CIRCUIT_PURPOSE_C_GENERAL:
    case CIRCUIT_PURPOSE_C_HSDIR_GET:
    case CIRCUIT_PURPOSE_S_HSDIR_POST:
      /* Tell any AP connections that have been waiting for a new
       * circuit that one is ready. */
      circuit_try_attaching_streams(circ);
      break;
    case CIRCUIT_PURPOSE_S_ESTABLISH_INTRO:
      /* at the service, waiting for introductions */
      hs_service_circuit_has_opened(circ);
      break;
    case CIRCUIT_PURPOSE_S_CONNECT_REND:
      /* at the service, connecting to rend point */
      hs_service_circuit_has_opened(circ);
      break;
    case CIRCUIT_PURPOSE_TESTING:
      circuit_testing_opened(circ);
      break;
    /* default:
     * This won't happen in normal operation, but might happen if the
     * controller did it. Just let it slide. */
  }
}

/** If the stream-isolation state of <b>circ</b> can be cleared, clear
 * it.  Return non-zero iff <b>circ</b>'s isolation state was cleared. */
static int
circuit_try_clearing_isolation_state(origin_circuit_t *circ)
{
  if (/* The circuit may have become non-open if it was cannibalized.*/
      circ->base_.state == CIRCUIT_STATE_OPEN &&
      /* If !isolation_values_set, there is nothing to clear. */
      circ->isolation_values_set &&
      /* It's not legal to clear a circuit's isolation info if it's ever had
       * streams attached */
      !circ->isolation_any_streams_attached) {
    /* If we have any isolation information set on this circuit, and
     * we didn't manage to attach any streams to it, then we can
     * and should clear it and try again. */
    circuit_clear_isolation(circ);
    return 1;
  } else {
    return 0;
  }
}

/** Called when a circuit becomes ready for streams to be attached to
 * it. */
void
circuit_try_attaching_streams(origin_circuit_t *circ)
{
  /* Attach streams to this circuit if we can. */
  connection_ap_attach_pending(1);

  /* The call to circuit_try_clearing_isolation_state here will do
   * nothing and return 0 if we didn't attach any streams to circ
   * above. */
  if (circuit_try_clearing_isolation_state(circ)) {
    /* Maybe *now* we can attach some streams to this circuit. */
    connection_ap_attach_pending(1);
  }
}

/** Called whenever a circuit could not be successfully built.
 */
void
circuit_build_failed(origin_circuit_t *circ)
{
  channel_t *n_chan = NULL;
  /* we should examine circ and see if it failed because of
   * the last hop or an earlier hop. then use this info below.
   */
  int failed_at_last_hop = 0;

  /* First, check to see if this was a path failure, rather than build
   * failure.
   *
   * Note that we deliberately use circuit_get_cpath_len() (and not
   * circuit_get_cpath_opened_len()) because we only want to ensure
   * that a full path is *chosen*. This is different than a full path
   * being *built*. We only want to count *build* failures below.
   *
   * Path selection failures can happen spuriously for a number
   * of reasons (such as aggressive/invalid user-specified path
   * restrictions in the torrc, insufficient microdescriptors, and
   * non-user reasons like exitpolicy issues), and so should not be
   * counted as failures below.
   */
  if (circuit_get_cpath_len(circ) < circ->build_state->desired_path_len) {
    static ratelim_t pathfail_limit = RATELIM_INIT(3600);
    log_fn_ratelim(&pathfail_limit, LOG_NOTICE, LD_CIRC,
             "Our circuit %u (id: %" PRIu32 ") died due to an invalid "
             "selected path, purpose %s. This may be a torrc "
             "configuration issue, or a bug.",
              TO_CIRCUIT(circ)->n_circ_id, circ->global_identifier,
              circuit_purpose_to_string(TO_CIRCUIT(circ)->purpose));

    /* If the path failed on an RP, retry it. */
    if (TO_CIRCUIT(circ)->purpose == CIRCUIT_PURPOSE_S_CONNECT_REND)
      hs_circ_retry_service_rendezvous_point(circ);

    /* In all other cases, just bail. The rest is just failure accounting
     * that we don't want to do */
    return;
  }

  /* If the last hop isn't open, and the second-to-last is, we failed
   * at the last hop. */
  if (circ->cpath &&
      circ->cpath->prev->state != CPATH_STATE_OPEN &&
      circ->cpath->prev->prev->state == CPATH_STATE_OPEN) {
    failed_at_last_hop = 1;
  }

  /* Check if we failed at first hop */
  if (circ->cpath &&
      circ->cpath->state != CPATH_STATE_OPEN &&
      ! circ->base_.received_destroy) {
    /* We failed at the first hop for some reason other than a DESTROY cell.
     * If there's an OR connection to blame, blame it. Also, avoid this relay
     * for a while, and fail any one-hop directory fetches destined for it. */
    const char *n_chan_ident = circ->cpath->extend_info->identity_digest;
    tor_assert(n_chan_ident);
    int already_marked = 0;
    if (circ->base_.n_chan) {
      n_chan = circ->base_.n_chan;

      if (n_chan->is_bad_for_new_circs) {
        /* We only want to blame this router when a fresh healthy
         * connection fails. So don't mark this router as newly failed,
         * since maybe this was just an old circuit attempt that's
         * finally timing out now. Also, there's no need to blow away
         * circuits/streams/etc, since the failure of an unhealthy conn
         * doesn't tell us much about whether a healthy conn would
         * succeed. */
        already_marked = 1;
      }
      log_info(LD_OR,
               "Our circuit %u (id: %" PRIu32 ") failed to get a response "
               "from the first hop (%s). I'm going to try to rotate to a "
               "better connection.",
               TO_CIRCUIT(circ)->n_circ_id, circ->global_identifier,
               channel_get_canonical_remote_descr(n_chan));
      n_chan->is_bad_for_new_circs = 1;
    } else {
      log_info(LD_OR,
               "Our circuit %u (id: %" PRIu32 ") died before the first hop "
               "with no connection",
               TO_CIRCUIT(circ)->n_circ_id, circ->global_identifier);
    }
    if (!already_marked) {
      /*
       * If we have guard state (new guard API) and our path selection
       * code actually chose a full path, then blame the failure of this
       * circuit on the guard.
       */
      if (circ->guard_state)
        entry_guard_failed(&circ->guard_state);
      /* if there are any one-hop streams waiting on this circuit, fail
       * them now so they can retry elsewhere. */
      connection_ap_fail_onehop(n_chan_ident, circ->build_state);
    }
  }

  switch (circ->base_.purpose) {
    case CIRCUIT_PURPOSE_C_HSDIR_GET:
    case CIRCUIT_PURPOSE_S_HSDIR_POST:
    case CIRCUIT_PURPOSE_C_GENERAL:
      /* If we never built the circuit, note it as a failure. */
      circuit_increment_failure_count();
      if (failed_at_last_hop) {
        /* Make sure any streams that demand our last hop as their exit
         * know that it's unlikely to happen. */
        circuit_discard_optional_exit_enclaves(circ->cpath->prev->extend_info);
      }
      break;
    case CIRCUIT_PURPOSE_TESTING:
      circuit_testing_failed(circ, failed_at_last_hop);
      break;
    case CIRCUIT_PURPOSE_S_ESTABLISH_INTRO:
      /* at the service, waiting for introductions */
      if (circ->base_.state != CIRCUIT_STATE_OPEN) {
        circuit_increment_failure_count();
      }
      /* no need to care here, because the service will rebuild intro
       * points periodically. */
      break;
    case CIRCUIT_PURPOSE_C_INTRODUCING:
      /* at the client, connecting to intro point */
      /* Don't increment failure count, since the service may have picked
       * the introduction point maliciously */
      /* The client will pick a new intro point when this one dies, if
       * the stream in question still cares. No need to act here. */
      break;
    case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
      /* at the client, waiting for the service */
      circuit_increment_failure_count();
      /* the client will pick a new rend point when this one dies, if
       * the stream in question still cares. No need to act here. */
      break;
    case CIRCUIT_PURPOSE_S_CONNECT_REND:
      /* at the service, connecting to rend point */
      /* Don't increment failure count, since the client may have picked
       * the rendezvous point maliciously */
      log_info(LD_REND,
               "Couldn't connect to the client's chosen rend point %s "
               "(%s hop failed).",
               escaped(build_state_get_exit_nickname(circ->build_state)),
               failed_at_last_hop?"last":"non-last");
      hs_circ_retry_service_rendezvous_point(circ);
      break;
    /* default:
     * This won't happen in normal operation, but might happen if the
     * controller did it. Just let it slide. */
  }
}

/** Number of consecutive failures so far; should only be touched by
 * circuit_launch_new and circuit_*_failure_count.
 */
static int n_circuit_failures = 0;
/** Before the last time we called circuit_reset_failure_count(), were
 * there a lot of failures? */
static int did_circs_fail_last_period = 0;

/** Don't retry launching a new circuit if we try this many times with no
 * success. */
#define MAX_CIRCUIT_FAILURES 5

/** Launch a new circuit; see circuit_launch_by_extend_info() for
 * details on arguments. */
origin_circuit_t *
circuit_launch(uint8_t purpose, int flags)
{
  return circuit_launch_by_extend_info(purpose, NULL, flags);
}

/* Do we have enough descriptors to build paths?
 * If need_exit is true, return 1 if we can build exit paths.
 * (We need at least one Exit in the consensus to build exit paths.)
 * If need_exit is false, return 1 if we can build internal paths.
 */
static int
have_enough_path_info(int need_exit)
{
  if (need_exit)
    return router_have_consensus_path() == CONSENSUS_PATH_EXIT;
  else
    return router_have_consensus_path() != CONSENSUS_PATH_UNKNOWN;
}

/**
 * Tell us if a circuit is a hidden service circuit.
 */
int
circuit_purpose_is_hidden_service(uint8_t purpose)
{
   if (purpose == CIRCUIT_PURPOSE_HS_VANGUARDS) {
     return 1;
   }

   /* Client-side purpose */
   if (purpose >= CIRCUIT_PURPOSE_C_HS_MIN_ &&
       purpose <= CIRCUIT_PURPOSE_C_HS_MAX_) {
     return 1;
   }

   /* Service-side purpose */
   if (purpose >= CIRCUIT_PURPOSE_S_HS_MIN_ &&
       purpose <= CIRCUIT_PURPOSE_S_HS_MAX_) {
     return 1;
   }

   return 0;
}

/**
 * Return true if this circuit purpose should use vanguards
 * or pinned Layer2 or Layer3 guards.
 *
 * This function takes both the circuit purpose and the
 * torrc options for pinned middles/vanguards into account
 * (ie: the circuit must be a hidden service circuit and
 * vanguards/pinned middles must be enabled for it to return
 * true).
 */
int
circuit_should_use_vanguards(uint8_t purpose)
{
  const or_options_t *options = get_options();

  /* Only hidden service circuits use vanguards */
  if (!circuit_purpose_is_hidden_service(purpose))
    return 0;

  /* Pinned middles are effectively vanguards */
  if (options->HSLayer2Nodes || options->HSLayer3Nodes)
    return 1;

  return 0;
}

/**
 * Return true for the set of conditions for which it is OK to use
 * a cannibalized circuit.
 *
 * Don't cannibalize for onehops, or certain purposes.
 */
static int
circuit_should_cannibalize_to_build(uint8_t purpose_to_build,
                                    int has_extend_info,
                                    int onehop_tunnel)
{

  /* Do not try to cannibalize if this is a one hop circuit. */
  if (onehop_tunnel) {
    return 0;
  }

  /* Don't try to cannibalize for general purpose circuits that do not
   * specify a custom exit. */
  if (purpose_to_build == CIRCUIT_PURPOSE_C_GENERAL && !has_extend_info) {
    return 0;
  }

  /* Don't cannibalize for testing circuits. We want to see if they
   * complete normally. Also don't cannibalize for vanguard-purpose
   * circuits, since those are specially pre-built for later
   * cannibalization by the actual specific circuit types that need
   * vanguards.
   */
  if (purpose_to_build == CIRCUIT_PURPOSE_TESTING ||
      purpose_to_build == CIRCUIT_PURPOSE_HS_VANGUARDS) {
    return 0;
  }

  /* For vanguards, the server-side intro circ is not cannibalized
   * because we pre-build 4 hop HS circuits, and it only needs a 3 hop
   * circuit. It is also long-lived, so it is more important that
   * it have lower latency than get built fast.
   */
  if (circuit_should_use_vanguards(purpose_to_build) &&
      purpose_to_build == CIRCUIT_PURPOSE_S_ESTABLISH_INTRO) {
    return 0;
  }

  return 1;
}

/** Launch a new circuit with purpose <b>purpose</b> and exit node
 * <b>extend_info</b> (or NULL to select a random exit node).  If flags
 * contains CIRCLAUNCH_NEED_UPTIME, choose among routers with high uptime.  If
 * CIRCLAUNCH_NEED_CAPACITY is set, choose among routers with high bandwidth.
 * If CIRCLAUNCH_IS_INTERNAL is true, the last hop need not be an exit node.
 * If CIRCLAUNCH_ONEHOP_TUNNEL is set, the circuit will have only one hop.
 * Return the newly allocated circuit on success, or NULL on failure. */
origin_circuit_t *
circuit_launch_by_extend_info(uint8_t purpose,
                              extend_info_t *extend_info,
                              int flags)
{
  origin_circuit_t *circ;
  int onehop_tunnel = (flags & CIRCLAUNCH_ONEHOP_TUNNEL) != 0;
  int have_path = have_enough_path_info(! (flags & CIRCLAUNCH_IS_INTERNAL) );

  /* Keep some stats about our attempts to launch HS rendezvous circuits */
  if (purpose == CIRCUIT_PURPOSE_S_CONNECT_REND) {
    hs_stats_note_service_rendezvous_launch();
  }

  if (!onehop_tunnel && (!router_have_minimum_dir_info() || !have_path)) {
    log_debug(LD_CIRC,"Haven't %s yet; canceling "
              "circuit launch.",
              !router_have_minimum_dir_info() ?
              "fetched enough directory info" :
              "received a consensus with exits");
    return NULL;
  }

  /* If we can/should cannibalize another circuit to build this one,
   * then do so. */
  if (circuit_should_cannibalize_to_build(purpose,
                                          extend_info != NULL,
                                          onehop_tunnel)) {
    /* see if there are appropriate circs available to cannibalize. */
    /* XXX if we're planning to add a hop, perhaps we want to look for
     * internal circs rather than exit circs? -RD */
    circ = circuit_find_to_cannibalize(purpose, extend_info, flags);
    if (circ) {
      uint8_t old_purpose = circ->base_.purpose;
      struct timeval old_timestamp_began = circ->base_.timestamp_began;

      log_info(LD_CIRC, "Cannibalizing circ %u (id: %" PRIu32 ") for "
                        "purpose %d (%s)",
               TO_CIRCUIT(circ)->n_circ_id, circ->global_identifier, purpose,
               circuit_purpose_to_string(purpose));

      if ((purpose == CIRCUIT_PURPOSE_S_CONNECT_REND ||
           purpose == CIRCUIT_PURPOSE_C_INTRODUCING) &&
          circ->path_state == PATH_STATE_BUILD_SUCCEEDED) {
        /* Path bias: Cannibalized rends pre-emptively count as a
         * successfully built but unused closed circuit. We don't
         * wait until the extend (or the close) because the rend
         * point could be malicious.
         *
         * Same deal goes for client side introductions. Clients
         * can be manipulated to connect repeatedly to them
         * (especially web clients).
         *
         * If we decide to probe the initial portion of these circs,
         * (up to the adversary's final hop), we need to remove this,
         * or somehow mark the circuit with a special path state.
         */

        /* This must be called before the purpose change */
        pathbias_check_close(circ, END_CIRC_REASON_FINISHED);
      }

      circuit_change_purpose(TO_CIRCUIT(circ), purpose);
      /* Reset the start date of this circ, else expire_building
       * will see it and think it's been trying to build since it
       * began.
       *
       * Technically, the code should reset this when the
       * create cell is finally sent, but we're close enough
       * here. */
      tor_gettimeofday(&circ->base_.timestamp_began);

      control_event_circuit_cannibalized(circ, old_purpose,
                                         &old_timestamp_began);

      switch (purpose) {
        case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
          /* it's ready right now */
          break;
        case CIRCUIT_PURPOSE_C_INTRODUCING:
        case CIRCUIT_PURPOSE_S_CONNECT_REND:
        case CIRCUIT_PURPOSE_C_GENERAL:
        case CIRCUIT_PURPOSE_S_HSDIR_POST:
        case CIRCUIT_PURPOSE_C_HSDIR_GET:
        case CIRCUIT_PURPOSE_S_ESTABLISH_INTRO:
          /* need to add a new hop */
          tor_assert(extend_info);
          if (circuit_extend_to_new_exit(circ, extend_info) < 0)
            return NULL;
          break;
        default:
          log_warn(LD_BUG,
                   "unexpected purpose %d when cannibalizing a circ.",
                   purpose);
          tor_fragile_assert();
          return NULL;
      }
      return circ;
    }
  }

  if (did_circs_fail_last_period &&
      n_circuit_failures > MAX_CIRCUIT_FAILURES) {
    /* too many failed circs in a row. don't try. */
//    log_fn(LOG_INFO,"%d failures so far, not trying.",n_circuit_failures);
    return NULL;
  }

  /* try a circ. if it fails, circuit_mark_for_close will increment
   * n_circuit_failures */
  return circuit_establish_circuit(purpose, extend_info, flags);
}

/** Record another failure at opening a general circuit. When we have
 * too many, we'll stop trying for the remainder of this minute.
 */
static void
circuit_increment_failure_count(void)
{
  ++n_circuit_failures;
  log_debug(LD_CIRC,"n_circuit_failures now %d.",n_circuit_failures);
}

/** Reset the failure count for opening general circuits. This means
 * we will try MAX_CIRCUIT_FAILURES times more (if necessary) before
 * stopping again.
 */
void
circuit_reset_failure_count(int timeout)
{
  if (timeout && n_circuit_failures > MAX_CIRCUIT_FAILURES)
    did_circs_fail_last_period = 1;
  else
    did_circs_fail_last_period = 0;
  n_circuit_failures = 0;
}

/** Find an open circ that we're happy to use for <b>conn</b> and return 1. If
 * there isn't one, and there isn't one on the way, launch one and return
 * 0. If it will never work, return -1.
 *
 * Write the found or in-progress or launched circ into *circp.
 */
static int
circuit_get_open_circ_or_launch(entry_connection_t *conn,
                                uint8_t desired_circuit_purpose,
                                origin_circuit_t **circp)
{
  origin_circuit_t *circ;
  int check_exit_policy;
  int need_uptime, need_internal;
  int want_onehop;
  const or_options_t *options = get_options();

  tor_assert(conn);
  tor_assert(circp);
  if (ENTRY_TO_CONN(conn)->state != AP_CONN_STATE_CIRCUIT_WAIT) {
    connection_t *c = ENTRY_TO_CONN(conn);
    log_err(LD_BUG, "Connection state mismatch: wanted "
            "AP_CONN_STATE_CIRCUIT_WAIT, but got %d (%s)",
            c->state, conn_state_to_string(c->type, c->state));
  }
  tor_assert(ENTRY_TO_CONN(conn)->state == AP_CONN_STATE_CIRCUIT_WAIT);

  /* Will the exit policy of the exit node apply to this stream? */
  check_exit_policy =
      conn->socks_request->command == SOCKS_COMMAND_CONNECT &&
      !conn->use_begindir &&
      !connection_edge_is_rendezvous_stream(ENTRY_TO_EDGE_CONN(conn));

  /* Does this connection want a one-hop circuit? */
  want_onehop = conn->want_onehop;

  /* Do we need a high-uptime circuit? */
  need_uptime = !conn->want_onehop && !conn->use_begindir &&
                smartlist_contains_int_as_string(options->LongLivedPorts,
                                          conn->socks_request->port);

  /* Do we need an "internal" circuit? */
  if (desired_circuit_purpose != CIRCUIT_PURPOSE_C_GENERAL)
    need_internal = 1;
  else if (conn->use_begindir || conn->want_onehop)
    need_internal = 1;
  else
    need_internal = 0;

  /* We now know what kind of circuit we need.  See if there is an
   * open circuit that we can use for this stream */
  circ = circuit_get_best(conn, 1 /* Insist on open circuits */,
                          desired_circuit_purpose,
                          need_uptime, need_internal);

  if (circ) {
    /* We got a circuit that will work for this stream!  We can return it. */
    *circp = circ;
    return 1; /* we're happy */
  }

  /* Okay, there's no circuit open that will work for this stream. Let's
   * see if there's an in-progress circuit or if we have to launch one */

  /* Do we know enough directory info to build circuits at all? */
  int have_path = have_enough_path_info(!need_internal);

  if (!want_onehop && (!router_have_minimum_dir_info() || !have_path)) {
    /* If we don't have enough directory information, we can't build
     * multihop circuits.
     */
    if (!connection_get_by_type(CONN_TYPE_DIR)) {
      int severity = LOG_NOTICE;
      /* Retry some stuff that might help the connection work. */
      /* If we are configured with EntryNodes or UseBridges */
      if (entry_list_is_constrained(options)) {
        /* Retry all our guards / bridges.
         * guards_retry_optimistic() always returns true here. */
        int rv = guards_retry_optimistic(options);
        tor_assert_nonfatal_once(rv);
        log_fn(severity, LD_APP|LD_DIR,
               "Application request when we haven't %s. "
               "Optimistically trying known %s again.",
               !router_have_minimum_dir_info() ?
               "used client functionality lately" :
               "received a consensus with exits",
               options->UseBridges ? "bridges" : "entrynodes");
      } else {
        /* Getting directory documents doesn't help much if we have a limited
         * number of guards */
        tor_assert_nonfatal(!options->UseBridges);
        tor_assert_nonfatal(!options->EntryNodes);
        /* Retry our directory fetches, so we have a fresh set of guard info */
        log_fn(severity, LD_APP|LD_DIR,
               "Application request when we haven't %s. "
               "Optimistically trying directory fetches again.",
               !router_have_minimum_dir_info() ?
               "used client functionality lately" :
               "received a consensus with exits");
        routerlist_retry_directory_downloads(time(NULL));
      }
    }
    /* Since we didn't have enough directory info, we can't attach now.  The
     * stream will be dealt with when router_have_minimum_dir_info becomes 1,
     * or when all directory attempts fail and directory_all_unreachable()
     * kills it.
     */
    return 0;
  }

  /* Check whether the exit policy of the chosen exit, or the exit policies
   * of _all_ nodes, would forbid this node. */
  if (check_exit_policy) {
    if (!conn->chosen_exit_name) {
      struct in_addr in;
      tor_addr_t addr, *addrp=NULL;
      if (tor_inet_aton(conn->socks_request->address, &in)) {
        tor_addr_from_in(&addr, &in);
        addrp = &addr;
      }
      if (router_exit_policy_all_nodes_reject(addrp,
                                              conn->socks_request->port,
                                              need_uptime)) {
        log_notice(LD_APP,
                   "No Tor server allows exit to %s:%d. Rejecting.",
                   safe_str_client(conn->socks_request->address),
                   conn->socks_request->port);
        return -1;
      }
    } else {
      /* XXXX Duplicates checks in connection_ap_handshake_attach_circuit:
       * refactor into a single function. */
      const node_t *node = node_get_by_nickname(conn->chosen_exit_name, 0);
      int opt = conn->chosen_exit_optional;
      if (node && !connection_ap_can_use_exit(conn, node)) {
        log_fn(opt ? LOG_INFO : LOG_WARN, LD_APP,
               "Requested exit point '%s' is excluded or "
               "would refuse request. %s.",
               conn->chosen_exit_name, opt ? "Trying others" : "Closing");
        if (opt) {
          conn->chosen_exit_optional = 0;
          tor_free(conn->chosen_exit_name);
          /* Try again. */
          return circuit_get_open_circ_or_launch(conn,
                                                 desired_circuit_purpose,
                                                 circp);
        }
        return -1;
      }
    }
  }

  /* Now, check whether there already a circuit on the way that could handle
   * this stream. This check matches the one above, but this time we
   * do not require that the circuit will work. */
  circ = circuit_get_best(conn, 0 /* don't insist on open circuits */,
                          desired_circuit_purpose,
                          need_uptime, need_internal);
  if (circ)
    log_debug(LD_CIRC, "one on the way!");

  if (!circ) {
    /* No open or in-progress circuit could handle this stream!  We
     * will have to launch one!
     */

    /* The chosen exit node, if there is one. */
    extend_info_t *extend_info=NULL;
    const int n_pending = count_pending_general_client_circuits();

    /* Do we have too many pending circuits? */
    if (n_pending >= options->MaxClientCircuitsPending) {
      static ratelim_t delay_limit = RATELIM_INIT(10*60);
      char *m;
      if ((m = rate_limit_log(&delay_limit, approx_time()))) {
        log_notice(LD_APP, "We'd like to launch a circuit to handle a "
                   "connection, but we already have %d general-purpose client "
                   "circuits pending. Waiting until some finish.%s",
                   n_pending, m);
        tor_free(m);
      }
      return 0;
    }

    /* If this is a hidden service trying to start an introduction point,
     * handle that case. */
    if (desired_circuit_purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT) {
      const edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(conn);
      /* need to pick an intro point */
      extend_info = hs_client_get_random_intro_from_edge(edge_conn);
      if (!extend_info) {
        log_info(LD_REND, "No intro points: re-fetching service descriptor.");
        if (edge_conn->rend_data) {
          rend_client_refetch_v2_renddesc(edge_conn->rend_data);
        } else {
          hs_client_refetch_hsdesc(&edge_conn->hs_ident->identity_pk);
        }
        connection_ap_mark_as_waiting_for_renddesc(conn);
        return 0;
      }
      log_info(LD_REND,"Chose %s as intro point for '%s'.",
               extend_info_describe(extend_info),
               (edge_conn->rend_data) ?
               safe_str_client(rend_data_get_address(edge_conn->rend_data)) :
               "service");
    }

    /* If we have specified a particular exit node for our
     * connection, then be sure to open a circuit to that exit node.
     */
    if (desired_circuit_purpose == CIRCUIT_PURPOSE_C_GENERAL ||
        desired_circuit_purpose == CIRCUIT_PURPOSE_S_HSDIR_POST ||
        desired_circuit_purpose == CIRCUIT_PURPOSE_C_HSDIR_GET) {
      if (conn->chosen_exit_name) {
        const node_t *r;
        int opt = conn->chosen_exit_optional;
        r = node_get_by_nickname(conn->chosen_exit_name, 0);
        if (r && node_has_preferred_descriptor(r, conn->want_onehop ? 1 : 0)) {
          /* We might want to connect to an IPv6 bridge for loading
             descriptors so we use the preferred address rather than
             the primary. */
          extend_info = extend_info_from_node(r, conn->want_onehop ? 1 : 0);
          if (!extend_info) {
            log_warn(LD_CIRC,"Could not make a one-hop connection to %s. "
                     "Discarding this circuit.", conn->chosen_exit_name);
            return -1;
          }
        } else  { /* ! (r && node_has_preferred_descriptor(...)) */
          log_debug(LD_DIR, "considering %d, %s",
                    want_onehop, conn->chosen_exit_name);
          if (want_onehop && conn->chosen_exit_name[0] == '$') {
            /* We're asking for a one-hop circuit to a router that
             * we don't have a routerinfo about. Make up an extend_info. */
            /* XXX prop220: we need to make chosen_exit_name able to
             * encode both key formats. This is not absolutely critical
             * since this is just for one-hop circuits, but we should
             * still get it done */
            char digest[DIGEST_LEN];
            char *hexdigest = conn->chosen_exit_name+1;
            tor_addr_t addr;
            if (strlen(hexdigest) < HEX_DIGEST_LEN ||
                base16_decode(digest,DIGEST_LEN,
                              hexdigest,HEX_DIGEST_LEN) != DIGEST_LEN) {
              log_info(LD_DIR, "Broken exit digest on tunnel conn. Closing.");
              return -1;
            }
            if (tor_addr_parse(&addr, conn->socks_request->address) < 0) {
              log_info(LD_DIR, "Broken address %s on tunnel conn. Closing.",
                       escaped_safe_str_client(conn->socks_request->address));
              return -1;
            }
            /* XXXX prop220 add a workaround for ed25519 ID below*/
            extend_info = extend_info_new(conn->chosen_exit_name+1,
                                          digest,
                                          NULL, /* Ed25519 ID */
                                          NULL, NULL, /* onion keys */
                                          &addr, conn->socks_request->port);
          } else { /* ! (want_onehop && conn->chosen_exit_name[0] == '$') */
            /* We will need an onion key for the router, and we
             * don't have one. Refuse or relax requirements. */
            log_fn(opt ? LOG_INFO : LOG_WARN, LD_APP,
                   "Requested exit point '%s' is not known. %s.",
                   conn->chosen_exit_name, opt ? "Trying others" : "Closing");
            if (opt) {
              conn->chosen_exit_optional = 0;
              tor_free(conn->chosen_exit_name);
              /* Try again with no requested exit */
              return circuit_get_open_circ_or_launch(conn,
                                                     desired_circuit_purpose,
                                                     circp);
            }
            return -1;
          }
        }
      }
    } /* Done checking for general circutis with chosen exits. */

    /* What purpose do we need to launch this circuit with? */
    uint8_t new_circ_purpose;
    if (desired_circuit_purpose == CIRCUIT_PURPOSE_C_REND_JOINED)
      new_circ_purpose = CIRCUIT_PURPOSE_C_ESTABLISH_REND;
    else if (desired_circuit_purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT)
      new_circ_purpose = CIRCUIT_PURPOSE_C_INTRODUCING;
    else
      new_circ_purpose = desired_circuit_purpose;

    /* Determine what kind of a circuit to launch, and actually launch it. */
    {
      int flags = CIRCLAUNCH_NEED_CAPACITY;
      if (want_onehop) flags |= CIRCLAUNCH_ONEHOP_TUNNEL;
      if (need_uptime) flags |= CIRCLAUNCH_NEED_UPTIME;
      if (need_internal) flags |= CIRCLAUNCH_IS_INTERNAL;

      /* If we are about to pick a v3 RP right now, make sure we pick a
       * rendezvous point that supports the v3 protocol! */
      if (desired_circuit_purpose == CIRCUIT_PURPOSE_C_REND_JOINED &&
          new_circ_purpose == CIRCUIT_PURPOSE_C_ESTABLISH_REND &&
          ENTRY_TO_EDGE_CONN(conn)->hs_ident) {
        flags |= CIRCLAUNCH_IS_V3_RP;
        log_info(LD_GENERAL, "Getting rendezvous circuit to v3 service!");
      }

      circ = circuit_launch_by_extend_info(new_circ_purpose, extend_info,
                                           flags);
    }

    extend_info_free(extend_info);

    /* Now trigger things that need to happen when we launch circuits */

    if (desired_circuit_purpose == CIRCUIT_PURPOSE_C_GENERAL ||
        desired_circuit_purpose == CIRCUIT_PURPOSE_C_HSDIR_GET ||
        desired_circuit_purpose == CIRCUIT_PURPOSE_S_HSDIR_POST) {
      /* We just caused a circuit to get built because of this stream.
       * If this stream has caused a _lot_ of circuits to be built, that's
       * a bad sign: we should tell the user. */
      if (conn->num_circuits_launched < NUM_CIRCUITS_LAUNCHED_THRESHOLD &&
          ++conn->num_circuits_launched == NUM_CIRCUITS_LAUNCHED_THRESHOLD)
        log_info(LD_CIRC, "The application request to %s:%d has launched "
                 "%d circuits without finding one it likes.",
                 escaped_safe_str_client(conn->socks_request->address),
                 conn->socks_request->port,
                 conn->num_circuits_launched);
    } else {
      /* help predict this next time */
      rep_hist_note_used_internal(time(NULL), need_uptime, 1);
      if (circ) {
        const edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(conn);
        if (edge_conn->rend_data) {
          /* write the service_id into circ */
          circ->rend_data = rend_data_dup(edge_conn->rend_data);
        } else if (edge_conn->hs_ident) {
          circ->hs_ident =
            hs_ident_circuit_new(&edge_conn->hs_ident->identity_pk,
                                 HS_IDENT_CIRCUIT_INTRO);
        }
        if (circ->base_.purpose == CIRCUIT_PURPOSE_C_ESTABLISH_REND &&
            circ->base_.state == CIRCUIT_STATE_OPEN)
          circuit_has_opened(circ);
      }
    }
  } /* endif (!circ) */

  /* We either found a good circuit, or launched a new circuit, or failed to
   * do so. Report success, and delay. */

  if (circ) {
    /* Mark the circuit with the isolation fields for this connection.
     * When the circuit arrives, we'll clear these flags: this is
     * just some internal bookkeeping to make sure that we have
     * launched enough circuits.
     */
    connection_edge_update_circuit_isolation(conn, circ, 0);
  } else {
    log_info(LD_APP,
             "No safe circuit (purpose %d) ready for edge "
             "connection; delaying.",
             desired_circuit_purpose);
  }
  *circp = circ;
  return 0;
}

/** Return true iff <b>crypt_path</b> is one of the crypt_paths for
 * <b>circ</b>. */
static int
cpath_is_on_circuit(origin_circuit_t *circ, crypt_path_t *crypt_path)
{
  crypt_path_t *cpath, *cpath_next = NULL;
  for (cpath = circ->cpath; cpath_next != circ->cpath; cpath = cpath_next) {
    cpath_next = cpath->next;
    if (crypt_path == cpath)
      return 1;
  }
  return 0;
}

/** Return true iff client-side optimistic data is supported. */
static int
optimistic_data_enabled(void)
{
  const or_options_t *options = get_options();
  if (options->OptimisticData < 0) {
    /* Note: this default was 0 before #18815 was merged. We can't take the
     * parameter out of the consensus until versions before that are all
     * obsolete. */
    const int32_t enabled =
      networkstatus_get_param(NULL, "UseOptimisticData", /*default*/ 1, 0, 1);
    return (int)enabled;
  }
  return options->OptimisticData;
}

/** Attach the AP stream <b>apconn</b> to circ's linked list of
 * p_streams. Also set apconn's cpath_layer to <b>cpath</b>, or to the last
 * hop in circ's cpath if <b>cpath</b> is NULL.
 */
static void
link_apconn_to_circ(entry_connection_t *apconn, origin_circuit_t *circ,
                    crypt_path_t *cpath)
{
  const node_t *exitnode = NULL;

  /* add it into the linked list of streams on this circuit */
  log_debug(LD_APP|LD_CIRC, "attaching new conn to circ. n_circ_id %u.",
            (unsigned)circ->base_.n_circ_id);
  /* reset it, so we can measure circ timeouts */
  ENTRY_TO_CONN(apconn)->timestamp_last_read_allowed = time(NULL);
  ENTRY_TO_EDGE_CONN(apconn)->next_stream = circ->p_streams;
  ENTRY_TO_EDGE_CONN(apconn)->on_circuit = TO_CIRCUIT(circ);
  /* assert_connection_ok(conn, time(NULL)); */
  circ->p_streams = ENTRY_TO_EDGE_CONN(apconn);

  if (connection_edge_is_rendezvous_stream(ENTRY_TO_EDGE_CONN(apconn))) {
    /* We are attaching a stream to a rendezvous circuit.  That means
     * that an attempt to connect to a hidden service just
     * succeeded.  Tell rendclient.c. */
    hs_client_note_connection_attempt_succeeded(ENTRY_TO_EDGE_CONN(apconn));
  }

  if (cpath) { /* we were given one; use it */
    tor_assert(cpath_is_on_circuit(circ, cpath));
  } else {
    /* use the last hop in the circuit */
    tor_assert(circ->cpath);
    tor_assert(circ->cpath->prev);
    tor_assert(circ->cpath->prev->state == CPATH_STATE_OPEN);
    cpath = circ->cpath->prev;
  }
  ENTRY_TO_EDGE_CONN(apconn)->cpath_layer = cpath;

  circ->isolation_any_streams_attached = 1;
  connection_edge_update_circuit_isolation(apconn, circ, 0);

  /* Compute the exitnode if possible, for logging below */
  if (cpath->extend_info)
    exitnode = node_get_by_id(cpath->extend_info->identity_digest);

  /* See if we can use optimistic data on this circuit */
  if (optimistic_data_enabled() &&
      (circ->base_.purpose == CIRCUIT_PURPOSE_C_GENERAL ||
       circ->base_.purpose == CIRCUIT_PURPOSE_C_HSDIR_GET ||
       circ->base_.purpose == CIRCUIT_PURPOSE_S_HSDIR_POST ||
       circ->base_.purpose == CIRCUIT_PURPOSE_C_REND_JOINED))
    apconn->may_use_optimistic_data = 1;
  else
    apconn->may_use_optimistic_data = 0;
  log_info(LD_APP, "Looks like completed circuit to %s %s allow "
           "optimistic data for connection to %s",
           circ->base_.purpose == CIRCUIT_PURPOSE_C_GENERAL ?
             /* node_describe() does the right thing if exitnode is NULL */
             safe_str_client(node_describe(exitnode)) :
             "hidden service",
           apconn->may_use_optimistic_data ? "does" : "doesn't",
           safe_str_client(apconn->socks_request->address));
}

/** Return true iff <b>address</b> is matched by one of the entries in
 * TrackHostExits. */
int
hostname_in_track_host_exits(const or_options_t *options, const char *address)
{
  if (!options->TrackHostExits)
    return 0;
  SMARTLIST_FOREACH_BEGIN(options->TrackHostExits, const char *, cp) {
    if (cp[0] == '.') { /* match end */
      if (cp[1] == '\0' ||
          !strcasecmpend(address, cp) ||
          !strcasecmp(address, &cp[1]))
        return 1;
    } else if (strcasecmp(cp, address) == 0) {
      return 1;
    }
  } SMARTLIST_FOREACH_END(cp);
  return 0;
}

/** If an exit wasn't explicitly specified for <b>conn</b>, consider saving
 * the exit that we *did* choose for use by future connections to
 * <b>conn</b>'s destination.
 */
static void
consider_recording_trackhost(const entry_connection_t *conn,
                             const origin_circuit_t *circ)
{
  const or_options_t *options = get_options();
  char *new_address = NULL;
  char fp[HEX_DIGEST_LEN+1];

  /* Search the addressmap for this conn's destination. */
  /* If they're not in the address map.. */
  if (!options->TrackHostExits ||
      addressmap_have_mapping(conn->socks_request->address,
                              options->TrackHostExitsExpire))
    return; /* nothing to track, or already mapped */

  if (!hostname_in_track_host_exits(options, conn->socks_request->address) ||
      !circ->build_state->chosen_exit)
    return;

  /* write down the fingerprint of the chosen exit, not the nickname,
   * because the chosen exit might not be named. */
  base16_encode(fp, sizeof(fp),
                circ->build_state->chosen_exit->identity_digest, DIGEST_LEN);

  /* Add this exit/hostname pair to the addressmap. */
  tor_asprintf(&new_address, "%s.%s.exit",
               conn->socks_request->address, fp);

  addressmap_register(conn->socks_request->address, new_address,
                      time(NULL) + options->TrackHostExitsExpire,
                      ADDRMAPSRC_TRACKEXIT, 0, 0);
}

/** Attempt to attach the connection <b>conn</b> to <b>circ</b>, and send a
 * begin or resolve cell as appropriate.  Return values are as for
 * connection_ap_handshake_attach_circuit.  The stream will exit from the hop
 * indicated by <b>cpath</b>, or from the last hop in circ's cpath if
 * <b>cpath</b> is NULL. */
int
connection_ap_handshake_attach_chosen_circuit(entry_connection_t *conn,
                                              origin_circuit_t *circ,
                                              crypt_path_t *cpath)
{
  connection_t *base_conn = ENTRY_TO_CONN(conn);
  tor_assert(conn);
  tor_assert(base_conn->state == AP_CONN_STATE_CIRCUIT_WAIT ||
             base_conn->state == AP_CONN_STATE_CONTROLLER_WAIT);
  tor_assert(conn->socks_request);
  tor_assert(circ);
  tor_assert(circ->base_.state == CIRCUIT_STATE_OPEN);

  base_conn->state = AP_CONN_STATE_CIRCUIT_WAIT;

  if (!circ->base_.timestamp_dirty ||
      ((conn->entry_cfg.isolation_flags & ISO_SOCKSAUTH) &&
       (conn->entry_cfg.socks_iso_keep_alive) &&
       (conn->socks_request->usernamelen ||
        conn->socks_request->passwordlen))) {
    /* When stream isolation is in use and controlled by an application
     * we are willing to keep using the stream. */
    circ->base_.timestamp_dirty = approx_time();
  }

  pathbias_count_use_attempt(circ);

  /* Now, actually link the connection. */
  link_apconn_to_circ(conn, circ, cpath);

  tor_assert(conn->socks_request);
  if (conn->socks_request->command == SOCKS_COMMAND_CONNECT) {
    if (!conn->use_begindir)
      consider_recording_trackhost(conn, circ);
    if (connection_ap_handshake_send_begin(conn) < 0)
      return -1;
  } else {
    if (connection_ap_handshake_send_resolve(conn) < 0)
      return -1;
  }

  return 1;
}

/**
 * Return an appropriate circuit purpose for non-rend streams.
 * We don't handle rends here because a rend stream triggers two
 * circuit builds with different purposes, so it is handled elsewhere.
 *
 * This function just figures out what type of hsdir activity this is,
 * and tells us. Everything else is general.
 */
static int
connection_ap_get_nonrend_circ_purpose(const entry_connection_t *conn)
{
  const connection_t *base_conn = ENTRY_TO_CONN(conn);
  tor_assert_nonfatal(!connection_edge_is_rendezvous_stream(
                      ENTRY_TO_EDGE_CONN(conn)));

  if (base_conn->linked_conn &&
      base_conn->linked_conn->type == CONN_TYPE_DIR) {
    /* Set a custom purpose for hsdir activity */
    if (base_conn->linked_conn->purpose == DIR_PURPOSE_UPLOAD_RENDDESC_V2 ||
       base_conn->linked_conn->purpose == DIR_PURPOSE_UPLOAD_HSDESC) {
      return CIRCUIT_PURPOSE_S_HSDIR_POST;
    } else if (base_conn->linked_conn->purpose
                 == DIR_PURPOSE_FETCH_RENDDESC_V2 ||
               base_conn->linked_conn->purpose
                 == DIR_PURPOSE_FETCH_HSDESC) {
      return CIRCUIT_PURPOSE_C_HSDIR_GET;
    }
  }

  /* All other purposes are general for now */
  return CIRCUIT_PURPOSE_C_GENERAL;
}

/** Try to find a safe live circuit for stream <b>conn</b>.  If we find one,
 * attach the stream, send appropriate cells, and return 1.  Otherwise,
 * try to launch new circuit(s) for the stream.  If we can launch
 * circuits, return 0.  Otherwise, if we simply can't proceed with
 * this stream, return -1. (conn needs to die, and is maybe already marked).
 */
/* XXXX this function should mark for close whenever it returns -1;
 * its callers shouldn't have to worry about that. */
int
connection_ap_handshake_attach_circuit(entry_connection_t *conn)
{
  connection_t *base_conn = ENTRY_TO_CONN(conn);
  int retval;
  int conn_age;
  int want_onehop;

  tor_assert(conn);
  tor_assert(base_conn->state == AP_CONN_STATE_CIRCUIT_WAIT);
  tor_assert(conn->socks_request);
  want_onehop = conn->want_onehop;

  conn_age = (int)(time(NULL) - base_conn->timestamp_created);

  /* Is this connection so old that we should give up on it? */
  if (conn_age >= get_options()->SocksTimeout) {
    int severity = (tor_addr_is_null(&base_conn->addr) && !base_conn->port) ?
      LOG_INFO : LOG_NOTICE;
    log_fn(severity, LD_APP,
           "Tried for %d seconds to get a connection to %s:%d. Giving up.",
           conn_age, safe_str_client(conn->socks_request->address),
           conn->socks_request->port);
    return -1;
  }

  /* We handle "general" (non-onion) connections much more straightforwardly.
   */
  if (!connection_edge_is_rendezvous_stream(ENTRY_TO_EDGE_CONN(conn))) {
    /* we're a general conn */
    origin_circuit_t *circ=NULL;

    /* Are we linked to a dir conn that aims to fetch a consensus?
     * We check here because the conn might no longer be needed. */
    if (base_conn->linked_conn &&
        base_conn->linked_conn->type == CONN_TYPE_DIR &&
        base_conn->linked_conn->purpose == DIR_PURPOSE_FETCH_CONSENSUS) {

      /* Yes we are. Is there a consensus fetch farther along than us? */
      if (networkstatus_consensus_is_already_downloading(
            TO_DIR_CONN(base_conn->linked_conn)->requested_resource)) {
        /* We're doing the "multiple consensus fetch attempts" game from
         * proposal 210, and we're late to the party. Just close this conn.
         * The circuit and TLS conn that we made will time out after a while
         * if nothing else wants to use them. */
        log_info(LD_DIR, "Closing extra consensus fetch (to %s) since one "
                 "is already downloading.", base_conn->linked_conn->address);
        return -1;
      }
    }

    /* If we have a chosen exit, we need to use a circuit that's
     * open to that exit. See what exit we meant, and whether we can use it.
     */
    if (conn->chosen_exit_name) {
      const node_t *node = node_get_by_nickname(conn->chosen_exit_name, 0);
      int opt = conn->chosen_exit_optional;
      if (!node && !want_onehop) {
        /* We ran into this warning when trying to extend a circuit to a
         * hidden service directory for which we didn't have a router
         * descriptor. See flyspray task 767 for more details. We should
         * keep this in mind when deciding to use BEGIN_DIR cells for other
         * directory requests as well. -KL*/
        log_fn(opt ? LOG_INFO : LOG_WARN, LD_APP,
               "Requested exit point '%s' is not known. %s.",
               conn->chosen_exit_name, opt ? "Trying others" : "Closing");
        if (opt) {
          /* If we are allowed to ignore the .exit request, do so */
          conn->chosen_exit_optional = 0;
          tor_free(conn->chosen_exit_name);
          return 0;
        }
        return -1;
      }
      if (node && !connection_ap_can_use_exit(conn, node)) {
        log_fn(opt ? LOG_INFO : LOG_WARN, LD_APP,
               "Requested exit point '%s' is excluded or "
               "would refuse request. %s.",
               conn->chosen_exit_name, opt ? "Trying others" : "Closing");
        if (opt) {
          /* If we are allowed to ignore the .exit request, do so */
          conn->chosen_exit_optional = 0;
          tor_free(conn->chosen_exit_name);
          return 0;
        }
        return -1;
      }
    }

    /* Find the circuit that we should use, if there is one. Otherwise
     * launch it
     */
    retval = circuit_get_open_circ_or_launch(conn,
            connection_ap_get_nonrend_circ_purpose(conn),
            &circ);

    if (retval < 1) {
      /* We were either told "-1" (complete failure) or 0 (circuit in
       * progress); we can't attach this stream yet. */
      return retval;
    }

    log_debug(LD_APP|LD_CIRC,
              "Attaching apconn to circ %u (stream %d sec old).",
              (unsigned)circ->base_.n_circ_id, conn_age);
    /* print the circ's path, so clients can figure out which circs are
     * sucking. */
    circuit_log_path(LOG_INFO,LD_APP|LD_CIRC,circ);

    /* We have found a suitable circuit for our conn. Hurray.  Do
     * the attachment. */
    return connection_ap_handshake_attach_chosen_circuit(conn, circ, NULL);

  } else { /* we're a rendezvous conn */
    origin_circuit_t *rendcirc=NULL, *introcirc=NULL;

    tor_assert(!ENTRY_TO_EDGE_CONN(conn)->cpath_layer);

    /* start by finding a rendezvous circuit for us */

    retval = circuit_get_open_circ_or_launch(
       conn, CIRCUIT_PURPOSE_C_REND_JOINED, &rendcirc);
    if (retval < 0) return -1; /* failed */

    if (retval > 0) {
      tor_assert(rendcirc);
      /* one is already established, attach */
      log_info(LD_REND,
               "rend joined circ %u (id: %" PRIu32 ") already here. "
               "Attaching. (stream %d sec old)",
               (unsigned) TO_CIRCUIT(rendcirc)->n_circ_id,
               rendcirc->global_identifier, conn_age);
      /* Mark rendezvous circuits as 'newly dirty' every time you use
       * them, since the process of rebuilding a rendezvous circ is so
       * expensive. There is a tradeoff between linkability and
       * feasibility, at this point.
       */
      rendcirc->base_.timestamp_dirty = time(NULL);

      /* We've also attempted to use them. If they fail, we need to
       * probe them for path bias */
      pathbias_count_use_attempt(rendcirc);

      link_apconn_to_circ(conn, rendcirc, NULL);
      if (connection_ap_handshake_send_begin(conn) < 0)
        return 0; /* already marked, let them fade away */
      return 1;
    }

    /* At this point we need to re-check the state, since it's possible that
     * our call to circuit_get_open_circ_or_launch() changed the connection's
     * state from "CIRCUIT_WAIT" to "RENDDESC_WAIT" because we decided to
     * re-fetch the descriptor.
     */
    if (ENTRY_TO_CONN(conn)->state != AP_CONN_STATE_CIRCUIT_WAIT) {
      log_info(LD_REND, "This connection is no longer ready to attach; its "
               "state changed."
               "(We probably have to re-fetch its descriptor.)");
      return 0;
    }

    if (rendcirc && (rendcirc->base_.purpose ==
                     CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED)) {
      log_info(LD_REND,
               "pending-join circ %u (id: %" PRIu32 ") already here, with "
               "intro ack. Stalling. (stream %d sec old)",
               (unsigned) TO_CIRCUIT(rendcirc)->n_circ_id,
               rendcirc->global_identifier, conn_age);
      return 0;
    }

    /* it's on its way. find an intro circ. */
    retval = circuit_get_open_circ_or_launch(
      conn, CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT, &introcirc);
    if (retval < 0) return -1; /* failed */

    if (retval > 0) {
      /* one has already sent the intro. keep waiting. */
      tor_assert(introcirc);
      log_info(LD_REND, "Intro circ %u (id: %" PRIu32 ") present and "
                        "awaiting ACK. Rend circuit %u (id: %" PRIu32 "). "
                        "Stalling. (stream %d sec old)",
               (unsigned) TO_CIRCUIT(introcirc)->n_circ_id,
               introcirc->global_identifier,
               rendcirc ? (unsigned) TO_CIRCUIT(rendcirc)->n_circ_id : 0,
               rendcirc ? rendcirc->global_identifier : 0,
               conn_age);
      return 0;
    }

    /* now rendcirc and introcirc are each either undefined or not finished */

    if (rendcirc && introcirc &&
        rendcirc->base_.purpose == CIRCUIT_PURPOSE_C_REND_READY) {
      log_info(LD_REND,
               "ready rend circ %u (id: %" PRIu32 ") already here. No"
               "intro-ack yet on intro %u (id: %" PRIu32 "). "
               "(stream %d sec old)",
               (unsigned) TO_CIRCUIT(rendcirc)->n_circ_id,
               rendcirc->global_identifier,
               (unsigned) TO_CIRCUIT(introcirc)->n_circ_id,
               introcirc->global_identifier, conn_age);

      tor_assert(introcirc->base_.purpose == CIRCUIT_PURPOSE_C_INTRODUCING);
      if (introcirc->base_.state == CIRCUIT_STATE_OPEN) {
        int ret;
        log_info(LD_REND, "Found open intro circ %u (id: %" PRIu32 "). "
                          "Rend circuit %u (id: %" PRIu32 "); Sending "
                          "introduction. (stream %d sec old)",
                 (unsigned) TO_CIRCUIT(introcirc)->n_circ_id,
                 introcirc->global_identifier,
                 (unsigned) TO_CIRCUIT(rendcirc)->n_circ_id,
                 rendcirc->global_identifier, conn_age);
        ret = hs_client_send_introduce1(introcirc, rendcirc);
        switch (ret) {
        case 0: /* success */
          rendcirc->base_.timestamp_dirty = time(NULL);
          introcirc->base_.timestamp_dirty = time(NULL);

          pathbias_count_use_attempt(introcirc);
          pathbias_count_use_attempt(rendcirc);

          assert_circuit_ok(TO_CIRCUIT(rendcirc));
          assert_circuit_ok(TO_CIRCUIT(introcirc));
          return 0;
        case -1: /* transient error */
          return 0;
        case -2: /* permanent error */
          return -1;
        default: /* oops */
          tor_fragile_assert();
          return -1;
        }
      }
    }

    log_info(LD_REND, "Intro %u (id: %" PRIu32 ") and rend circuit %u "
                      "(id: %" PRIu32 ") circuits are not both ready. "
                      "Stalling conn. (%d sec old)",
             introcirc ? (unsigned) TO_CIRCUIT(introcirc)->n_circ_id : 0,
             introcirc ? introcirc->global_identifier : 0,
             rendcirc ? (unsigned) TO_CIRCUIT(rendcirc)->n_circ_id : 0,
             rendcirc ? rendcirc->global_identifier : 0, conn_age);
    return 0;
  }
}

/** Change <b>circ</b>'s purpose to <b>new_purpose</b>. */
void
circuit_change_purpose(circuit_t *circ, uint8_t new_purpose)
{
  uint8_t old_purpose;
  /* Don't allow an OR circ to become an origin circ or vice versa. */
  tor_assert(!!(CIRCUIT_IS_ORIGIN(circ)) ==
             !!(CIRCUIT_PURPOSE_IS_ORIGIN(new_purpose)));

  if (circ->purpose == new_purpose) return;

  if (CIRCUIT_IS_ORIGIN(circ)) {
    char old_purpose_desc[80] = "";

    strncpy(old_purpose_desc, circuit_purpose_to_string(circ->purpose), 80-1);
    old_purpose_desc[80-1] = '\0';

    log_debug(LD_CIRC,
              "changing purpose of origin circ %d "
              "from \"%s\" (%d) to \"%s\" (%d)",
              TO_ORIGIN_CIRCUIT(circ)->global_identifier,
              old_purpose_desc,
              circ->purpose,
              circuit_purpose_to_string(new_purpose),
              new_purpose);
  }

  old_purpose = circ->purpose;
  circ->purpose = new_purpose;

  if (CIRCUIT_IS_ORIGIN(circ)) {
    control_event_circuit_purpose_changed(TO_ORIGIN_CIRCUIT(circ),
                                          old_purpose);
  }
}

/** Mark <b>circ</b> so that no more connections can be attached to it. */
void
mark_circuit_unusable_for_new_conns(origin_circuit_t *circ)
{
  const or_options_t *options = get_options();
  tor_assert(circ);

  /* XXXX This is a kludge; we're only keeping it around in case there's
   * something that doesn't check unusable_for_new_conns, and to avoid
   * deeper refactoring of our expiration logic. */
  if (! circ->base_.timestamp_dirty)
    circ->base_.timestamp_dirty = approx_time();
  if (options->MaxCircuitDirtiness >= circ->base_.timestamp_dirty)
    circ->base_.timestamp_dirty = 1; /* prevent underflow */
  else
    circ->base_.timestamp_dirty -= options->MaxCircuitDirtiness;

  circ->unusable_for_new_conns = 1;
}

/**
 * Add relay_body_len and RELAY_PAYLOAD_SIZE-relay_body_len to
 * the valid delivered written fields and the overhead field,
 * respectively.
 */
void
circuit_sent_valid_data(origin_circuit_t *circ, uint16_t relay_body_len)
{
  if (!circ) return;

  tor_assert_nonfatal(relay_body_len <= RELAY_PAYLOAD_SIZE);

  circ->n_delivered_written_circ_bw =
      tor_add_u32_nowrap(circ->n_delivered_written_circ_bw, relay_body_len);
  circ->n_overhead_written_circ_bw =
      tor_add_u32_nowrap(circ->n_overhead_written_circ_bw,
                         RELAY_PAYLOAD_SIZE-relay_body_len);
}

/**
 * Add relay_body_len and RELAY_PAYLOAD_SIZE-relay_body_len to
 * the valid delivered read field and the overhead field,
 * respectively.
 */
void
circuit_read_valid_data(origin_circuit_t *circ, uint16_t relay_body_len)
{
  if (!circ) return;

  tor_assert_nonfatal(relay_body_len <= RELAY_PAYLOAD_SIZE);

  circ->n_delivered_read_circ_bw =
      tor_add_u32_nowrap(circ->n_delivered_read_circ_bw, relay_body_len);
  circ->n_overhead_read_circ_bw =
      tor_add_u32_nowrap(circ->n_overhead_read_circ_bw,
                         RELAY_PAYLOAD_SIZE-relay_body_len);
}
