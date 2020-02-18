/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file connection_edge.c
 * \brief Handle edge streams.
 *
 * An edge_connection_t is a subtype of a connection_t, and represents two
 * critical concepts in Tor: a stream, and an edge connection.  From the Tor
 * protocol's point of view, a stream is a bi-directional channel that is
 * multiplexed on a single circuit.  Each stream on a circuit is identified
 * with a separate 16-bit stream ID, local to the (circuit,exit) pair.
 * Streams are created in response to client requests.
 *
 * An edge connection is one thing that can implement a stream: it is either a
 * TCP application socket that has arrived via (e.g.) a SOCKS request, or an
 * exit connection.
 *
 * Not every instance of edge_connection_t truly represents an edge connction,
 * however. (Sorry!) We also create edge_connection_t objects for streams that
 * we will not be handling with TCP.  The types of these streams are:
 *   <ul>
 *   <li>DNS lookup streams, created on the client side in response to
 *     a UDP DNS request received on a DNSPort, or a RESOLVE command
 *     on a controller.
 *   <li>DNS lookup streams, created on the exit side in response to
 *     a RELAY_RESOLVE cell from a client.
 *   <li>Tunneled directory streams, created on the directory cache side
 *     in response to a RELAY_BEGIN_DIR cell.  These streams attach directly
 *     to a dir_connection_t object without ever using TCP.
 *   </ul>
 *
 * This module handles general-purpose functionality having to do with
 * edge_connection_t.  On the client side, it accepts various types of
 * application requests on SocksPorts, TransPorts, and NATDPorts, and
 * creates streams appropriately.
 *
 * This module is also responsible for implementing stream isolation:
 * ensuring that streams that should not be linkable to one another are
 * kept to different circuits.
 *
 * On the exit side, this module handles the various stream-creating
 * type of RELAY cells by launching appropriate outgoing connections,
 * DNS requests, or directory connection objects.
 *
 * And for all edge connections, this module is responsible for handling
 * incoming and outdoing data as it arrives or leaves in the relay.c
 * module.  (Outgoing data will be packaged in
 * connection_edge_process_inbuf() as it calls
 * connection_edge_package_raw_inbuf(); incoming data from RELAY_DATA
 * cells is applied in connection_edge_process_relay_cell().)
 **/
#define CONNECTION_EDGE_PRIVATE

#include "core/or/or.h"

#include "lib/err/backtrace.h"

#include "app/config/config.h"
#include "core/mainloop/connection.h"
#include "core/mainloop/mainloop.h"
#include "core/or/channel.h"
#include "core/or/circuitbuild.h"
#include "core/or/circuitlist.h"
#include "core/or/circuituse.h"
#include "core/or/connection_edge.h"
#include "core/or/connection_or.h"
#include "core/or/policies.h"
#include "core/or/reasons.h"
#include "core/or/relay.h"
#include "core/proto/proto_http.h"
#include "core/proto/proto_socks.h"
#include "feature/client/addressmap.h"
#include "feature/client/circpathbias.h"
#include "feature/client/dnsserv.h"
#include "feature/control/control.h"
#include "feature/dircache/dirserv.h"
#include "feature/dircommon/directory.h"
#include "feature/hibernate/hibernate.h"
#include "feature/hs/hs_cache.h"
#include "feature/hs/hs_circuit.h"
#include "feature/hs/hs_client.h"
#include "feature/hs/hs_common.h"
#include "feature/nodelist/describe.h"
#include "feature/nodelist/networkstatus.h"
#include "feature/nodelist/nodelist.h"
#include "feature/nodelist/routerlist.h"
#include "feature/nodelist/routerset.h"
#include "feature/relay/dns.h"
#include "feature/relay/router.h"
#include "feature/relay/routermode.h"
#include "feature/rend/rendclient.h"
#include "feature/rend/rendcommon.h"
#include "feature/rend/rendservice.h"
#include "feature/stats/predict_ports.h"
#include "feature/stats/rephist.h"
#include "lib/container/buffers.h"
#include "lib/crypt_ops/crypto_util.h"

#include "core/or/cell_st.h"
#include "core/or/cpath_build_state_st.h"
#include "feature/dircommon/dir_connection_st.h"
#include "core/or/entry_connection_st.h"
#include "core/or/extend_info_st.h"
#include "feature/nodelist/node_st.h"
#include "core/or/or_circuit_st.h"
#include "core/or/origin_circuit_st.h"
#include "core/or/half_edge_st.h"
#include "core/or/socks_request_st.h"
#include "lib/evloop/compat_libevent.h"

#ifdef HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif
#ifdef HAVE_LINUX_NETFILTER_IPV4_H
#include <linux/netfilter_ipv4.h>
#define TRANS_NETFILTER
#define TRANS_NETFILTER_IPV4
#endif

#ifdef HAVE_LINUX_IF_H
#include <linux/if.h>
#endif

#ifdef HAVE_LINUX_NETFILTER_IPV6_IP6_TABLES_H
#include <linux/netfilter_ipv6/ip6_tables.h>
#if defined(IP6T_SO_ORIGINAL_DST)
#define TRANS_NETFILTER
#define TRANS_NETFILTER_IPV6
#endif
#endif /* defined(HAVE_LINUX_NETFILTER_IPV6_IP6_TABLES_H) */

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if defined(HAVE_NET_IF_H) && defined(HAVE_NET_PFVAR_H)
#include <net/if.h>
#include <net/pfvar.h>
#define TRANS_PF
#endif

#ifdef IP_TRANSPARENT
#define TRANS_TPROXY
#endif

#define SOCKS4_GRANTED          90
#define SOCKS4_REJECT           91

static int connection_ap_handshake_process_socks(entry_connection_t *conn);
static int connection_ap_process_natd(entry_connection_t *conn);
static int connection_exit_connect_dir(edge_connection_t *exitconn);
static int consider_plaintext_ports(entry_connection_t *conn, uint16_t port);
static int connection_ap_supports_optimistic_data(const entry_connection_t *);

/** Convert a connection_t* to an edge_connection_t*; assert if the cast is
 * invalid. */
edge_connection_t *
TO_EDGE_CONN(connection_t *c)
{
  tor_assert(c->magic == EDGE_CONNECTION_MAGIC ||
             c->magic == ENTRY_CONNECTION_MAGIC);
  return DOWNCAST(edge_connection_t, c);
}

entry_connection_t *
TO_ENTRY_CONN(connection_t *c)
{
  tor_assert(c->magic == ENTRY_CONNECTION_MAGIC);
  return (entry_connection_t*) SUBTYPE_P(c, entry_connection_t, edge_.base_);
}

entry_connection_t *
EDGE_TO_ENTRY_CONN(edge_connection_t *c)
{
  tor_assert(c->base_.magic == ENTRY_CONNECTION_MAGIC);
  return (entry_connection_t*) SUBTYPE_P(c, entry_connection_t, edge_);
}

/** An AP stream has failed/finished. If it hasn't already sent back
 * a socks reply, send one now (based on endreason). Also set
 * has_sent_end to 1, and mark the conn.
 */
MOCK_IMPL(void,
connection_mark_unattached_ap_,(entry_connection_t *conn, int endreason,
                                int line, const char *file))
{
  connection_t *base_conn = ENTRY_TO_CONN(conn);
  edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(conn);
  tor_assert(base_conn->type == CONN_TYPE_AP);
  ENTRY_TO_EDGE_CONN(conn)->edge_has_sent_end = 1; /* no circ yet */

  /* If this is a rendezvous stream and it is failing without ever
   * being attached to a circuit, assume that an attempt to connect to
   * the destination hidden service has just ended.
   *
   * XXXX This condition doesn't limit to only streams failing
   * without ever being attached.  That sloppiness should be harmless,
   * but we should fix it someday anyway. */
  if ((edge_conn->on_circuit != NULL || edge_conn->edge_has_sent_end) &&
      connection_edge_is_rendezvous_stream(edge_conn)) {
    if (edge_conn->rend_data) {
      rend_client_note_connection_attempt_ended(edge_conn->rend_data);
    }
  }

  if (base_conn->marked_for_close) {
    /* This call will warn as appropriate. */
    connection_mark_for_close_(base_conn, line, file);
    return;
  }

  if (!conn->socks_request->has_finished) {
    if (endreason & END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED)
      log_warn(LD_BUG,
               "stream (marked at %s:%d) sending two socks replies?",
               file, line);

    if (SOCKS_COMMAND_IS_CONNECT(conn->socks_request->command))
      connection_ap_handshake_socks_reply(conn, NULL, 0, endreason);
    else if (SOCKS_COMMAND_IS_RESOLVE(conn->socks_request->command))
      connection_ap_handshake_socks_resolved(conn,
                                             RESOLVED_TYPE_ERROR_TRANSIENT,
                                             0, NULL, -1, -1);
    else /* unknown or no handshake at all. send no response. */
      conn->socks_request->has_finished = 1;
  }

  connection_mark_and_flush_(base_conn, line, file);

  ENTRY_TO_EDGE_CONN(conn)->end_reason = endreason;
}

/** There was an EOF. Send an end and mark the connection for close.
 */
int
connection_edge_reached_eof(edge_connection_t *conn)
{
  if (connection_get_inbuf_len(TO_CONN(conn)) &&
      connection_state_is_open(TO_CONN(conn))) {
    /* it still has stuff to process. don't let it die yet. */
    return 0;
  }
  log_info(LD_EDGE,"conn (fd "TOR_SOCKET_T_FORMAT") reached eof. Closing.",
           conn->base_.s);
  if (!conn->base_.marked_for_close) {
    /* only mark it if not already marked. it's possible to
     * get the 'end' right around when the client hangs up on us. */
    connection_edge_end(conn, END_STREAM_REASON_DONE);
    if (conn->base_.type == CONN_TYPE_AP) {
      /* eof, so don't send a socks reply back */
      if (EDGE_TO_ENTRY_CONN(conn)->socks_request)
        EDGE_TO_ENTRY_CONN(conn)->socks_request->has_finished = 1;
    }
    connection_mark_for_close(TO_CONN(conn));
  }
  return 0;
}

/** Handle new bytes on conn->inbuf based on state:
 *   - If it's waiting for socks info, try to read another step of the
 *     socks handshake out of conn->inbuf.
 *   - If it's waiting for the original destination, fetch it.
 *   - If it's open, then package more relay cells from the stream.
 *   - Else, leave the bytes on inbuf alone for now.
 *
 * Mark and return -1 if there was an unexpected error with the conn,
 * else return 0.
 */
int
connection_edge_process_inbuf(edge_connection_t *conn, int package_partial)
{
  tor_assert(conn);

  switch (conn->base_.state) {
    case AP_CONN_STATE_SOCKS_WAIT:
      if (connection_ap_handshake_process_socks(EDGE_TO_ENTRY_CONN(conn)) <0) {
        /* already marked */
        return -1;
      }
      return 0;
    case AP_CONN_STATE_NATD_WAIT:
      if (connection_ap_process_natd(EDGE_TO_ENTRY_CONN(conn)) < 0) {
        /* already marked */
        return -1;
      }
      return 0;
    case AP_CONN_STATE_HTTP_CONNECT_WAIT:
      if (connection_ap_process_http_connect(EDGE_TO_ENTRY_CONN(conn)) < 0) {
        return -1;
      }
      return 0;
    case AP_CONN_STATE_OPEN:
    case EXIT_CONN_STATE_OPEN:
      if (connection_edge_package_raw_inbuf(conn, package_partial, NULL) < 0) {
        /* (We already sent an end cell if possible) */
        connection_mark_for_close(TO_CONN(conn));
        return -1;
      }
      return 0;
    case AP_CONN_STATE_CONNECT_WAIT:
      if (connection_ap_supports_optimistic_data(EDGE_TO_ENTRY_CONN(conn))) {
        log_info(LD_EDGE,
                 "data from edge while in '%s' state. Sending it anyway. "
                 "package_partial=%d, buflen=%ld",
                 conn_state_to_string(conn->base_.type, conn->base_.state),
                 package_partial,
                 (long)connection_get_inbuf_len(TO_CONN(conn)));
        if (connection_edge_package_raw_inbuf(conn, package_partial, NULL)<0) {
          /* (We already sent an end cell if possible) */
          connection_mark_for_close(TO_CONN(conn));
          return -1;
        }
        return 0;
      }
      /* Fall through if the connection is on a circuit without optimistic
       * data support. */
      /* Falls through. */
    case EXIT_CONN_STATE_CONNECTING:
    case AP_CONN_STATE_RENDDESC_WAIT:
    case AP_CONN_STATE_CIRCUIT_WAIT:
    case AP_CONN_STATE_RESOLVE_WAIT:
    case AP_CONN_STATE_CONTROLLER_WAIT:
      log_info(LD_EDGE,
               "data from edge while in '%s' state. Leaving it on buffer.",
               conn_state_to_string(conn->base_.type, conn->base_.state));
      return 0;
  }
  log_warn(LD_BUG,"Got unexpected state %d. Closing.",conn->base_.state);
  tor_fragile_assert();
  connection_edge_end(conn, END_STREAM_REASON_INTERNAL);
  connection_mark_for_close(TO_CONN(conn));
  return -1;
}

/** This edge needs to be closed, because its circuit has closed.
 * Mark it for close and return 0.
 */
int
connection_edge_destroy(circid_t circ_id, edge_connection_t *conn)
{
  if (!conn->base_.marked_for_close) {
    log_info(LD_EDGE, "CircID %u: At an edge. Marking connection for close.",
             (unsigned) circ_id);
    if (conn->base_.type == CONN_TYPE_AP) {
      entry_connection_t *entry_conn = EDGE_TO_ENTRY_CONN(conn);
      connection_mark_unattached_ap(entry_conn, END_STREAM_REASON_DESTROY);
      control_event_stream_bandwidth(conn);
      control_event_stream_status(entry_conn, STREAM_EVENT_CLOSED,
                                  END_STREAM_REASON_DESTROY);
      conn->end_reason |= END_STREAM_REASON_FLAG_ALREADY_SENT_CLOSED;
    } else {
      /* closing the circuit, nothing to send an END to */
      conn->edge_has_sent_end = 1;
      conn->end_reason = END_STREAM_REASON_DESTROY;
      conn->end_reason |= END_STREAM_REASON_FLAG_ALREADY_SENT_CLOSED;
      connection_mark_and_flush(TO_CONN(conn));
    }
  }
  conn->cpath_layer = NULL;
  conn->on_circuit = NULL;
  return 0;
}

/** Send a raw end cell to the stream with ID <b>stream_id</b> out over the
 * <b>circ</b> towards the hop identified with <b>cpath_layer</b>. If this
 * is not a client connection, set the relay end cell's reason for closing
 * as <b>reason</b> */
static int
relay_send_end_cell_from_edge(streamid_t stream_id, circuit_t *circ,
                              uint8_t reason, crypt_path_t *cpath_layer)
{
  char payload[1];

  if (CIRCUIT_PURPOSE_IS_CLIENT(circ->purpose)) {
    /* Never send the server an informative reason code; it doesn't need to
     * know why the client stream is failing. */
    reason = END_STREAM_REASON_MISC;
  }

  payload[0] = (char) reason;

  /* Note: we have to use relay_send_command_from_edge here, not
   * connection_edge_end or connection_edge_send_command, since those require
   * that we have a stream connected to a circuit, and we don't connect to a
   * circuit until we have a pending/successful resolve. */
  return relay_send_command_from_edge(stream_id, circ, RELAY_COMMAND_END,
                                      payload, 1, cpath_layer);
}

/* If the connection <b>conn</b> is attempting to connect to an external
 * destination that is an hidden service and the reason is a connection
 * refused or timeout, log it so the operator can take appropriate actions.
 * The log statement is a rate limited warning. */
static void
warn_if_hs_unreachable(const edge_connection_t *conn, uint8_t reason)
{
  tor_assert(conn);

  if (conn->base_.type == CONN_TYPE_EXIT &&
      connection_edge_is_rendezvous_stream(conn) &&
      (reason == END_STREAM_REASON_CONNECTREFUSED ||
       reason == END_STREAM_REASON_TIMEOUT)) {
#define WARN_FAILED_HS_CONNECTION 300
    static ratelim_t warn_limit = RATELIM_INIT(WARN_FAILED_HS_CONNECTION);
    char *m;
    if ((m = rate_limit_log(&warn_limit, approx_time()))) {
      log_warn(LD_EDGE, "Onion service connection to %s failed (%s)",
               (conn->base_.socket_family == AF_UNIX) ?
               safe_str(conn->base_.address) :
               safe_str(fmt_addrport(&conn->base_.addr, conn->base_.port)),
               stream_end_reason_to_string(reason));
      tor_free(m);
    }
  }
}

/** Send a relay end cell from stream <b>conn</b> down conn's circuit, and
 * remember that we've done so.  If this is not a client connection, set the
 * relay end cell's reason for closing as <b>reason</b>.
 *
 * Return -1 if this function has already been called on this conn,
 * else return 0.
 */
int
connection_edge_end(edge_connection_t *conn, uint8_t reason)
{
  char payload[RELAY_PAYLOAD_SIZE];
  size_t payload_len=1;
  circuit_t *circ;
  uint8_t control_reason = reason;

  if (conn->edge_has_sent_end) {
    log_warn(LD_BUG,"(Harmless.) Calling connection_edge_end (reason %d) "
             "on an already ended stream?", reason);
    tor_fragile_assert();
    return -1;
  }

  if (conn->base_.marked_for_close) {
    log_warn(LD_BUG,
             "called on conn that's already marked for close at %s:%d.",
             conn->base_.marked_for_close_file, conn->base_.marked_for_close);
    return 0;
  }

  circ = circuit_get_by_edge_conn(conn);
  if (circ && CIRCUIT_PURPOSE_IS_CLIENT(circ->purpose)) {
    /* If this is a client circuit, don't send the server an informative
     * reason code; it doesn't need to know why the client stream is
     * failing. */
    reason = END_STREAM_REASON_MISC;
  }

  payload[0] = (char)reason;
  if (reason == END_STREAM_REASON_EXITPOLICY &&
      !connection_edge_is_rendezvous_stream(conn)) {
    int addrlen;
    if (tor_addr_family(&conn->base_.addr) == AF_INET) {
      set_uint32(payload+1, tor_addr_to_ipv4n(&conn->base_.addr));
      addrlen = 4;
    } else {
      memcpy(payload+1, tor_addr_to_in6_addr8(&conn->base_.addr), 16);
      addrlen = 16;
    }
    set_uint32(payload+1+addrlen, htonl(dns_clip_ttl(conn->address_ttl)));
    payload_len += 4+addrlen;
  }

  if (circ && !circ->marked_for_close) {
    log_debug(LD_EDGE,"Sending end on conn (fd "TOR_SOCKET_T_FORMAT").",
              conn->base_.s);

    if (CIRCUIT_IS_ORIGIN(circ)) {
      origin_circuit_t *origin_circ = TO_ORIGIN_CIRCUIT(circ);
      connection_half_edge_add(conn, origin_circ);
    }

    connection_edge_send_command(conn, RELAY_COMMAND_END,
                                 payload, payload_len);
    /* We'll log warn if the connection was an hidden service and couldn't be
     * made because the service wasn't available. */
    warn_if_hs_unreachable(conn, control_reason);
  } else {
    log_debug(LD_EDGE,"No circ to send end on conn "
              "(fd "TOR_SOCKET_T_FORMAT").",
              conn->base_.s);
  }

  conn->edge_has_sent_end = 1;
  conn->end_reason = control_reason;
  return 0;
}

/**
 * Helper function for bsearch.
 *
 * As per smartlist_bsearch, return < 0 if key preceeds member,
 * > 0 if member preceeds key, and 0 if they are equal.
 *
 * This is equivalent to subtraction of the values of key - member
 * (why does no one ever say that explicitly?).
 */
static int
connection_half_edge_compare_bsearch(const void *key, const void **member)
{
  const half_edge_t *e2;
  tor_assert(key);
  tor_assert(member && *(half_edge_t**)member);
  e2 = *(const half_edge_t **)member;

  return *(const streamid_t*)key - e2->stream_id;
}

/** Total number of half_edge_t objects allocated */
static size_t n_half_conns_allocated = 0;

/**
 * Add a half-closed connection to the list, to watch for activity.
 *
 * These connections are removed from the list upon receiving an end
 * cell.
 */
STATIC void
connection_half_edge_add(const edge_connection_t *conn,
                         origin_circuit_t *circ)
{
  half_edge_t *half_conn = NULL;
  int insert_at = 0;
  int ignored;

  /* Double-check for re-insertion. This should not happen,
   * but this check is cheap compared to the sort anyway */
  if (connection_half_edge_find_stream_id(circ->half_streams,
                                          conn->stream_id)) {
    log_warn(LD_BUG, "Duplicate stream close for stream %d on circuit %d",
             conn->stream_id, circ->global_identifier);
    return;
  }

  half_conn = tor_malloc_zero(sizeof(half_edge_t));
  ++n_half_conns_allocated;

  if (!circ->half_streams) {
    circ->half_streams = smartlist_new();
  }

  half_conn->stream_id = conn->stream_id;

  // How many sendme's should I expect?
  half_conn->sendmes_pending =
   (STREAMWINDOW_START-conn->package_window)/STREAMWINDOW_INCREMENT;

   // Is there a connected cell pending?
  half_conn->connected_pending = conn->base_.state ==
      AP_CONN_STATE_CONNECT_WAIT;

  /* Data should only arrive if we're not waiting on a resolved cell.
   * It can arrive after waiting on connected, because of optimistic
   * data. */
  if (conn->base_.state != AP_CONN_STATE_RESOLVE_WAIT) {
    // How many more data cells can arrive on this id?
    half_conn->data_pending = conn->deliver_window;
  }

  insert_at = smartlist_bsearch_idx(circ->half_streams, &half_conn->stream_id,
                                    connection_half_edge_compare_bsearch,
                                    &ignored);
  smartlist_insert(circ->half_streams, insert_at, half_conn);
}

/** Release space held by <b>he</b> */
void
half_edge_free_(half_edge_t *he)
{
  if (!he)
    return;
  --n_half_conns_allocated;
  tor_free(he);
}

/** Return the number of bytes devoted to storing info on half-open streams. */
size_t
half_streams_get_total_allocation(void)
{
  return n_half_conns_allocated * sizeof(half_edge_t);
}

/**
 * Find a stream_id_t in the list in O(lg(n)).
 *
 * Returns NULL if the list is empty or element is not found.
 * Returns a pointer to the element if found.
 */
STATIC half_edge_t *
connection_half_edge_find_stream_id(const smartlist_t *half_conns,
                                    streamid_t stream_id)
{
  if (!half_conns)
    return NULL;

  return smartlist_bsearch(half_conns, &stream_id,
                           connection_half_edge_compare_bsearch);
}

/**
 * Check if this stream_id is in a half-closed state. If so,
 * check if it still has data cells pending, and decrement that
 * window if so.
 *
 * Return 1 if the data window was not empty.
 * Return 0 otherwise.
 */
int
connection_half_edge_is_valid_data(const smartlist_t *half_conns,
                                   streamid_t stream_id)
{
  half_edge_t *half = connection_half_edge_find_stream_id(half_conns,
                                                          stream_id);

  if (!half)
    return 0;

  if (half->data_pending > 0) {
    half->data_pending--;
    return 1;
  }

  return 0;
}

/**
 * Check if this stream_id is in a half-closed state. If so,
 * check if it still has a connected cell pending, and decrement
 * that window if so.
 *
 * Return 1 if the connected window was not empty.
 * Return 0 otherwise.
 */
int
connection_half_edge_is_valid_connected(const smartlist_t *half_conns,
                                        streamid_t stream_id)
{
  half_edge_t *half = connection_half_edge_find_stream_id(half_conns,
                                                          stream_id);

  if (!half)
    return 0;

  if (half->connected_pending) {
    half->connected_pending = 0;
    return 1;
  }

  return 0;
}

/**
 * Check if this stream_id is in a half-closed state. If so,
 * check if it still has sendme cells pending, and decrement that
 * window if so.
 *
 * Return 1 if the sendme window was not empty.
 * Return 0 otherwise.
 */
int
connection_half_edge_is_valid_sendme(const smartlist_t *half_conns,
                                     streamid_t stream_id)
{
  half_edge_t *half = connection_half_edge_find_stream_id(half_conns,
                                                          stream_id);

  if (!half)
    return 0;

  if (half->sendmes_pending > 0) {
    half->sendmes_pending--;
    return 1;
  }

  return 0;
}

/**
 * Check if this stream_id is in a half-closed state. If so, remove
 * it from the list. No other data should come after the END cell.
 *
 * Return 1 if stream_id was in half-closed state.
 * Return 0 otherwise.
 */
int
connection_half_edge_is_valid_end(smartlist_t *half_conns,
                                  streamid_t stream_id)
{
  half_edge_t *half;
  int found, remove_idx;

  if (!half_conns)
    return 0;

  remove_idx = smartlist_bsearch_idx(half_conns, &stream_id,
                                    connection_half_edge_compare_bsearch,
                                    &found);
  if (!found)
    return 0;

  half = smartlist_get(half_conns, remove_idx);
  smartlist_del_keeporder(half_conns, remove_idx);
  half_edge_free(half);
  return 1;
}

/**
 * Streams that were used to send a RESOLVE cell are closed
 * when they get the RESOLVED, without an end. So treat
 * a RESOLVED just like an end, and remove from the list.
 */
int
connection_half_edge_is_valid_resolved(smartlist_t *half_conns,
                                       streamid_t stream_id)
{
  return connection_half_edge_is_valid_end(half_conns, stream_id);
}

/** An error has just occurred on an operation on an edge connection
 * <b>conn</b>.  Extract the errno; convert it to an end reason, and send an
 * appropriate relay end cell to the other end of the connection's circuit.
 **/
int
connection_edge_end_errno(edge_connection_t *conn)
{
  uint8_t reason;
  tor_assert(conn);
  reason = errno_to_stream_end_reason(tor_socket_errno(conn->base_.s));
  return connection_edge_end(conn, reason);
}

/** We just wrote some data to <b>conn</b>; act appropriately.
 *
 * (That is, if it's open, consider sending a stream-level sendme cell if we
 * have just flushed enough.)
 */
int
connection_edge_flushed_some(edge_connection_t *conn)
{
  switch (conn->base_.state) {
    case AP_CONN_STATE_OPEN:
    case EXIT_CONN_STATE_OPEN:
      connection_edge_consider_sending_sendme(conn);
      break;
  }
  return 0;
}

/** Connection <b>conn</b> has finished writing and has no bytes left on
 * its outbuf.
 *
 * If it's in state 'open', stop writing, consider responding with a
 * sendme, and return.
 * Otherwise, stop writing and return.
 *
 * If <b>conn</b> is broken, mark it for close and return -1, else
 * return 0.
 */
int
connection_edge_finished_flushing(edge_connection_t *conn)
{
  tor_assert(conn);

  switch (conn->base_.state) {
    case AP_CONN_STATE_OPEN:
    case EXIT_CONN_STATE_OPEN:
      connection_edge_consider_sending_sendme(conn);
      return 0;
    case AP_CONN_STATE_SOCKS_WAIT:
    case AP_CONN_STATE_NATD_WAIT:
    case AP_CONN_STATE_RENDDESC_WAIT:
    case AP_CONN_STATE_CIRCUIT_WAIT:
    case AP_CONN_STATE_CONNECT_WAIT:
    case AP_CONN_STATE_CONTROLLER_WAIT:
    case AP_CONN_STATE_RESOLVE_WAIT:
    case AP_CONN_STATE_HTTP_CONNECT_WAIT:
      return 0;
    default:
      log_warn(LD_BUG, "Called in unexpected state %d.",conn->base_.state);
      tor_fragile_assert();
      return -1;
  }
  return 0;
}

/** Longest size for the relay payload of a RELAY_CONNECTED cell that we're
 * able to generate. */
/* 4 zero bytes; 1 type byte; 16 byte IPv6 address; 4 byte TTL. */
#define MAX_CONNECTED_CELL_PAYLOAD_LEN 25

/** Set the buffer at <b>payload_out</b> -- which must have at least
 * MAX_CONNECTED_CELL_PAYLOAD_LEN bytes available -- to the body of a
 * RELAY_CONNECTED cell indicating that we have connected to <b>addr</b>, and
 * that the name resolution that led us to <b>addr</b> will be valid for
 * <b>ttl</b> seconds. Return -1 on error, or the number of bytes used on
 * success. */
STATIC int
connected_cell_format_payload(uint8_t *payload_out,
                              const tor_addr_t *addr,
                              uint32_t ttl)
{
  const sa_family_t family = tor_addr_family(addr);
  int connected_payload_len;

  /* should be needless */
  memset(payload_out, 0, MAX_CONNECTED_CELL_PAYLOAD_LEN);

  if (family == AF_INET) {
    set_uint32(payload_out, tor_addr_to_ipv4n(addr));
    connected_payload_len = 4;
  } else if (family == AF_INET6) {
    set_uint32(payload_out, 0);
    set_uint8(payload_out + 4, 6);
    memcpy(payload_out + 5, tor_addr_to_in6_addr8(addr), 16);
    connected_payload_len = 21;
  } else {
    return -1;
  }

  set_uint32(payload_out + connected_payload_len, htonl(dns_clip_ttl(ttl)));
  connected_payload_len += 4;

  tor_assert(connected_payload_len <= MAX_CONNECTED_CELL_PAYLOAD_LEN);

  return connected_payload_len;
}

/* This is an onion service client connection: Export the client circuit ID
 * according to the HAProxy proxy protocol. */
STATIC void
export_hs_client_circuit_id(edge_connection_t *edge_conn,
                            hs_circuit_id_protocol_t protocol)
{
  /* We only support HAProxy right now. */
  if (protocol != HS_CIRCUIT_ID_PROTOCOL_HAPROXY)
    return;

  char *buf = NULL;
  const char dst_ipv6[] = "::1";
  /* See RFC4193 regarding fc00::/7 */
  const char src_ipv6_prefix[] = "fc00:dead:beef:4dad:";
  uint16_t dst_port = 0;
  uint16_t src_port = 1; /* default value */
  uint32_t gid = 0; /* default value */

  /* Generate a GID and source port for this client */
  if (edge_conn->on_circuit != NULL) {
    gid = TO_ORIGIN_CIRCUIT(edge_conn->on_circuit)->global_identifier;
    src_port = gid & 0x0000ffff;
  }

  /* Grab the original dest port from the hs ident */
  if (edge_conn->hs_ident) {
    dst_port = edge_conn->hs_ident->orig_virtual_port;
  }

  /* Build the string */
  tor_asprintf(&buf, "PROXY TCP6 %s:%x:%x %s %d %d\r\n",
               src_ipv6_prefix,
               gid >> 16, gid & 0x0000ffff,
               dst_ipv6, src_port, dst_port);

  connection_buf_add(buf, strlen(buf), TO_CONN(edge_conn));

  tor_free(buf);
}

/** Connected handler for exit connections: start writing pending
 * data, deliver 'CONNECTED' relay cells as appropriate, and check
 * any pending data that may have been received. */
int
connection_edge_finished_connecting(edge_connection_t *edge_conn)
{
  connection_t *conn;

  tor_assert(edge_conn);
  tor_assert(edge_conn->base_.type == CONN_TYPE_EXIT);
  conn = TO_CONN(edge_conn);
  tor_assert(conn->state == EXIT_CONN_STATE_CONNECTING);

  log_info(LD_EXIT,"Exit connection to %s:%u (%s) established.",
           escaped_safe_str(conn->address), conn->port,
           safe_str(fmt_and_decorate_addr(&conn->addr)));

  rep_hist_note_exit_stream_opened(conn->port);

  conn->state = EXIT_CONN_STATE_OPEN;

  connection_watch_events(conn, READ_EVENT); /* stop writing, keep reading */
  if (connection_get_outbuf_len(conn)) /* in case there are any queued relay
                                        * cells */
    connection_start_writing(conn);
  /* deliver a 'connected' relay cell back through the circuit. */
  if (connection_edge_is_rendezvous_stream(edge_conn)) {
    if (connection_edge_send_command(edge_conn,
                                     RELAY_COMMAND_CONNECTED, NULL, 0) < 0)
      return 0; /* circuit is closed, don't continue */
  } else {
    uint8_t connected_payload[MAX_CONNECTED_CELL_PAYLOAD_LEN];
    int connected_payload_len =
      connected_cell_format_payload(connected_payload, &conn->addr,
                                    edge_conn->address_ttl);
    if (connected_payload_len < 0)
      return -1;

    if (connection_edge_send_command(edge_conn,
                        RELAY_COMMAND_CONNECTED,
                        (char*)connected_payload, connected_payload_len) < 0)
      return 0; /* circuit is closed, don't continue */
  }
  tor_assert(edge_conn->package_window > 0);
  /* in case the server has written anything */
  return connection_edge_process_inbuf(edge_conn, 1);
}

/** A list of all the entry_connection_t * objects that are not marked
 * for close, and are in AP_CONN_STATE_CIRCUIT_WAIT.
 *
 * (Right now, we check in several places to make sure that this list is
 * correct.  When it's incorrect, we'll fix it, and log a BUG message.)
 */
static smartlist_t *pending_entry_connections = NULL;

static int untried_pending_connections = 0;

/**
 * Mainloop event to tell us to scan for pending connections that can
 * be attached.
 */
static mainloop_event_t *attach_pending_entry_connections_ev = NULL;

/** Common code to connection_(ap|exit)_about_to_close. */
static void
connection_edge_about_to_close(edge_connection_t *edge_conn)
{
  if (!edge_conn->edge_has_sent_end) {
    connection_t *conn = TO_CONN(edge_conn);
    log_warn(LD_BUG, "(Harmless.) Edge connection (marked at %s:%d) "
             "hasn't sent end yet?",
             conn->marked_for_close_file, conn->marked_for_close);
    tor_fragile_assert();
  }
}

/** Called when we're about to finally unlink and free an AP (client)
 * connection: perform necessary accounting and cleanup */
void
connection_ap_about_to_close(entry_connection_t *entry_conn)
{
  circuit_t *circ;
  edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(entry_conn);
  connection_t *conn = ENTRY_TO_CONN(entry_conn);

  connection_edge_about_to_close(edge_conn);

  if (entry_conn->socks_request->has_finished == 0) {
    /* since conn gets removed right after this function finishes,
     * there's no point trying to send back a reply at this point. */
    log_warn(LD_BUG,"Closing stream (marked at %s:%d) without sending"
             " back a socks reply.",
             conn->marked_for_close_file, conn->marked_for_close);
  }
  if (!edge_conn->end_reason) {
    log_warn(LD_BUG,"Closing stream (marked at %s:%d) without having"
             " set end_reason.",
             conn->marked_for_close_file, conn->marked_for_close);
  }
  if (entry_conn->dns_server_request) {
    log_warn(LD_BUG,"Closing stream (marked at %s:%d) without having"
             " replied to DNS request.",
             conn->marked_for_close_file, conn->marked_for_close);
    dnsserv_reject_request(entry_conn);
  }

  if (TO_CONN(edge_conn)->state == AP_CONN_STATE_CIRCUIT_WAIT) {
    smartlist_remove(pending_entry_connections, entry_conn);
  }

#if 1
  /* Check to make sure that this isn't in pending_entry_connections if it
   * didn't actually belong there. */
  if (TO_CONN(edge_conn)->type == CONN_TYPE_AP) {
    connection_ap_warn_and_unmark_if_pending_circ(entry_conn,
                                                  "about_to_close");
  }
#endif /* 1 */

  control_event_stream_bandwidth(edge_conn);
  control_event_stream_status(entry_conn, STREAM_EVENT_CLOSED,
                              edge_conn->end_reason);
  circ = circuit_get_by_edge_conn(edge_conn);
  if (circ)
    circuit_detach_stream(circ, edge_conn);
}

/** Called when we're about to finally unlink and free an exit
 * connection: perform necessary accounting and cleanup */
void
connection_exit_about_to_close(edge_connection_t *edge_conn)
{
  circuit_t *circ;
  connection_t *conn = TO_CONN(edge_conn);

  connection_edge_about_to_close(edge_conn);

  circ = circuit_get_by_edge_conn(edge_conn);
  if (circ)
    circuit_detach_stream(circ, edge_conn);
  if (conn->state == EXIT_CONN_STATE_RESOLVING) {
    connection_dns_remove(edge_conn);
  }
}

/** Define a schedule for how long to wait between retrying
 * application connections. Rather than waiting a fixed amount of
 * time between each retry, we wait 10 seconds each for the first
 * two tries, and 15 seconds for each retry after
 * that. Hopefully this will improve the expected user experience. */
static int
compute_retry_timeout(entry_connection_t *conn)
{
  int timeout = get_options()->CircuitStreamTimeout;
  if (timeout) /* if our config options override the default, use them */
    return timeout;
  if (conn->num_socks_retries < 2) /* try 0 and try 1 */
    return 10;
  return 15;
}

/** Find all general-purpose AP streams waiting for a response that sent their
 * begin/resolve cell too long ago. Detach from their current circuit, and
 * mark their current circuit as unsuitable for new streams. Then call
 * connection_ap_handshake_attach_circuit() to attach to a new circuit (if
 * available) or launch a new one.
 *
 * For rendezvous streams, simply give up after SocksTimeout seconds (with no
 * retry attempt).
 */
void
connection_ap_expire_beginning(void)
{
  edge_connection_t *conn;
  entry_connection_t *entry_conn;
  circuit_t *circ;
  time_t now = time(NULL);
  const or_options_t *options = get_options();
  int severity;
  int cutoff;
  int seconds_idle, seconds_since_born;
  smartlist_t *conns = get_connection_array();

  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, base_conn) {
    if (base_conn->type != CONN_TYPE_AP || base_conn->marked_for_close)
      continue;
    entry_conn = TO_ENTRY_CONN(base_conn);
    conn = ENTRY_TO_EDGE_CONN(entry_conn);
    /* if it's an internal linked connection, don't yell its status. */
    severity = (tor_addr_is_null(&base_conn->addr) && !base_conn->port)
      ? LOG_INFO : LOG_NOTICE;
    seconds_idle = (int)( now - base_conn->timestamp_last_read_allowed );
    seconds_since_born = (int)( now - base_conn->timestamp_created );

    if (base_conn->state == AP_CONN_STATE_OPEN)
      continue;

    /* We already consider SocksTimeout in
     * connection_ap_handshake_attach_circuit(), but we need to consider
     * it here too because controllers that put streams in controller_wait
     * state never ask Tor to attach the circuit. */
    if (AP_CONN_STATE_IS_UNATTACHED(base_conn->state)) {
      if (seconds_since_born >= options->SocksTimeout) {
        log_fn(severity, LD_APP,
            "Tried for %d seconds to get a connection to %s:%d. "
            "Giving up. (%s)",
            seconds_since_born,
            safe_str_client(entry_conn->socks_request->address),
            entry_conn->socks_request->port,
            conn_state_to_string(CONN_TYPE_AP, base_conn->state));
        connection_mark_unattached_ap(entry_conn, END_STREAM_REASON_TIMEOUT);
      }
      continue;
    }

    /* We're in state connect_wait or resolve_wait now -- waiting for a
     * reply to our relay cell. See if we want to retry/give up. */

    cutoff = compute_retry_timeout(entry_conn);
    if (seconds_idle < cutoff)
      continue;
    circ = circuit_get_by_edge_conn(conn);
    if (!circ) { /* it's vanished? */
      log_info(LD_APP,"Conn is waiting (address %s), but lost its circ.",
               safe_str_client(entry_conn->socks_request->address));
      connection_mark_unattached_ap(entry_conn, END_STREAM_REASON_TIMEOUT);
      continue;
    }
    if (circ->purpose == CIRCUIT_PURPOSE_C_REND_JOINED) {
      if (seconds_idle >= options->SocksTimeout) {
        log_fn(severity, LD_REND,
               "Rend stream is %d seconds late. Giving up on address"
               " '%s.onion'.",
               seconds_idle,
               safe_str_client(entry_conn->socks_request->address));
        /* Roll back path bias use state so that we probe the circuit
         * if nothing else succeeds on it */
        pathbias_mark_use_rollback(TO_ORIGIN_CIRCUIT(circ));

        connection_edge_end(conn, END_STREAM_REASON_TIMEOUT);
        connection_mark_unattached_ap(entry_conn, END_STREAM_REASON_TIMEOUT);
      }
      continue;
    }

    if (circ->purpose != CIRCUIT_PURPOSE_C_GENERAL &&
        circ->purpose != CIRCUIT_PURPOSE_C_HSDIR_GET &&
        circ->purpose != CIRCUIT_PURPOSE_S_HSDIR_POST &&
        circ->purpose != CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT &&
        circ->purpose != CIRCUIT_PURPOSE_PATH_BIAS_TESTING) {
      log_warn(LD_BUG, "circuit->purpose == CIRCUIT_PURPOSE_C_GENERAL failed. "
               "The purpose on the circuit was %s; it was in state %s, "
               "path_state %s.",
               circuit_purpose_to_string(circ->purpose),
               circuit_state_to_string(circ->state),
               CIRCUIT_IS_ORIGIN(circ) ?
                pathbias_state_to_string(TO_ORIGIN_CIRCUIT(circ)->path_state) :
                "none");
    }
    log_fn(cutoff < 15 ? LOG_INFO : severity, LD_APP,
           "We tried for %d seconds to connect to '%s' using exit %s."
           " Retrying on a new circuit.",
           seconds_idle,
           safe_str_client(entry_conn->socks_request->address),
           conn->cpath_layer ?
             extend_info_describe(conn->cpath_layer->extend_info):
             "*unnamed*");
    /* send an end down the circuit */
    connection_edge_end(conn, END_STREAM_REASON_TIMEOUT);
    /* un-mark it as ending, since we're going to reuse it */
    conn->edge_has_sent_end = 0;
    conn->end_reason = 0;
    /* make us not try this circuit again, but allow
     * current streams on it to survive if they can */
    mark_circuit_unusable_for_new_conns(TO_ORIGIN_CIRCUIT(circ));

    /* give our stream another 'cutoff' seconds to try */
    conn->base_.timestamp_last_read_allowed += cutoff;
    if (entry_conn->num_socks_retries < 250) /* avoid overflow */
      entry_conn->num_socks_retries++;
    /* move it back into 'pending' state, and try to attach. */
    if (connection_ap_detach_retriable(entry_conn, TO_ORIGIN_CIRCUIT(circ),
                                       END_STREAM_REASON_TIMEOUT)<0) {
      if (!base_conn->marked_for_close)
        connection_mark_unattached_ap(entry_conn,
                                      END_STREAM_REASON_CANT_ATTACH);
    }
  } SMARTLIST_FOREACH_END(base_conn);
}

/**
 * As connection_ap_attach_pending, but first scans the entire connection
 * array to see if any elements are missing.
 */
void
connection_ap_rescan_and_attach_pending(void)
{
  entry_connection_t *entry_conn;
  smartlist_t *conns = get_connection_array();

  if (PREDICT_UNLIKELY(NULL == pending_entry_connections))
    pending_entry_connections = smartlist_new();

  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, conn) {
    if (conn->marked_for_close ||
        conn->type != CONN_TYPE_AP ||
        conn->state != AP_CONN_STATE_CIRCUIT_WAIT)
      continue;

    entry_conn = TO_ENTRY_CONN(conn);
    tor_assert(entry_conn);
    if (! smartlist_contains(pending_entry_connections, entry_conn)) {
      log_warn(LD_BUG, "Found a connection %p that was supposed to be "
               "in pending_entry_connections, but wasn't. No worries; "
               "adding it.",
               pending_entry_connections);
      untried_pending_connections = 1;
      connection_ap_mark_as_pending_circuit(entry_conn);
    }

  } SMARTLIST_FOREACH_END(conn);

  connection_ap_attach_pending(1);
}

#ifdef DEBUGGING_17659
#define UNMARK() do {                           \
    entry_conn->marked_pending_circ_line = 0;   \
    entry_conn->marked_pending_circ_file = 0;   \
  } while (0)
#else /* !(defined(DEBUGGING_17659)) */
#define UNMARK() do { } while (0)
#endif /* defined(DEBUGGING_17659) */

/** Tell any AP streams that are listed as waiting for a new circuit to try
 * again.  If there is an available circuit for a stream, attach it. Otherwise,
 * launch a new circuit.
 *
 * If <b>retry</b> is false, only check the list if it contains at least one
 * streams that we have not yet tried to attach to a circuit.
 */
void
connection_ap_attach_pending(int retry)
{
  if (PREDICT_UNLIKELY(!pending_entry_connections)) {
    return;
  }

  if (untried_pending_connections == 0 && !retry)
    return;

  /* Don't allow any modifications to list while we are iterating over
   * it.  We'll put streams back on this list if we can't attach them
   * immediately. */
  smartlist_t *pending = pending_entry_connections;
  pending_entry_connections = smartlist_new();

  SMARTLIST_FOREACH_BEGIN(pending,
                          entry_connection_t *, entry_conn) {
    connection_t *conn = ENTRY_TO_CONN(entry_conn);
    tor_assert(conn && entry_conn);
    if (conn->marked_for_close) {
      UNMARK();
      continue;
    }
    if (conn->magic != ENTRY_CONNECTION_MAGIC) {
      log_warn(LD_BUG, "%p has impossible magic value %u.",
               entry_conn, (unsigned)conn->magic);
      UNMARK();
      continue;
    }
    if (conn->state != AP_CONN_STATE_CIRCUIT_WAIT) {
      log_warn(LD_BUG, "%p is no longer in circuit_wait. Its current state "
               "is %s. Why is it on pending_entry_connections?",
               entry_conn,
               conn_state_to_string(conn->type, conn->state));
      UNMARK();
      continue;
    }

    /* Okay, we're through the sanity checks. Try to handle this stream. */
    if (connection_ap_handshake_attach_circuit(entry_conn) < 0) {
      if (!conn->marked_for_close)
        connection_mark_unattached_ap(entry_conn,
                                      END_STREAM_REASON_CANT_ATTACH);
    }

    if (! conn->marked_for_close &&
        conn->type == CONN_TYPE_AP &&
        conn->state == AP_CONN_STATE_CIRCUIT_WAIT) {
      /* Is it still waiting for a circuit? If so, we didn't attach it,
       * so it's still pending.  Put it back on the list.
       */
      if (!smartlist_contains(pending_entry_connections, entry_conn)) {
        smartlist_add(pending_entry_connections, entry_conn);
        continue;
      }
    }

    /* If we got here, then we either closed the connection, or
     * we attached it. */
    UNMARK();
  } SMARTLIST_FOREACH_END(entry_conn);

  smartlist_free(pending);
  untried_pending_connections = 0;
}

static void
attach_pending_entry_connections_cb(mainloop_event_t *ev, void *arg)
{
  (void)ev;
  (void)arg;
  connection_ap_attach_pending(0);
}

/** Mark <b>entry_conn</b> as needing to get attached to a circuit.
 *
 * And <b>entry_conn</b> must be in AP_CONN_STATE_CIRCUIT_WAIT,
 * should not already be pending a circuit.  The circuit will get
 * launched or the connection will get attached the next time we
 * call connection_ap_attach_pending().
 */
void
connection_ap_mark_as_pending_circuit_(entry_connection_t *entry_conn,
                                       const char *fname, int lineno)
{
  connection_t *conn = ENTRY_TO_CONN(entry_conn);
  tor_assert(conn->state == AP_CONN_STATE_CIRCUIT_WAIT);
  tor_assert(conn->magic == ENTRY_CONNECTION_MAGIC);
  if (conn->marked_for_close)
    return;

  if (PREDICT_UNLIKELY(NULL == pending_entry_connections)) {
    pending_entry_connections = smartlist_new();
  }
  if (PREDICT_UNLIKELY(NULL == attach_pending_entry_connections_ev)) {
    attach_pending_entry_connections_ev = mainloop_event_postloop_new(
                                  attach_pending_entry_connections_cb, NULL);
  }
  if (PREDICT_UNLIKELY(smartlist_contains(pending_entry_connections,
                                          entry_conn))) {
    log_warn(LD_BUG, "What?? pending_entry_connections already contains %p! "
             "(Called from %s:%d.)",
             entry_conn, fname, lineno);
#ifdef DEBUGGING_17659
    const char *f2 = entry_conn->marked_pending_circ_file;
    log_warn(LD_BUG, "(Previously called from %s:%d.)\n",
             f2 ? f2 : "<NULL>",
             entry_conn->marked_pending_circ_line);
#endif /* defined(DEBUGGING_17659) */
    log_backtrace(LOG_WARN, LD_BUG, "To debug, this may help");
    return;
  }

#ifdef DEBUGGING_17659
  entry_conn->marked_pending_circ_line = (uint16_t) lineno;
  entry_conn->marked_pending_circ_file = fname;
#endif

  untried_pending_connections = 1;
  smartlist_add(pending_entry_connections, entry_conn);

  mainloop_event_activate(attach_pending_entry_connections_ev);
}

/** Mark <b>entry_conn</b> as no longer waiting for a circuit. */
void
connection_ap_mark_as_non_pending_circuit(entry_connection_t *entry_conn)
{
  if (PREDICT_UNLIKELY(NULL == pending_entry_connections))
    return;
  UNMARK();
  smartlist_remove(pending_entry_connections, entry_conn);
}

/** Mark <b>entry_conn</b> as waiting for a rendezvous descriptor. This
 * function will remove the entry connection from the waiting for a circuit
 * list (pending_entry_connections).
 *
 * This pattern is used across the code base because a connection in state
 * AP_CONN_STATE_RENDDESC_WAIT must not be in the pending list. */
void
connection_ap_mark_as_waiting_for_renddesc(entry_connection_t *entry_conn)
{
  tor_assert(entry_conn);

  connection_ap_mark_as_non_pending_circuit(entry_conn);
  ENTRY_TO_CONN(entry_conn)->state = AP_CONN_STATE_RENDDESC_WAIT;
}

/* DOCDOC */
void
connection_ap_warn_and_unmark_if_pending_circ(entry_connection_t *entry_conn,
                                              const char *where)
{
  if (pending_entry_connections &&
      smartlist_contains(pending_entry_connections, entry_conn)) {
    log_warn(LD_BUG, "What was %p doing in pending_entry_connections in %s?",
             entry_conn, where);
    connection_ap_mark_as_non_pending_circuit(entry_conn);
  }
}

/** Tell any AP streams that are waiting for a one-hop tunnel to
 * <b>failed_digest</b> that they are going to fail. */
/* XXXX We should get rid of this function, and instead attach
 * one-hop streams to circ->p_streams so they get marked in
 * circuit_mark_for_close like normal p_streams. */
void
connection_ap_fail_onehop(const char *failed_digest,
                          cpath_build_state_t *build_state)
{
  entry_connection_t *entry_conn;
  char digest[DIGEST_LEN];
  smartlist_t *conns = get_connection_array();
  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, conn) {
    if (conn->marked_for_close ||
        conn->type != CONN_TYPE_AP ||
        conn->state != AP_CONN_STATE_CIRCUIT_WAIT)
      continue;
    entry_conn = TO_ENTRY_CONN(conn);
    if (!entry_conn->want_onehop)
      continue;
    if (hexdigest_to_digest(entry_conn->chosen_exit_name, digest) < 0 ||
        tor_memneq(digest, failed_digest, DIGEST_LEN))
      continue;
    if (tor_digest_is_zero(digest)) {
      /* we don't know the digest; have to compare addr:port */
      tor_addr_t addr;
      if (!build_state || !build_state->chosen_exit ||
          !entry_conn->socks_request) {
        continue;
      }
      if (tor_addr_parse(&addr, entry_conn->socks_request->address)<0 ||
          !tor_addr_eq(&build_state->chosen_exit->addr, &addr) ||
          build_state->chosen_exit->port != entry_conn->socks_request->port)
        continue;
    }
    log_info(LD_APP, "Closing one-hop stream to '%s/%s' because the OR conn "
                     "just failed.", entry_conn->chosen_exit_name,
                     entry_conn->socks_request->address);
    connection_mark_unattached_ap(entry_conn, END_STREAM_REASON_TIMEOUT);
  } SMARTLIST_FOREACH_END(conn);
}

/** A circuit failed to finish on its last hop <b>info</b>. If there
 * are any streams waiting with this exit node in mind, but they
 * don't absolutely require it, make them give up on it.
 */
void
circuit_discard_optional_exit_enclaves(extend_info_t *info)
{
  entry_connection_t *entry_conn;
  const node_t *r1, *r2;

  smartlist_t *conns = get_connection_array();
  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, conn) {
    if (conn->marked_for_close ||
        conn->type != CONN_TYPE_AP ||
        conn->state != AP_CONN_STATE_CIRCUIT_WAIT)
      continue;
    entry_conn = TO_ENTRY_CONN(conn);
    if (!entry_conn->chosen_exit_optional &&
        !entry_conn->chosen_exit_retries)
      continue;
    r1 = node_get_by_nickname(entry_conn->chosen_exit_name,
                              NNF_NO_WARN_UNNAMED);
    r2 = node_get_by_id(info->identity_digest);
    if (!r1 || !r2 || r1 != r2)
      continue;
    tor_assert(entry_conn->socks_request);
    if (entry_conn->chosen_exit_optional) {
      log_info(LD_APP, "Giving up on enclave exit '%s' for destination %s.",
               safe_str_client(entry_conn->chosen_exit_name),
               escaped_safe_str_client(entry_conn->socks_request->address));
      entry_conn->chosen_exit_optional = 0;
      tor_free(entry_conn->chosen_exit_name); /* clears it */
      /* if this port is dangerous, warn or reject it now that we don't
       * think it'll be using an enclave. */
      consider_plaintext_ports(entry_conn, entry_conn->socks_request->port);
    }
    if (entry_conn->chosen_exit_retries) {
      if (--entry_conn->chosen_exit_retries == 0) { /* give up! */
        clear_trackexithost_mappings(entry_conn->chosen_exit_name);
        tor_free(entry_conn->chosen_exit_name); /* clears it */
        /* if this port is dangerous, warn or reject it now that we don't
         * think it'll be using an enclave. */
        consider_plaintext_ports(entry_conn, entry_conn->socks_request->port);
      }
    }
  } SMARTLIST_FOREACH_END(conn);
}

/** The AP connection <b>conn</b> has just failed while attaching or
 * sending a BEGIN or resolving on <b>circ</b>, but another circuit
 * might work. Detach the circuit, and either reattach it, launch a
 * new circuit, tell the controller, or give up as appropriate.
 *
 * Returns -1 on err, 1 on success, 0 on not-yet-sure.
 */
int
connection_ap_detach_retriable(entry_connection_t *conn,
                               origin_circuit_t *circ,
                               int reason)
{
  control_event_stream_status(conn, STREAM_EVENT_FAILED_RETRIABLE, reason);
  ENTRY_TO_CONN(conn)->timestamp_last_read_allowed = time(NULL);

  /* Roll back path bias use state so that we probe the circuit
   * if nothing else succeeds on it */
  pathbias_mark_use_rollback(circ);

  if (conn->pending_optimistic_data) {
    buf_set_to_copy(&conn->sending_optimistic_data,
                    conn->pending_optimistic_data);
  }

  if (!get_options()->LeaveStreamsUnattached || conn->use_begindir) {
    /* If we're attaching streams ourself, or if this connection is
     * a tunneled directory connection, then just attach it. */
    ENTRY_TO_CONN(conn)->state = AP_CONN_STATE_CIRCUIT_WAIT;
    circuit_detach_stream(TO_CIRCUIT(circ),ENTRY_TO_EDGE_CONN(conn));
    connection_ap_mark_as_pending_circuit(conn);
  } else {
    CONNECTION_AP_EXPECT_NONPENDING(conn);
    ENTRY_TO_CONN(conn)->state = AP_CONN_STATE_CONTROLLER_WAIT;
    circuit_detach_stream(TO_CIRCUIT(circ),ENTRY_TO_EDGE_CONN(conn));
  }
  return 0;
}

/** Check if <b>conn</b> is using a dangerous port. Then warn and/or
 * reject depending on our config options. */
static int
consider_plaintext_ports(entry_connection_t *conn, uint16_t port)
{
  const or_options_t *options = get_options();
  int reject = smartlist_contains_int_as_string(
                                     options->RejectPlaintextPorts, port);

  if (smartlist_contains_int_as_string(options->WarnPlaintextPorts, port)) {
    log_warn(LD_APP, "Application request to port %d: this port is "
             "commonly used for unencrypted protocols. Please make sure "
             "you don't send anything you would mind the rest of the "
             "Internet reading!%s", port, reject ? " Closing." : "");
    control_event_client_status(LOG_WARN, "DANGEROUS_PORT PORT=%d RESULT=%s",
                                port, reject ? "REJECT" : "WARN");
  }

  if (reject) {
    log_info(LD_APP, "Port %d listed in RejectPlaintextPorts. Closing.", port);
    connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
    return -1;
  }

  return 0;
}

/** How many times do we try connecting with an exit configured via
 * TrackHostExits before concluding that it won't work any more and trying a
 * different one? */
#define TRACKHOSTEXITS_RETRIES 5

/** Call connection_ap_handshake_rewrite_and_attach() unless a controller
 *  asked us to leave streams unattached. Return 0 in that case.
 *
 *  See connection_ap_handshake_rewrite_and_attach()'s
 *  documentation for arguments and return value.
 */
MOCK_IMPL(int,
connection_ap_rewrite_and_attach_if_allowed,(entry_connection_t *conn,
                                             origin_circuit_t *circ,
                                             crypt_path_t *cpath))
{
  const or_options_t *options = get_options();

  if (options->LeaveStreamsUnattached) {
    CONNECTION_AP_EXPECT_NONPENDING(conn);
    ENTRY_TO_CONN(conn)->state = AP_CONN_STATE_CONTROLLER_WAIT;
    return 0;
  }
  return connection_ap_handshake_rewrite_and_attach(conn, circ, cpath);
}

/* Try to perform any map-based rewriting of the target address in
 * <b>conn</b>, filling in the fields of <b>out</b> as we go, and modifying
 * conn->socks_request.address as appropriate.
 */
STATIC void
connection_ap_handshake_rewrite(entry_connection_t *conn,
                                rewrite_result_t *out)
{
  socks_request_t *socks = conn->socks_request;
  const or_options_t *options = get_options();
  tor_addr_t addr_tmp;

  /* Initialize all the fields of 'out' to reasonable defaults */
  out->automap = 0;
  out->exit_source = ADDRMAPSRC_NONE;
  out->map_expires = TIME_MAX;
  out->end_reason = 0;
  out->should_close = 0;
  out->orig_address[0] = 0;

  /* We convert all incoming addresses to lowercase. */
  tor_strlower(socks->address);
  /* Remember the original address. */
  strlcpy(out->orig_address, socks->address, sizeof(out->orig_address));
  log_debug(LD_APP,"Client asked for %s:%d",
            safe_str_client(socks->address),
            socks->port);

  /* Check for whether this is a .exit address.  By default, those are
   * disallowed when they're coming straight from the client, but you're
   * allowed to have them in MapAddress commands and so forth. */
  if (!strcmpend(socks->address, ".exit")) {
    log_warn(LD_APP, "The  \".exit\" notation is disabled in Tor due to "
             "security risks.");
    control_event_client_status(LOG_WARN, "SOCKS_BAD_HOSTNAME HOSTNAME=%s",
                                escaped(socks->address));
    out->end_reason = END_STREAM_REASON_TORPROTOCOL;
    out->should_close = 1;
    return;
  }

  /* Remember the original address so we can tell the user about what
   * they actually said, not just what it turned into. */
  /* XXX yes, this is the same as out->orig_address above. One is
   * in the output, and one is in the connection. */
  if (! conn->original_dest_address) {
    /* Is the 'if' necessary here? XXXX */
    conn->original_dest_address = tor_strdup(conn->socks_request->address);
  }

  /* First, apply MapAddress and MAPADDRESS mappings. We need to do
   * these only for non-reverse lookups, since they don't exist for those.
   * We also need to do this before we consider automapping, since we might
   * e.g. resolve irc.oftc.net into irconionaddress.onion, at which point
   * we'd need to automap it. */
  if (socks->command != SOCKS_COMMAND_RESOLVE_PTR) {
    const unsigned rewrite_flags = AMR_FLAG_USE_MAPADDRESS;
    if (addressmap_rewrite(socks->address, sizeof(socks->address),
                       rewrite_flags, &out->map_expires, &out->exit_source)) {
      control_event_stream_status(conn, STREAM_EVENT_REMAP,
                                  REMAP_STREAM_SOURCE_CACHE);
    }
  }

  /* Now see if we need to create or return an existing Hostname->IP
   * automapping.  Automapping happens when we're asked to resolve a
   * hostname, and AutomapHostsOnResolve is set, and the hostname has a
   * suffix listed in AutomapHostsSuffixes.  It's a handy feature
   * that lets you have Tor assign e.g. IPv6 addresses for .onion
   * names, and return them safely from DNSPort.
   */
  if (socks->command == SOCKS_COMMAND_RESOLVE &&
      tor_addr_parse(&addr_tmp, socks->address)<0 &&
      options->AutomapHostsOnResolve) {
    /* Check the suffix... */
    out->automap = addressmap_address_should_automap(socks->address, options);
    if (out->automap) {
      /* If we get here, then we should apply an automapping for this. */
      const char *new_addr;
      /* We return an IPv4 address by default, or an IPv6 address if we
       * are allowed to do so. */
      int addr_type = RESOLVED_TYPE_IPV4;
      if (conn->socks_request->socks_version != 4) {
        if (!conn->entry_cfg.ipv4_traffic ||
            (conn->entry_cfg.ipv6_traffic && conn->entry_cfg.prefer_ipv6) ||
            conn->entry_cfg.prefer_ipv6_virtaddr)
          addr_type = RESOLVED_TYPE_IPV6;
      }
      /* Okay, register the target address as automapped, and find the new
       * address we're supposed to give as a resolve answer.  (Return a cached
       * value if we've looked up this address before.
       */
      new_addr = addressmap_register_virtual_address(
                                    addr_type, tor_strdup(socks->address));
      if (! new_addr) {
        log_warn(LD_APP, "Unable to automap address %s",
                 escaped_safe_str(socks->address));
        out->end_reason = END_STREAM_REASON_INTERNAL;
        out->should_close = 1;
        return;
      }
      log_info(LD_APP, "Automapping %s to %s",
               escaped_safe_str_client(socks->address),
               safe_str_client(new_addr));
      strlcpy(socks->address, new_addr, sizeof(socks->address));
    }
  }

  /* Now handle reverse lookups, if they're in the cache.  This doesn't
   * happen too often, since client-side DNS caching is off by default,
   * and very deprecated. */
  if (socks->command == SOCKS_COMMAND_RESOLVE_PTR) {
    unsigned rewrite_flags = 0;
    if (conn->entry_cfg.use_cached_ipv4_answers)
      rewrite_flags |= AMR_FLAG_USE_IPV4_DNS;
    if (conn->entry_cfg.use_cached_ipv6_answers)
      rewrite_flags |= AMR_FLAG_USE_IPV6_DNS;

    if (addressmap_rewrite_reverse(socks->address, sizeof(socks->address),
                                   rewrite_flags, &out->map_expires)) {
      char *result = tor_strdup(socks->address);
      /* remember _what_ is supposed to have been resolved. */
      tor_snprintf(socks->address, sizeof(socks->address), "REVERSE[%s]",
                   out->orig_address);
      connection_ap_handshake_socks_resolved(conn, RESOLVED_TYPE_HOSTNAME,
                                             strlen(result), (uint8_t*)result,
                                             -1,
                                             out->map_expires);
      tor_free(result);
      out->end_reason = END_STREAM_REASON_DONE |
                        END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED;
      out->should_close = 1;
      return;
    }

    /* Hang on, did we find an answer saying that this is a reverse lookup for
     * an internal address?  If so, we should reject it if we're configured to
     * do so. */
    if (options->ClientDNSRejectInternalAddresses) {
      /* Don't let clients try to do a reverse lookup on 10.0.0.1. */
      tor_addr_t addr;
      int ok;
      ok = tor_addr_parse_PTR_name(
                               &addr, socks->address, AF_UNSPEC, 1);
      if (ok == 1 && tor_addr_is_internal(&addr, 0)) {
        connection_ap_handshake_socks_resolved(conn, RESOLVED_TYPE_ERROR,
                                               0, NULL, -1, TIME_MAX);
        out->end_reason = END_STREAM_REASON_SOCKSPROTOCOL |
                          END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED;
        out->should_close = 1;
        return;
      }
    }
  }

  /* If we didn't automap it before, then this is still the address that
   * came straight from the user, mapped according to any
   * MapAddress/MAPADDRESS commands.  Now apply other mappings,
   * including previously registered Automap entries (IP back to
   * hostname), TrackHostExits entries, and client-side DNS cache
   * entries (if they're turned on).
   */
  if (socks->command != SOCKS_COMMAND_RESOLVE_PTR &&
      !out->automap) {
    unsigned rewrite_flags = AMR_FLAG_USE_AUTOMAP | AMR_FLAG_USE_TRACKEXIT;
    addressmap_entry_source_t exit_source2;
    if (conn->entry_cfg.use_cached_ipv4_answers)
      rewrite_flags |= AMR_FLAG_USE_IPV4_DNS;
    if (conn->entry_cfg.use_cached_ipv6_answers)
      rewrite_flags |= AMR_FLAG_USE_IPV6_DNS;
    if (addressmap_rewrite(socks->address, sizeof(socks->address),
                        rewrite_flags, &out->map_expires, &exit_source2)) {
      control_event_stream_status(conn, STREAM_EVENT_REMAP,
                                  REMAP_STREAM_SOURCE_CACHE);
    }
    if (out->exit_source == ADDRMAPSRC_NONE) {
      /* If it wasn't a .exit before, maybe it turned into a .exit. Remember
       * the original source of a .exit. */
      out->exit_source = exit_source2;
    }
  }

  /* Check to see whether we're about to use an address in the virtual
   * range without actually having gotten it from an Automap. */
  if (!out->automap && address_is_in_virtual_range(socks->address)) {
    /* This address was probably handed out by
     * client_dns_get_unmapped_address, but the mapping was discarded for some
     * reason.  Or the user typed in a virtual address range manually.  We
     * *don't* want to send the address through Tor; that's likely to fail,
     * and may leak information.
     */
    log_warn(LD_APP,"Missing mapping for virtual address '%s'. Refusing.",
             safe_str_client(socks->address));
    out->end_reason = END_STREAM_REASON_INTERNAL;
    out->should_close = 1;
    return;
  }
}

/** We just received a SOCKS request in <b>conn</b> to an onion address of type
 *  <b>addresstype</b>. Start connecting to the onion service. */
static int
connection_ap_handle_onion(entry_connection_t *conn,
                           socks_request_t *socks,
                           origin_circuit_t *circ,
                           hostname_type_t addresstype)
{
  time_t now = approx_time();
  connection_t *base_conn = ENTRY_TO_CONN(conn);

  /* If .onion address requests are disabled, refuse the request */
  if (!conn->entry_cfg.onion_traffic) {
    log_warn(LD_APP, "Onion address %s requested from a port with .onion "
             "disabled", safe_str_client(socks->address));
    connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
    return -1;
  }

  /* Check whether it's RESOLVE or RESOLVE_PTR.  We don't handle those
   * for hidden service addresses. */
  if (SOCKS_COMMAND_IS_RESOLVE(socks->command)) {
    /* if it's a resolve request, fail it right now, rather than
     * building all the circuits and then realizing it won't work. */
    log_warn(LD_APP,
             "Resolve requests to hidden services not allowed. Failing.");
    connection_ap_handshake_socks_resolved(conn,RESOLVED_TYPE_ERROR,
                                           0,NULL,-1,TIME_MAX);
    connection_mark_unattached_ap(conn,
                               END_STREAM_REASON_SOCKSPROTOCOL |
                               END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);
    return -1;
  }

  /* If we were passed a circuit, then we need to fail.  .onion addresses
   * only work when we launch our own circuits for now. */
  if (circ) {
    log_warn(LD_CONTROL, "Attachstream to a circuit is not "
             "supported for .onion addresses currently. Failing.");
    connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
    return -1;
  }

  /* Interface: Regardless of HS version after the block below we should have
     set onion_address, rend_cache_lookup_result, and descriptor_is_usable. */
  const char *onion_address = NULL;
  int rend_cache_lookup_result = -ENOENT;
  int descriptor_is_usable = 0;

  if (addresstype == ONION_V2_HOSTNAME) { /* it's a v2 hidden service */
    rend_cache_entry_t *entry = NULL;
    /* Look up if we have client authorization configured for this hidden
     * service.  If we do, associate it with the rend_data. */
    rend_service_authorization_t *client_auth =
      rend_client_lookup_service_authorization(socks->address);

    const uint8_t *cookie = NULL;
    rend_auth_type_t auth_type = REND_NO_AUTH;
    if (client_auth) {
      log_info(LD_REND, "Using previously configured client authorization "
               "for hidden service request.");
      auth_type = client_auth->auth_type;
      cookie = client_auth->descriptor_cookie;
    }

    /* Fill in the rend_data field so we can start doing a connection to
     * a hidden service. */
    rend_data_t *rend_data = ENTRY_TO_EDGE_CONN(conn)->rend_data =
      rend_data_client_create(socks->address, NULL, (char *) cookie,
                              auth_type);
    if (rend_data == NULL) {
      return -1;
    }
    onion_address = rend_data_get_address(rend_data);
    log_info(LD_REND,"Got a hidden service request for ID '%s'",
             safe_str_client(onion_address));

    rend_cache_lookup_result = rend_cache_lookup_entry(onion_address,-1,
                                                       &entry);
    if (!rend_cache_lookup_result && entry) {
      descriptor_is_usable = rend_client_any_intro_points_usable(entry);
    }
  } else { /* it's a v3 hidden service */
    tor_assert(addresstype == ONION_V3_HOSTNAME);
    const hs_descriptor_t *cached_desc = NULL;
    int retval;
    /* Create HS conn identifier with HS pubkey */
    hs_ident_edge_conn_t *hs_conn_ident =
      tor_malloc_zero(sizeof(hs_ident_edge_conn_t));

    retval = hs_parse_address(socks->address, &hs_conn_ident->identity_pk,
                              NULL, NULL);
    if (retval < 0) {
      log_warn(LD_GENERAL, "failed to parse hs address");
      tor_free(hs_conn_ident);
      return -1;
    }
    ENTRY_TO_EDGE_CONN(conn)->hs_ident = hs_conn_ident;

    onion_address = socks->address;

    /* Check the v3 desc cache */
    cached_desc = hs_cache_lookup_as_client(&hs_conn_ident->identity_pk);
    if (cached_desc) {
      rend_cache_lookup_result = 0;
      descriptor_is_usable =
        hs_client_any_intro_points_usable(&hs_conn_ident->identity_pk,
                                          cached_desc);
      log_info(LD_GENERAL, "Found %s descriptor in cache for %s. %s.",
               (descriptor_is_usable) ? "usable" : "unusable",
               safe_str_client(onion_address),
               (descriptor_is_usable) ? "Not fetching." : "Refecting.");
    } else {
      rend_cache_lookup_result = -ENOENT;
    }
  }

  /* Lookup the given onion address. If invalid, stop right now.
   * Otherwise, we might have it in the cache or not. */
  unsigned int refetch_desc = 0;
  if (rend_cache_lookup_result < 0) {
    switch (-rend_cache_lookup_result) {
    case EINVAL:
      /* We should already have rejected this address! */
      log_warn(LD_BUG,"Invalid service name '%s'",
               safe_str_client(onion_address));
      connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
      return -1;
    case ENOENT:
      /* We didn't have this; we should look it up. */
      log_info(LD_REND, "No descriptor found in our cache for %s. Fetching.",
               safe_str_client(onion_address));
      refetch_desc = 1;
      break;
    default:
      log_warn(LD_BUG, "Unknown cache lookup error %d",
               rend_cache_lookup_result);
      return -1;
    }
  }

  /* Help predict that we'll want to do hidden service circuits in the
   * future. We're not sure if it will need a stable circuit yet, but
   * we know we'll need *something*. */
  rep_hist_note_used_internal(now, 0, 1);

  /* Now we have a descriptor but is it usable or not? If not, refetch.
   * Also, a fetch could have been requested if the onion address was not
   * found in the cache previously. */
  if (refetch_desc || !descriptor_is_usable) {
    edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(conn);
    connection_ap_mark_as_non_pending_circuit(conn);
    base_conn->state = AP_CONN_STATE_RENDDESC_WAIT;
    if (addresstype == ONION_V2_HOSTNAME) {
      tor_assert(edge_conn->rend_data);
      rend_client_refetch_v2_renddesc(edge_conn->rend_data);
      /* Whatever the result of the refetch, we don't go further. */
      return 0;
    } else {
      tor_assert(addresstype == ONION_V3_HOSTNAME);
      tor_assert(edge_conn->hs_ident);
      /* Attempt to fetch the hsv3 descriptor. Check the retval to see how it
       * went and act accordingly. */
      int ret = hs_client_refetch_hsdesc(&edge_conn->hs_ident->identity_pk);
      switch (ret) {
      case HS_CLIENT_FETCH_MISSING_INFO:
        /* Keeping the connection in descriptor wait state is fine because
         * once we get enough dirinfo or a new live consensus, the HS client
         * subsystem is notified and every connection in that state will
         * trigger a fetch for the service key. */
      case HS_CLIENT_FETCH_LAUNCHED:
      case HS_CLIENT_FETCH_PENDING:
      case HS_CLIENT_FETCH_HAVE_DESC:
        return 0;
      case HS_CLIENT_FETCH_ERROR:
      case HS_CLIENT_FETCH_NO_HSDIRS:
      case HS_CLIENT_FETCH_NOT_ALLOWED:
        /* Can't proceed further and better close the SOCKS request. */
        return -1;
      }
    }
  }

  /* We have the descriptor!  So launch a connection to the HS. */
  log_info(LD_REND, "Descriptor is here. Great.");

  base_conn->state = AP_CONN_STATE_CIRCUIT_WAIT;
  /* We'll try to attach it at the next event loop, or whenever
   * we call connection_ap_attach_pending() */
  connection_ap_mark_as_pending_circuit(conn);
  return 0;
}

/** Connection <b>conn</b> just finished its socks handshake, or the
 * controller asked us to take care of it. If <b>circ</b> is defined,
 * then that's where we'll want to attach it. Otherwise we have to
 * figure it out ourselves.
 *
 * First, parse whether it's a .exit address, remap it, and so on. Then
 * if it's for a general circuit, try to attach it to a circuit (or launch
 * one as needed), else if it's for a rendezvous circuit, fetch a
 * rendezvous descriptor first (or attach/launch a circuit if the
 * rendezvous descriptor is already here and fresh enough).
 *
 * The stream will exit from the hop
 * indicated by <b>cpath</b>, or from the last hop in circ's cpath if
 * <b>cpath</b> is NULL.
 */
int
connection_ap_handshake_rewrite_and_attach(entry_connection_t *conn,
                                           origin_circuit_t *circ,
                                           crypt_path_t *cpath)
{
  socks_request_t *socks = conn->socks_request;
  const or_options_t *options = get_options();
  connection_t *base_conn = ENTRY_TO_CONN(conn);
  time_t now = time(NULL);
  rewrite_result_t rr;

  /* First we'll do the rewrite part.  Let's see if we get a reasonable
   * answer.
   */
  memset(&rr, 0, sizeof(rr));
  connection_ap_handshake_rewrite(conn,&rr);

  if (rr.should_close) {
    /* connection_ap_handshake_rewrite told us to close the connection:
     * either because it sent back an answer, or because it sent back an
     * error */
    connection_mark_unattached_ap(conn, rr.end_reason);
    if (END_STREAM_REASON_DONE == (rr.end_reason & END_STREAM_REASON_MASK))
      return 0;
    else
      return -1;
  }

  const time_t map_expires = rr.map_expires;
  const int automap = rr.automap;
  const addressmap_entry_source_t exit_source = rr.exit_source;

  /* Now, we parse the address to see if it's an .onion or .exit or
   * other special address.
   */
  const hostname_type_t addresstype = parse_extended_hostname(socks->address);

  /* Now see whether the hostname is bogus.  This could happen because of an
   * onion hostname whose format we don't recognize. */
  if (addresstype == BAD_HOSTNAME) {
    control_event_client_status(LOG_WARN, "SOCKS_BAD_HOSTNAME HOSTNAME=%s",
                                escaped(socks->address));
    connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
    return -1;
  }

  /* If this is a .exit hostname, strip off the .name.exit part, and
   * see whether we're willing to connect there, and and otherwise handle the
   * .exit address.
   *
   * We'll set chosen_exit_name and/or close the connection as appropriate.
   */
  if (addresstype == EXIT_HOSTNAME) {
    /* If StrictNodes is not set, then .exit overrides ExcludeNodes but
     * not ExcludeExitNodes. */
    routerset_t *excludeset = options->StrictNodes ?
      options->ExcludeExitNodesUnion_ : options->ExcludeExitNodes;
    const node_t *node = NULL;

    /* If this .exit was added by an AUTOMAP, then it came straight from
     * a user.  That's not safe. */
    if (exit_source == ADDRMAPSRC_AUTOMAP) {
      /* Whoops; this one is stale.  It must have gotten added earlier?
       * (Probably this is not possible, since AllowDotExit no longer
       * exists.) */
      log_warn(LD_APP,"Stale automapped address for '%s.exit'. Refusing.",
               safe_str_client(socks->address));
      control_event_client_status(LOG_WARN, "SOCKS_BAD_HOSTNAME HOSTNAME=%s",
                                  escaped(socks->address));
      connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
      tor_assert_nonfatal_unreached();
      return -1;
    }

    /* Double-check to make sure there are no .exits coming from
     * impossible/weird sources. */
    if (exit_source == ADDRMAPSRC_DNS || exit_source == ADDRMAPSRC_NONE) {
      /* It shouldn't be possible to get a .exit address from any of these
       * sources. */
      log_warn(LD_BUG,"Address '%s.exit', with impossible source for the "
               ".exit part. Refusing.",
               safe_str_client(socks->address));
      control_event_client_status(LOG_WARN, "SOCKS_BAD_HOSTNAME HOSTNAME=%s",
                                  escaped(socks->address));
      connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
      return -1;
    }

    tor_assert(!automap);

    /* Now, find the character before the .(name) part.
     * (The ".exit" part got stripped off by "parse_extended_hostname").
     *
     * We're going to put the exit name into conn->chosen_exit_name, and
     * look up a node correspondingly. */
    char *s = strrchr(socks->address,'.');
    if (s) {
      /* The address was of the form "(stuff).(name).exit */
      if (s[1] != '\0') {
        /* Looks like a real .exit one. */
        conn->chosen_exit_name = tor_strdup(s+1);
        node = node_get_by_nickname(conn->chosen_exit_name, 0);

        if (exit_source == ADDRMAPSRC_TRACKEXIT) {
          /* We 5 tries before it expires the addressmap */
          conn->chosen_exit_retries = TRACKHOSTEXITS_RETRIES;
        }
        *s = 0;
      } else {
        /* Oops, the address was (stuff)..exit.  That's not okay. */
        log_warn(LD_APP,"Malformed exit address '%s.exit'. Refusing.",
                 safe_str_client(socks->address));
        control_event_client_status(LOG_WARN, "SOCKS_BAD_HOSTNAME HOSTNAME=%s",
                                    escaped(socks->address));
        connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
        return -1;
      }
    } else {
      /* It looks like they just asked for "foo.exit".  That's a special
       * form that means (foo's address).foo.exit. */

      conn->chosen_exit_name = tor_strdup(socks->address);
      node = node_get_by_nickname(conn->chosen_exit_name, 0);
      if (node) {
        *socks->address = 0;
        node_get_address_string(node, socks->address, sizeof(socks->address));
      }
    }

    /* Now make sure that the chosen exit exists... */
    if (!node) {
      log_warn(LD_APP,
               "Unrecognized relay in exit address '%s.exit'. Refusing.",
               safe_str_client(socks->address));
      connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
      return -1;
    }
    /* ...and make sure that it isn't excluded. */
    if (routerset_contains_node(excludeset, node)) {
      log_warn(LD_APP,
               "Excluded relay in exit address '%s.exit'. Refusing.",
               safe_str_client(socks->address));
      connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
      return -1;
    }
    /* XXXX-1090 Should we also allow foo.bar.exit if ExitNodes is set and
       Bar is not listed in it?  I say yes, but our revised manpage branch
       implies no. */
  }

  /* Now, we handle everything that isn't a .onion address. */
  if (addresstype != ONION_V2_HOSTNAME && addresstype != ONION_V3_HOSTNAME) {
    /* Not a hidden-service request.  It's either a hostname or an IP,
     * possibly with a .exit that we stripped off.  We're going to check
     * if we're allowed to connect/resolve there, and then launch the
     * appropriate request. */

    /* Check for funny characters in the address. */
    if (address_is_invalid_destination(socks->address, 1)) {
      control_event_client_status(LOG_WARN, "SOCKS_BAD_HOSTNAME HOSTNAME=%s",
                                  escaped(socks->address));
      log_warn(LD_APP,
               "Destination '%s' seems to be an invalid hostname. Failing.",
               safe_str_client(socks->address));
      connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
      return -1;
    }

    /* socks->address is a non-onion hostname or IP address.
     * If we can't do any non-onion requests, refuse the connection.
     * If we have a hostname but can't do DNS, refuse the connection.
     * If we have an IP address, but we can't use that address family,
     * refuse the connection.
     *
     * If we can do DNS requests, and we can use at least one address family,
     * then we have to resolve the address first. Then we'll know if it
     * resolves to a usable address family. */

    /* First, check if all non-onion traffic is disabled */
    if (!conn->entry_cfg.dns_request && !conn->entry_cfg.ipv4_traffic
        && !conn->entry_cfg.ipv6_traffic) {
        log_warn(LD_APP, "Refusing to connect to non-hidden-service hostname "
                 "or IP address %s because Port has OnionTrafficOnly set (or "
                 "NoDNSRequest, NoIPv4Traffic, and NoIPv6Traffic).",
                 safe_str_client(socks->address));
        connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
        return -1;
    }

    /* Then check if we have a hostname or IP address, and whether DNS or
     * the IP address family are permitted.  Reject if not. */
    tor_addr_t dummy_addr;
    int socks_family = tor_addr_parse(&dummy_addr, socks->address);
    /* family will be -1 for a non-onion hostname that's not an IP */
    if (socks_family == -1) {
      if (!conn->entry_cfg.dns_request) {
        log_warn(LD_APP, "Refusing to connect to hostname %s "
                 "because Port has NoDNSRequest set.",
                 safe_str_client(socks->address));
        connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
        return -1;
      }
    } else if (socks_family == AF_INET) {
      if (!conn->entry_cfg.ipv4_traffic) {
        log_warn(LD_APP, "Refusing to connect to IPv4 address %s because "
                 "Port has NoIPv4Traffic set.",
                 safe_str_client(socks->address));
        connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
        return -1;
      }
    } else if (socks_family == AF_INET6) {
      if (!conn->entry_cfg.ipv6_traffic) {
        log_warn(LD_APP, "Refusing to connect to IPv6 address %s because "
                 "Port has NoIPv6Traffic set.",
                 safe_str_client(socks->address));
        connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
        return -1;
      }
    } else {
      tor_assert_nonfatal_unreached_once();
    }

    /* See if this is a hostname lookup that we can answer immediately.
     * (For example, an attempt to look up the IP address for an IP address.)
     */
    if (socks->command == SOCKS_COMMAND_RESOLVE) {
      tor_addr_t answer;
      /* Reply to resolves immediately if we can. */
      if (tor_addr_parse(&answer, socks->address) >= 0) {/* is it an IP? */
        /* remember _what_ is supposed to have been resolved. */
        strlcpy(socks->address, rr.orig_address, sizeof(socks->address));
        connection_ap_handshake_socks_resolved_addr(conn, &answer, -1,
                                                    map_expires);
        connection_mark_unattached_ap(conn,
                                END_STREAM_REASON_DONE |
                                END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);
        return 0;
      }
      tor_assert(!automap);
      rep_hist_note_used_resolve(now); /* help predict this next time */
    } else if (socks->command == SOCKS_COMMAND_CONNECT) {
      /* Now see if this is a connect request that we can reject immediately */

      tor_assert(!automap);
      /* Don't allow connections to port 0. */
      if (socks->port == 0) {
        log_notice(LD_APP,"Application asked to connect to port 0. Refusing.");
        connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
        return -1;
      }
      /* You can't make connections to internal addresses, by default.
       * Exceptions are begindir requests (where the address is meaningless),
       * or cases where you've hand-configured a particular exit, thereby
       * making the local address meaningful. */
      if (options->ClientRejectInternalAddresses &&
          !conn->use_begindir && !conn->chosen_exit_name && !circ) {
        /* If we reach this point then we don't want to allow internal
         * addresses.  Check if we got one. */
        tor_addr_t addr;
        if (tor_addr_hostname_is_local(socks->address) ||
            (tor_addr_parse(&addr, socks->address) >= 0 &&
             tor_addr_is_internal(&addr, 0))) {
          /* If this is an explicit private address with no chosen exit node,
           * then we really don't want to try to connect to it.  That's
           * probably an error. */
          if (conn->is_transparent_ap) {
#define WARN_INTRVL_LOOP 300
            static ratelim_t loop_warn_limit = RATELIM_INIT(WARN_INTRVL_LOOP);
            char *m;
            if ((m = rate_limit_log(&loop_warn_limit, approx_time()))) {
              log_warn(LD_NET,
                       "Rejecting request for anonymous connection to private "
                       "address %s on a TransPort or NATDPort.  Possible loop "
                       "in your NAT rules?%s", safe_str_client(socks->address),
                       m);
              tor_free(m);
            }
          } else {
#define WARN_INTRVL_PRIV 300
            static ratelim_t priv_warn_limit = RATELIM_INIT(WARN_INTRVL_PRIV);
            char *m;
            if ((m = rate_limit_log(&priv_warn_limit, approx_time()))) {
              log_warn(LD_NET,
                       "Rejecting SOCKS request for anonymous connection to "
                       "private address %s.%s",
                       safe_str_client(socks->address),m);
              tor_free(m);
            }
          }
          connection_mark_unattached_ap(conn, END_STREAM_REASON_PRIVATE_ADDR);
          return -1;
        }
      } /* end "if we should check for internal addresses" */

      /* Okay.  We're still doing a CONNECT, and it wasn't a private
       * address.  Here we do special handling for literal IP addresses,
       * to see if we should reject this preemptively, and to set up
       * fields in conn->entry_cfg to tell the exit what AF we want. */
      {
        tor_addr_t addr;
        /* XXX Duplicate call to tor_addr_parse. */
        if (tor_addr_parse(&addr, socks->address) >= 0) {
          /* If we reach this point, it's an IPv4 or an IPv6 address. */
          sa_family_t family = tor_addr_family(&addr);

          if ((family == AF_INET && ! conn->entry_cfg.ipv4_traffic) ||
              (family == AF_INET6 && ! conn->entry_cfg.ipv6_traffic)) {
            /* You can't do an IPv4 address on a v6-only socks listener,
             * or vice versa. */
            log_warn(LD_NET, "Rejecting SOCKS request for an IP address "
                     "family that this listener does not support.");
            connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
            return -1;
          } else if (family == AF_INET6 && socks->socks_version == 4) {
            /* You can't make a socks4 request to an IPv6 address. Socks4
             * doesn't support that. */
            log_warn(LD_NET, "Rejecting SOCKS4 request for an IPv6 address.");
            connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
            return -1;
          } else if (socks->socks_version == 4 &&
                     !conn->entry_cfg.ipv4_traffic) {
            /* You can't do any kind of Socks4 request when IPv4 is forbidden.
             *
             * XXX raise this check outside the enclosing block? */
            log_warn(LD_NET, "Rejecting SOCKS4 request on a listener with "
                     "no IPv4 traffic supported.");
            connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
            return -1;
          } else if (family == AF_INET6) {
            /* Tell the exit: we won't accept any ipv4 connection to an IPv6
             * address. */
            conn->entry_cfg.ipv4_traffic = 0;
          } else if (family == AF_INET) {
            /* Tell the exit: we won't accept any ipv6 connection to an IPv4
             * address. */
            conn->entry_cfg.ipv6_traffic = 0;
          }
        }
      }

      /* we never allow IPv6 answers on socks4. (TODO: Is this smart?) */
      if (socks->socks_version == 4)
        conn->entry_cfg.ipv6_traffic = 0;

      /* Still handling CONNECT. Now, check for exit enclaves.  (Which we
       * don't do on BEGIN_DIR, or when there is a chosen exit.)
       *
       * TODO: Should we remove this?  Exit enclaves are nutty and don't
       * work very well
       */
      if (!conn->use_begindir && !conn->chosen_exit_name && !circ) {
        /* see if we can find a suitable enclave exit */
        const node_t *r =
          router_find_exact_exit_enclave(socks->address, socks->port);
        if (r) {
          log_info(LD_APP,
                   "Redirecting address %s to exit at enclave router %s",
                   safe_str_client(socks->address), node_describe(r));
          /* use the hex digest, not nickname, in case there are two
             routers with this nickname */
          conn->chosen_exit_name =
            tor_strdup(hex_str(r->identity, DIGEST_LEN));
          conn->chosen_exit_optional = 1;
        }
      }

      /* Still handling CONNECT: warn or reject if it's using a dangerous
       * port. */
      if (!conn->use_begindir && !conn->chosen_exit_name && !circ)
        if (consider_plaintext_ports(conn, socks->port) < 0)
          return -1;

      /* Remember the port so that we will predict that more requests
         there will happen in the future. */
      if (!conn->use_begindir) {
        /* help predict this next time */
        rep_hist_note_used_port(now, socks->port);
      }
    } else if (socks->command == SOCKS_COMMAND_RESOLVE_PTR) {
      rep_hist_note_used_resolve(now); /* help predict this next time */
      /* no extra processing needed */
    } else {
      /* We should only be doing CONNECT, RESOLVE, or RESOLVE_PTR! */
      tor_fragile_assert();
    }

    /* Okay. At this point we've set chosen_exit_name if needed, rewritten the
     * address, and decided not to reject it for any number of reasons. Now
     * mark the connection as waiting for a circuit, and try to attach it!
     */
    base_conn->state = AP_CONN_STATE_CIRCUIT_WAIT;

    /* If we were given a circuit to attach to, try to attach. Otherwise,
     * try to find a good one and attach to that. */
    int rv;
    if (circ) {
      rv = connection_ap_handshake_attach_chosen_circuit(conn, circ, cpath);
    } else {
      /* We'll try to attach it at the next event loop, or whenever
       * we call connection_ap_attach_pending() */
      connection_ap_mark_as_pending_circuit(conn);
      rv = 0;
    }

    /* If the above function returned 0 then we're waiting for a circuit.
     * if it returned 1, we're attached.  Both are okay.  But if it returned
     * -1, there was an error, so make sure the connection is marked, and
     * return -1. */
    if (rv < 0) {
      if (!base_conn->marked_for_close)
        connection_mark_unattached_ap(conn, END_STREAM_REASON_CANT_ATTACH);
      return -1;
    }

    return 0;
  } else {
    /* If we get here, it's a request for a .onion address! */
    tor_assert(addresstype == ONION_V2_HOSTNAME ||
               addresstype == ONION_V3_HOSTNAME);
    tor_assert(!automap);
    return connection_ap_handle_onion(conn, socks, circ, addresstype);
  }

  return 0; /* unreached but keeps the compiler happy */
}

#ifdef TRANS_PF
static int pf_socket = -1;
int
get_pf_socket(void)
{
  int pf;
  /*  This should be opened before dropping privileges. */
  if (pf_socket >= 0)
    return pf_socket;

#if defined(OpenBSD)
  /* only works on OpenBSD */
  pf = tor_open_cloexec("/dev/pf", O_RDONLY, 0);
#else
  /* works on NetBSD and FreeBSD */
  pf = tor_open_cloexec("/dev/pf", O_RDWR, 0);
#endif /* defined(OpenBSD) */

  if (pf < 0) {
    log_warn(LD_NET, "open(\"/dev/pf\") failed: %s", strerror(errno));
    return -1;
  }

  pf_socket = pf;
  return pf_socket;
}
#endif /* defined(TRANS_PF) */

#if defined(TRANS_NETFILTER) || defined(TRANS_PF) || \
  defined(TRANS_TPROXY)
/** Try fill in the address of <b>req</b> from the socket configured
 * with <b>conn</b>. */
static int
destination_from_socket(entry_connection_t *conn, socks_request_t *req)
{
  struct sockaddr_storage orig_dst;
  socklen_t orig_dst_len = sizeof(orig_dst);
  tor_addr_t addr;

#ifdef TRANS_TPROXY
  if (get_options()->TransProxyType_parsed == TPT_TPROXY) {
    if (getsockname(ENTRY_TO_CONN(conn)->s, (struct sockaddr*)&orig_dst,
                    &orig_dst_len) < 0) {
      int e = tor_socket_errno(ENTRY_TO_CONN(conn)->s);
      log_warn(LD_NET, "getsockname() failed: %s", tor_socket_strerror(e));
      return -1;
    }
    goto done;
  }
#endif /* defined(TRANS_TPROXY) */

#ifdef TRANS_NETFILTER
  int rv = -1;
  switch (ENTRY_TO_CONN(conn)->socket_family) {
#ifdef TRANS_NETFILTER_IPV4
    case AF_INET:
      rv = getsockopt(ENTRY_TO_CONN(conn)->s, SOL_IP, SO_ORIGINAL_DST,
                  (struct sockaddr*)&orig_dst, &orig_dst_len);
      break;
#endif /* defined(TRANS_NETFILTER_IPV4) */
#ifdef TRANS_NETFILTER_IPV6
    case AF_INET6:
      rv = getsockopt(ENTRY_TO_CONN(conn)->s, SOL_IPV6, IP6T_SO_ORIGINAL_DST,
                  (struct sockaddr*)&orig_dst, &orig_dst_len);
      break;
#endif /* defined(TRANS_NETFILTER_IPV6) */
    default:
      log_warn(LD_BUG,
               "Received transparent data from an unsuported socket family %d",
               ENTRY_TO_CONN(conn)->socket_family);
      return -1;
  }
  if (rv < 0) {
    int e = tor_socket_errno(ENTRY_TO_CONN(conn)->s);
    log_warn(LD_NET, "getsockopt() failed: %s", tor_socket_strerror(e));
    return -1;
  }
  goto done;
#elif defined(TRANS_PF)
  if (getsockname(ENTRY_TO_CONN(conn)->s, (struct sockaddr*)&orig_dst,
                  &orig_dst_len) < 0) {
    int e = tor_socket_errno(ENTRY_TO_CONN(conn)->s);
    log_warn(LD_NET, "getsockname() failed: %s", tor_socket_strerror(e));
    return -1;
  }
  goto done;
#else
  (void)conn;
  (void)req;
  log_warn(LD_BUG, "Unable to determine destination from socket.");
  return -1;
#endif /* defined(TRANS_NETFILTER) || ... */

 done:
  tor_addr_from_sockaddr(&addr, (struct sockaddr*)&orig_dst, &req->port);
  tor_addr_to_str(req->address, &addr, sizeof(req->address), 1);

  return 0;
}
#endif /* defined(TRANS_NETFILTER) || defined(TRANS_PF) || ... */

#ifdef TRANS_PF
static int
destination_from_pf(entry_connection_t *conn, socks_request_t *req)
{
  struct sockaddr_storage proxy_addr;
  socklen_t proxy_addr_len = sizeof(proxy_addr);
  struct sockaddr *proxy_sa = (struct sockaddr*) &proxy_addr;
  struct pfioc_natlook pnl;
  tor_addr_t addr;
  int pf = -1;

  if (getsockname(ENTRY_TO_CONN(conn)->s, (struct sockaddr*)&proxy_addr,
                  &proxy_addr_len) < 0) {
    int e = tor_socket_errno(ENTRY_TO_CONN(conn)->s);
    log_warn(LD_NET, "getsockname() to determine transocks destination "
             "failed: %s", tor_socket_strerror(e));
    return -1;
  }

#ifdef __FreeBSD__
  if (get_options()->TransProxyType_parsed == TPT_IPFW) {
    /* ipfw(8) is used and in this case getsockname returned the original
       destination */
    if (tor_addr_from_sockaddr(&addr, proxy_sa, &req->port) < 0) {
      tor_fragile_assert();
      return -1;
    }

    tor_addr_to_str(req->address, &addr, sizeof(req->address), 0);

    return 0;
  }
#endif /* defined(__FreeBSD__) */

  memset(&pnl, 0, sizeof(pnl));
  pnl.proto           = IPPROTO_TCP;
  pnl.direction       = PF_OUT;
  if (proxy_sa->sa_family == AF_INET) {
    struct sockaddr_in *sin = (struct sockaddr_in *)proxy_sa;
    pnl.af              = AF_INET;
    pnl.saddr.v4.s_addr = tor_addr_to_ipv4n(&ENTRY_TO_CONN(conn)->addr);
    pnl.sport           = htons(ENTRY_TO_CONN(conn)->port);
    pnl.daddr.v4.s_addr = sin->sin_addr.s_addr;
    pnl.dport           = sin->sin_port;
  } else if (proxy_sa->sa_family == AF_INET6) {
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)proxy_sa;
    pnl.af = AF_INET6;
    memcpy(&pnl.saddr.v6, tor_addr_to_in6(&ENTRY_TO_CONN(conn)->addr),
           sizeof(struct in6_addr));
    pnl.sport = htons(ENTRY_TO_CONN(conn)->port);
    memcpy(&pnl.daddr.v6, &sin6->sin6_addr, sizeof(struct in6_addr));
    pnl.dport = sin6->sin6_port;
  } else {
    log_warn(LD_NET, "getsockname() gave an unexpected address family (%d)",
             (int)proxy_sa->sa_family);
    return -1;
  }

  pf = get_pf_socket();
  if (pf<0)
    return -1;

  if (ioctl(pf, DIOCNATLOOK, &pnl) < 0) {
    log_warn(LD_NET, "ioctl(DIOCNATLOOK) failed: %s", strerror(errno));
    return -1;
  }

  if (pnl.af == AF_INET) {
    tor_addr_from_ipv4n(&addr, pnl.rdaddr.v4.s_addr);
  } else if (pnl.af == AF_INET6) {
    tor_addr_from_in6(&addr, &pnl.rdaddr.v6);
  } else {
    tor_fragile_assert();
    return -1;
  }

  tor_addr_to_str(req->address, &addr, sizeof(req->address), 1);
  req->port = ntohs(pnl.rdport);

  return 0;
}
#endif /* defined(TRANS_PF) */

/** Fetch the original destination address and port from a
 * system-specific interface and put them into a
 * socks_request_t as if they came from a socks request.
 *
 * Return -1 if an error prevents fetching the destination,
 * else return 0.
 */
static int
connection_ap_get_original_destination(entry_connection_t *conn,
                                       socks_request_t *req)
{
#ifdef TRANS_NETFILTER
  return destination_from_socket(conn, req);
#elif defined(TRANS_PF)
  const or_options_t *options = get_options();

  if (options->TransProxyType_parsed == TPT_PF_DIVERT)
    return destination_from_socket(conn, req);

  if (options->TransProxyType_parsed == TPT_DEFAULT ||
      options->TransProxyType_parsed == TPT_IPFW)
    return destination_from_pf(conn, req);

  (void)conn;
  (void)req;
  log_warn(LD_BUG, "Proxy destination determination mechanism %s unknown.",
           options->TransProxyType);
  return -1;
#else
  (void)conn;
  (void)req;
  log_warn(LD_BUG, "Called connection_ap_get_original_destination, but no "
           "transparent proxy method was configured.");
  return -1;
#endif /* defined(TRANS_NETFILTER) || ... */
}

/** connection_edge_process_inbuf() found a conn in state
 * socks_wait. See if conn->inbuf has the right bytes to proceed with
 * the socks handshake.
 *
 * If the handshake is complete, send it to
 * connection_ap_handshake_rewrite_and_attach().
 *
 * Return -1 if an unexpected error with conn occurs (and mark it for close),
 * else return 0.
 */
static int
connection_ap_handshake_process_socks(entry_connection_t *conn)
{
  socks_request_t *socks;
  int sockshere;
  const or_options_t *options = get_options();
  int had_reply = 0;
  connection_t *base_conn = ENTRY_TO_CONN(conn);

  tor_assert(conn);
  tor_assert(base_conn->type == CONN_TYPE_AP);
  tor_assert(base_conn->state == AP_CONN_STATE_SOCKS_WAIT);
  tor_assert(conn->socks_request);
  socks = conn->socks_request;

  log_debug(LD_APP,"entered.");

  sockshere = fetch_from_buf_socks(base_conn->inbuf, socks,
                                   options->TestSocks, options->SafeSocks);

  if (socks->replylen) {
    had_reply = 1;
    connection_buf_add((const char*)socks->reply, socks->replylen,
                            base_conn);
    socks->replylen = 0;
    if (sockshere == -1) {
      /* An invalid request just got a reply, no additional
       * one is necessary. */
      socks->has_finished = 1;
    }
  }

  if (sockshere == 0) {
    log_debug(LD_APP,"socks handshake not all here yet.");
    return 0;
  } else if (sockshere == -1) {
    if (!had_reply) {
      log_warn(LD_APP,"Fetching socks handshake failed. Closing.");
      connection_ap_handshake_socks_reply(conn, NULL, 0,
                                          END_STREAM_REASON_SOCKSPROTOCOL);
    }
    connection_mark_unattached_ap(conn,
                              END_STREAM_REASON_SOCKSPROTOCOL |
                              END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);
    return -1;
  } /* else socks handshake is done, continue processing */

  if (SOCKS_COMMAND_IS_CONNECT(socks->command))
    control_event_stream_status(conn, STREAM_EVENT_NEW, 0);
  else
    control_event_stream_status(conn, STREAM_EVENT_NEW_RESOLVE, 0);

  return connection_ap_rewrite_and_attach_if_allowed(conn, NULL, NULL);
}

/** connection_init_accepted_conn() found a new trans AP conn.
 * Get the original destination and send it to
 * connection_ap_handshake_rewrite_and_attach().
 *
 * Return -1 if an unexpected error with conn (and it should be marked
 * for close), else return 0.
 */
int
connection_ap_process_transparent(entry_connection_t *conn)
{
  socks_request_t *socks;

  tor_assert(conn);
  tor_assert(conn->socks_request);
  socks = conn->socks_request;

  /* pretend that a socks handshake completed so we don't try to
   * send a socks reply down a transparent conn */
  socks->command = SOCKS_COMMAND_CONNECT;
  socks->has_finished = 1;

  log_debug(LD_APP,"entered.");

  if (connection_ap_get_original_destination(conn, socks) < 0) {
    log_warn(LD_APP,"Fetching original destination failed. Closing.");
    connection_mark_unattached_ap(conn,
                               END_STREAM_REASON_CANT_FETCH_ORIG_DEST);
    return -1;
  }
  /* we have the original destination */

  control_event_stream_status(conn, STREAM_EVENT_NEW, 0);

  return connection_ap_rewrite_and_attach_if_allowed(conn, NULL, NULL);
}

/** connection_edge_process_inbuf() found a conn in state natd_wait. See if
 * conn-\>inbuf has the right bytes to proceed.  See FreeBSD's libalias(3) and
 * ProxyEncodeTcpStream() in src/lib/libalias/alias_proxy.c for the encoding
 * form of the original destination.
 *
 * If the original destination is complete, send it to
 * connection_ap_handshake_rewrite_and_attach().
 *
 * Return -1 if an unexpected error with conn (and it should be marked
 * for close), else return 0.
 */
static int
connection_ap_process_natd(entry_connection_t *conn)
{
  char tmp_buf[36], *tbuf, *daddr;
  size_t tlen = 30;
  int err, port_ok;
  socks_request_t *socks;

  tor_assert(conn);
  tor_assert(ENTRY_TO_CONN(conn)->state == AP_CONN_STATE_NATD_WAIT);
  tor_assert(conn->socks_request);
  socks = conn->socks_request;

  log_debug(LD_APP,"entered.");

  /* look for LF-terminated "[DEST ip_addr port]"
   * where ip_addr is a dotted-quad and port is in string form */
  err = connection_buf_get_line(ENTRY_TO_CONN(conn), tmp_buf, &tlen);
  if (err == 0)
    return 0;
  if (err < 0) {
    log_warn(LD_APP,"NATD handshake failed (DEST too long). Closing");
    connection_mark_unattached_ap(conn, END_STREAM_REASON_INVALID_NATD_DEST);
    return -1;
  }

  if (strcmpstart(tmp_buf, "[DEST ")) {
    log_warn(LD_APP,"NATD handshake was ill-formed; closing. The client "
             "said: %s",
             escaped(tmp_buf));
    connection_mark_unattached_ap(conn, END_STREAM_REASON_INVALID_NATD_DEST);
    return -1;
  }

  daddr = tbuf = &tmp_buf[0] + 6; /* after end of "[DEST " */
  if (!(tbuf = strchr(tbuf, ' '))) {
    log_warn(LD_APP,"NATD handshake was ill-formed; closing. The client "
             "said: %s",
             escaped(tmp_buf));
    connection_mark_unattached_ap(conn, END_STREAM_REASON_INVALID_NATD_DEST);
    return -1;
  }
  *tbuf++ = '\0';

  /* pretend that a socks handshake completed so we don't try to
   * send a socks reply down a natd conn */
  strlcpy(socks->address, daddr, sizeof(socks->address));
  socks->port = (uint16_t)
    tor_parse_long(tbuf, 10, 1, 65535, &port_ok, &daddr);
  if (!port_ok) {
    log_warn(LD_APP,"NATD handshake failed; port %s is ill-formed or out "
             "of range.", escaped(tbuf));
    connection_mark_unattached_ap(conn, END_STREAM_REASON_INVALID_NATD_DEST);
    return -1;
  }

  socks->command = SOCKS_COMMAND_CONNECT;
  socks->has_finished = 1;

  control_event_stream_status(conn, STREAM_EVENT_NEW, 0);

  ENTRY_TO_CONN(conn)->state = AP_CONN_STATE_CIRCUIT_WAIT;

  return connection_ap_rewrite_and_attach_if_allowed(conn, NULL, NULL);
}

/** Called on an HTTP CONNECT entry connection when some bytes have arrived,
 * but we have not yet received a full HTTP CONNECT request.  Try to parse an
 * HTTP CONNECT request from the connection's inbuf.  On success, set up the
 * connection's socks_request field and try to attach the connection.  On
 * failure, send an HTTP reply, and mark the connection.
 */
STATIC int
connection_ap_process_http_connect(entry_connection_t *conn)
{
  if (BUG(ENTRY_TO_CONN(conn)->state != AP_CONN_STATE_HTTP_CONNECT_WAIT))
    return -1;

  char *headers = NULL, *body = NULL;
  char *command = NULL, *addrport = NULL;
  char *addr = NULL;
  size_t bodylen = 0;

  const char *errmsg = NULL;
  int rv = 0;

  const int http_status =
    fetch_from_buf_http(ENTRY_TO_CONN(conn)->inbuf, &headers, 8192,
                        &body, &bodylen, 1024, 0);
  if (http_status < 0) {
    /* Bad http status */
    errmsg = "HTTP/1.0 400 Bad Request\r\n\r\n";
    goto err;
  } else if (http_status == 0) {
    /* no HTTP request yet. */
    goto done;
  }

  const int cmd_status = parse_http_command(headers, &command, &addrport);
  if (cmd_status < 0) {
    errmsg = "HTTP/1.0 400 Bad Request\r\n\r\n";
    goto err;
  }
  tor_assert(command);
  tor_assert(addrport);
  if (strcasecmp(command, "connect")) {
    errmsg = "HTTP/1.0 405 Method Not Allowed\r\n\r\n";
    goto err;
  }

  tor_assert(conn->socks_request);
  socks_request_t *socks = conn->socks_request;
  uint16_t port;
  if (tor_addr_port_split(LOG_WARN, addrport, &addr, &port) < 0) {
    errmsg = "HTTP/1.0 400 Bad Request\r\n\r\n";
    goto err;
  }
  if (strlen(addr) >= MAX_SOCKS_ADDR_LEN) {
    errmsg = "HTTP/1.0 414 Request-URI Too Long\r\n\r\n";
    goto err;
  }

  /* Abuse the 'username' and 'password' fields here. They are already an
  * abuse. */
  {
    char *authorization = http_get_header(headers, "Proxy-Authorization: ");
    if (authorization) {
      socks->username = authorization; // steal reference
      socks->usernamelen = strlen(authorization);
    }
    char *isolation = http_get_header(headers, "X-Tor-Stream-Isolation: ");
    if (isolation) {
      socks->password = isolation; // steal reference
      socks->passwordlen = strlen(isolation);
    }
  }

  socks->command = SOCKS_COMMAND_CONNECT;
  socks->listener_type = CONN_TYPE_AP_HTTP_CONNECT_LISTENER;
  strlcpy(socks->address, addr, sizeof(socks->address));
  socks->port = port;

  control_event_stream_status(conn, STREAM_EVENT_NEW, 0);

  rv = connection_ap_rewrite_and_attach_if_allowed(conn, NULL, NULL);

  // XXXX send a "100 Continue" message?

  goto done;

 err:
  if (BUG(errmsg == NULL))
    errmsg = "HTTP/1.0 400 Bad Request\r\n\r\n";
  log_info(LD_EDGE, "HTTP tunnel error: saying %s", escaped(errmsg));
  connection_buf_add(errmsg, strlen(errmsg), ENTRY_TO_CONN(conn));
  /* Mark it as "has_finished" so that we don't try to send an extra socks
   * reply. */
  conn->socks_request->has_finished = 1;
  connection_mark_unattached_ap(conn,
                                END_STREAM_REASON_HTTPPROTOCOL|
                                END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);

 done:
  tor_free(headers);
  tor_free(body);
  tor_free(command);
  tor_free(addrport);
  tor_free(addr);
  return rv;
}

/** Iterate over the two bytes of stream_id until we get one that is not
 * already in use; return it. Return 0 if can't get a unique stream_id.
 */
streamid_t
get_unique_stream_id_by_circ(origin_circuit_t *circ)
{
  edge_connection_t *tmpconn;
  streamid_t test_stream_id;
  uint32_t attempts=0;

 again:
  test_stream_id = circ->next_stream_id++;
  if (++attempts > 1<<16) {
    /* Make sure we don't loop forever if all stream_id's are used. */
    log_warn(LD_APP,"No unused stream IDs. Failing.");
    return 0;
  }
  if (test_stream_id == 0)
    goto again;
  for (tmpconn = circ->p_streams; tmpconn; tmpconn=tmpconn->next_stream)
    if (tmpconn->stream_id == test_stream_id)
      goto again;

  if (connection_half_edge_find_stream_id(circ->half_streams,
                                           test_stream_id))
    goto again;

  return test_stream_id;
}

/** Return true iff <b>conn</b> is linked to a circuit and configured to use
 * an exit that supports optimistic data. */
static int
connection_ap_supports_optimistic_data(const entry_connection_t *conn)
{
  const edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(conn);
  /* We can only send optimistic data if we're connected to an open
     general circuit. */
  if (edge_conn->on_circuit == NULL ||
      edge_conn->on_circuit->state != CIRCUIT_STATE_OPEN ||
      (edge_conn->on_circuit->purpose != CIRCUIT_PURPOSE_C_GENERAL &&
       edge_conn->on_circuit->purpose != CIRCUIT_PURPOSE_C_HSDIR_GET &&
       edge_conn->on_circuit->purpose != CIRCUIT_PURPOSE_S_HSDIR_POST &&
       edge_conn->on_circuit->purpose != CIRCUIT_PURPOSE_C_REND_JOINED))
    return 0;

  return conn->may_use_optimistic_data;
}

/** Return a bitmask of BEGIN_FLAG_* flags that we should transmit in the
 * RELAY_BEGIN cell for <b>ap_conn</b>. */
static uint32_t
connection_ap_get_begincell_flags(entry_connection_t *ap_conn)
{
  edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(ap_conn);
  const node_t *exitnode = NULL;
  const crypt_path_t *cpath_layer = edge_conn->cpath_layer;
  uint32_t flags = 0;

  /* No flags for begindir */
  if (ap_conn->use_begindir)
    return 0;

  /* No flags for hidden services. */
  if (edge_conn->on_circuit->purpose != CIRCUIT_PURPOSE_C_GENERAL)
    return 0;

  /* If only IPv4 is supported, no flags */
  if (ap_conn->entry_cfg.ipv4_traffic && !ap_conn->entry_cfg.ipv6_traffic)
    return 0;

  if (! cpath_layer ||
      ! cpath_layer->extend_info)
    return 0;

  if (!ap_conn->entry_cfg.ipv4_traffic)
    flags |= BEGIN_FLAG_IPV4_NOT_OK;

  exitnode = node_get_by_id(cpath_layer->extend_info->identity_digest);

  if (ap_conn->entry_cfg.ipv6_traffic && exitnode) {
    tor_addr_t a;
    tor_addr_make_null(&a, AF_INET6);
    if (compare_tor_addr_to_node_policy(&a, ap_conn->socks_request->port,
                                        exitnode)
        != ADDR_POLICY_REJECTED) {
      /* Only say "IPv6 OK" if the exit node supports IPv6. Otherwise there's
       * no point. */
      flags |= BEGIN_FLAG_IPV6_OK;
    }
  }

  if (flags == BEGIN_FLAG_IPV6_OK) {
    /* When IPv4 and IPv6 are both allowed, consider whether to say we
     * prefer IPv6.  Otherwise there's no point in declaring a preference */
    if (ap_conn->entry_cfg.prefer_ipv6)
      flags |= BEGIN_FLAG_IPV6_PREFERRED;
  }

  if (flags == BEGIN_FLAG_IPV4_NOT_OK) {
    log_warn(LD_EDGE, "I'm about to ask a node for a connection that I "
             "am telling it to fulfil with neither IPv4 nor IPv6. That's "
             "not going to work. Did you perhaps ask for an IPv6 address "
             "on an IPv4Only port, or vice versa?");
  }

  return flags;
}

/** Write a relay begin cell, using destaddr and destport from ap_conn's
 * socks_request field, and send it down circ.
 *
 * If ap_conn is broken, mark it for close and return -1. Else return 0.
 */
MOCK_IMPL(int,
connection_ap_handshake_send_begin,(entry_connection_t *ap_conn))
{
  char payload[CELL_PAYLOAD_SIZE];
  int payload_len;
  int begin_type;
  const or_options_t *options = get_options();
  origin_circuit_t *circ;
  edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(ap_conn);
  connection_t *base_conn = TO_CONN(edge_conn);
  tor_assert(edge_conn->on_circuit);
  circ = TO_ORIGIN_CIRCUIT(edge_conn->on_circuit);

  tor_assert(base_conn->type == CONN_TYPE_AP);
  tor_assert(base_conn->state == AP_CONN_STATE_CIRCUIT_WAIT);
  tor_assert(ap_conn->socks_request);
  tor_assert(SOCKS_COMMAND_IS_CONNECT(ap_conn->socks_request->command));

  edge_conn->stream_id = get_unique_stream_id_by_circ(circ);
  if (edge_conn->stream_id==0) {
    /* XXXX+ Instead of closing this stream, we should make it get
     * retried on another circuit. */
    connection_mark_unattached_ap(ap_conn, END_STREAM_REASON_INTERNAL);

    /* Mark this circuit "unusable for new streams". */
    mark_circuit_unusable_for_new_conns(circ);
    return -1;
  }

  /* Set up begin cell flags. */
  edge_conn->begincell_flags = connection_ap_get_begincell_flags(ap_conn);

  tor_snprintf(payload,RELAY_PAYLOAD_SIZE, "%s:%d",
               (circ->base_.purpose == CIRCUIT_PURPOSE_C_GENERAL) ?
                 ap_conn->socks_request->address : "",
               ap_conn->socks_request->port);
  payload_len = (int)strlen(payload)+1;
  if (payload_len <= RELAY_PAYLOAD_SIZE - 4 && edge_conn->begincell_flags) {
    set_uint32(payload + payload_len, htonl(edge_conn->begincell_flags));
    payload_len += 4;
  }

  log_info(LD_APP,
           "Sending relay cell %d on circ %u to begin stream %d.",
           (int)ap_conn->use_begindir,
           (unsigned)circ->base_.n_circ_id,
           edge_conn->stream_id);

  begin_type = ap_conn->use_begindir ?
                 RELAY_COMMAND_BEGIN_DIR : RELAY_COMMAND_BEGIN;

  /* Check that circuits are anonymised, based on their type. */
  if (begin_type == RELAY_COMMAND_BEGIN) {
    /* This connection is a standard OR connection.
     * Make sure its path length is anonymous, or that we're in a
     * non-anonymous mode. */
    assert_circ_anonymity_ok(circ, options);
  } else if (begin_type == RELAY_COMMAND_BEGIN_DIR) {
    /* This connection is a begindir directory connection.
     * Look at the linked directory connection to access the directory purpose.
     * If a BEGINDIR connection is ever not linked, that's a bug. */
    if (BUG(!base_conn->linked)) {
      return -1;
    }
    connection_t *linked_dir_conn_base = base_conn->linked_conn;
    /* If the linked connection has been unlinked by other code, we can't send
     * a begin cell on it. */
    if (!linked_dir_conn_base) {
      return -1;
    }
    /* Sensitive directory connections must have an anonymous path length.
     * Otherwise, directory connections are typically one-hop.
     * This matches the earlier check for directory connection path anonymity
     * in directory_initiate_request(). */
    if (purpose_needs_anonymity(linked_dir_conn_base->purpose,
                    TO_DIR_CONN(linked_dir_conn_base)->router_purpose,
                    TO_DIR_CONN(linked_dir_conn_base)->requested_resource)) {
      assert_circ_anonymity_ok(circ, options);
    }
  } else {
    /* This code was written for the two connection types BEGIN and BEGIN_DIR
     */
    tor_assert_unreached();
  }

  if (connection_edge_send_command(edge_conn, begin_type,
                  begin_type == RELAY_COMMAND_BEGIN ? payload : NULL,
                  begin_type == RELAY_COMMAND_BEGIN ? payload_len : 0) < 0)
    return -1; /* circuit is closed, don't continue */

  edge_conn->package_window = STREAMWINDOW_START;
  edge_conn->deliver_window = STREAMWINDOW_START;
  base_conn->state = AP_CONN_STATE_CONNECT_WAIT;
  log_info(LD_APP,"Address/port sent, ap socket "TOR_SOCKET_T_FORMAT
           ", n_circ_id %u",
           base_conn->s, (unsigned)circ->base_.n_circ_id);
  control_event_stream_status(ap_conn, STREAM_EVENT_SENT_CONNECT, 0);

  /* If there's queued-up data, send it now */
  if ((connection_get_inbuf_len(base_conn) ||
       ap_conn->sending_optimistic_data) &&
      connection_ap_supports_optimistic_data(ap_conn)) {
    log_info(LD_APP, "Sending up to %ld + %ld bytes of queued-up data",
             (long)connection_get_inbuf_len(base_conn),
             ap_conn->sending_optimistic_data ?
             (long)buf_datalen(ap_conn->sending_optimistic_data) : 0);
    if (connection_edge_package_raw_inbuf(edge_conn, 1, NULL) < 0) {
      connection_mark_for_close(base_conn);
    }
  }

  return 0;
}

/** Write a relay resolve cell, using destaddr and destport from ap_conn's
 * socks_request field, and send it down circ.
 *
 * If ap_conn is broken, mark it for close and return -1. Else return 0.
 */
int
connection_ap_handshake_send_resolve(entry_connection_t *ap_conn)
{
  int payload_len, command;
  const char *string_addr;
  char inaddr_buf[REVERSE_LOOKUP_NAME_BUF_LEN];
  origin_circuit_t *circ;
  edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(ap_conn);
  connection_t *base_conn = TO_CONN(edge_conn);
  tor_assert(edge_conn->on_circuit);
  circ = TO_ORIGIN_CIRCUIT(edge_conn->on_circuit);

  tor_assert(base_conn->type == CONN_TYPE_AP);
  tor_assert(base_conn->state == AP_CONN_STATE_CIRCUIT_WAIT);
  tor_assert(ap_conn->socks_request);
  tor_assert(circ->base_.purpose == CIRCUIT_PURPOSE_C_GENERAL);

  command = ap_conn->socks_request->command;
  tor_assert(SOCKS_COMMAND_IS_RESOLVE(command));

  edge_conn->stream_id = get_unique_stream_id_by_circ(circ);
  if (edge_conn->stream_id==0) {
    /* XXXX+ Instead of closing this stream, we should make it get
     * retried on another circuit. */
    connection_mark_unattached_ap(ap_conn, END_STREAM_REASON_INTERNAL);

    /* Mark this circuit "unusable for new streams". */
    mark_circuit_unusable_for_new_conns(circ);
    return -1;
  }

  if (command == SOCKS_COMMAND_RESOLVE) {
    string_addr = ap_conn->socks_request->address;
    payload_len = (int)strlen(string_addr)+1;
  } else {
    /* command == SOCKS_COMMAND_RESOLVE_PTR */
    const char *a = ap_conn->socks_request->address;
    tor_addr_t addr;
    int r;

    /* We're doing a reverse lookup.  The input could be an IP address, or
     * could be an .in-addr.arpa or .ip6.arpa address */
    r = tor_addr_parse_PTR_name(&addr, a, AF_UNSPEC, 1);
    if (r <= 0) {
      log_warn(LD_APP, "Rejecting ill-formed reverse lookup of %s",
               safe_str_client(a));
      connection_mark_unattached_ap(ap_conn, END_STREAM_REASON_INTERNAL);
      return -1;
    }

    r = tor_addr_to_PTR_name(inaddr_buf, sizeof(inaddr_buf), &addr);
    if (r < 0) {
      log_warn(LD_BUG, "Couldn't generate reverse lookup hostname of %s",
               safe_str_client(a));
      connection_mark_unattached_ap(ap_conn, END_STREAM_REASON_INTERNAL);
      return -1;
    }

    string_addr = inaddr_buf;
    payload_len = (int)strlen(inaddr_buf)+1;
    tor_assert(payload_len <= (int)sizeof(inaddr_buf));
  }

  log_debug(LD_APP,
            "Sending relay cell to begin stream %d.", edge_conn->stream_id);

  if (connection_edge_send_command(edge_conn,
                           RELAY_COMMAND_RESOLVE,
                           string_addr, payload_len) < 0)
    return -1; /* circuit is closed, don't continue */

  if (!base_conn->address) {
    /* This might be unnecessary. XXXX */
    base_conn->address = tor_addr_to_str_dup(&base_conn->addr);
  }
  base_conn->state = AP_CONN_STATE_RESOLVE_WAIT;
  log_info(LD_APP,"Address sent for resolve, ap socket "TOR_SOCKET_T_FORMAT
           ", n_circ_id %u",
           base_conn->s, (unsigned)circ->base_.n_circ_id);
  control_event_stream_status(ap_conn, STREAM_EVENT_SENT_RESOLVE, 0);
  return 0;
}

/** Make an AP connection_t linked to the connection_t <b>partner</b>. make a
 * new linked connection pair, and attach one side to the conn, connection_add
 * it, initialize it to circuit_wait, and call
 * connection_ap_handshake_attach_circuit(conn) on it.
 *
 * Return the newly created end of the linked connection pair, or -1 if error.
 */
entry_connection_t *
connection_ap_make_link(connection_t *partner,
                        char *address, uint16_t port,
                        const char *digest,
                        int session_group, int isolation_flags,
                        int use_begindir, int want_onehop)
{
  entry_connection_t *conn;
  connection_t *base_conn;

  log_info(LD_APP,"Making internal %s tunnel to %s:%d ...",
           want_onehop ? "direct" : "anonymized",
           safe_str_client(address), port);

  conn = entry_connection_new(CONN_TYPE_AP, tor_addr_family(&partner->addr));
  base_conn = ENTRY_TO_CONN(conn);
  base_conn->linked = 1; /* so that we can add it safely below. */

  /* populate conn->socks_request */

  /* leave version at zero, so the socks_reply is empty */
  conn->socks_request->socks_version = 0;
  conn->socks_request->has_finished = 0; /* waiting for 'connected' */
  strlcpy(conn->socks_request->address, address,
          sizeof(conn->socks_request->address));
  conn->socks_request->port = port;
  conn->socks_request->command = SOCKS_COMMAND_CONNECT;
  conn->want_onehop = want_onehop;
  conn->use_begindir = use_begindir;
  if (use_begindir) {
    conn->chosen_exit_name = tor_malloc(HEX_DIGEST_LEN+2);
    conn->chosen_exit_name[0] = '$';
    tor_assert(digest);
    base16_encode(conn->chosen_exit_name+1,HEX_DIGEST_LEN+1,
                  digest, DIGEST_LEN);
  }

  /* Populate isolation fields. */
  conn->socks_request->listener_type = CONN_TYPE_DIR_LISTENER;
  conn->original_dest_address = tor_strdup(address);
  conn->entry_cfg.session_group = session_group;
  conn->entry_cfg.isolation_flags = isolation_flags;

  base_conn->address = tor_strdup("(Tor_internal)");
  tor_addr_make_unspec(&base_conn->addr);
  base_conn->port = 0;

  connection_link_connections(partner, base_conn);

  if (connection_add(base_conn) < 0) { /* no space, forget it */
    connection_free(base_conn);
    return NULL;
  }

  base_conn->state = AP_CONN_STATE_CIRCUIT_WAIT;

  control_event_stream_status(conn, STREAM_EVENT_NEW, 0);

  /* attaching to a dirty circuit is fine */
  connection_ap_mark_as_pending_circuit(conn);
  log_info(LD_APP,"... application connection created and linked.");
  return conn;
}

/** Notify any interested controller connections about a new hostname resolve
 * or resolve error.  Takes the same arguments as does
 * connection_ap_handshake_socks_resolved(). */
static void
tell_controller_about_resolved_result(entry_connection_t *conn,
                                      int answer_type,
                                      size_t answer_len,
                                      const char *answer,
                                      int ttl,
                                      time_t expires)
{
  expires = time(NULL) + ttl;
  if (answer_type == RESOLVED_TYPE_IPV4 && answer_len >= 4) {
    char *cp = tor_dup_ip(ntohl(get_uint32(answer)));
    control_event_address_mapped(conn->socks_request->address,
                                 cp, expires, NULL, 0);
    tor_free(cp);
  } else if (answer_type == RESOLVED_TYPE_HOSTNAME && answer_len < 256) {
    char *cp = tor_strndup(answer, answer_len);
    control_event_address_mapped(conn->socks_request->address,
                                 cp, expires, NULL, 0);
    tor_free(cp);
  } else {
    control_event_address_mapped(conn->socks_request->address,
                                 "<error>", time(NULL)+ttl,
                                 "error=yes", 0);
  }
}

/**
 * As connection_ap_handshake_socks_resolved, but take a tor_addr_t to send
 * as the answer.
 */
void
connection_ap_handshake_socks_resolved_addr(entry_connection_t *conn,
                                            const tor_addr_t *answer,
                                            int ttl,
                                            time_t expires)
{
  if (tor_addr_family(answer) == AF_INET) {
    uint32_t a = tor_addr_to_ipv4n(answer); /* network order */
    connection_ap_handshake_socks_resolved(conn,RESOLVED_TYPE_IPV4,4,
                                           (uint8_t*)&a,
                                           ttl, expires);
  } else if (tor_addr_family(answer) == AF_INET6) {
    const uint8_t *a = tor_addr_to_in6_addr8(answer);
    connection_ap_handshake_socks_resolved(conn,RESOLVED_TYPE_IPV6,16,
                                           a,
                                           ttl, expires);
  } else {
    log_warn(LD_BUG, "Got called with address of unexpected family %d",
             tor_addr_family(answer));
    connection_ap_handshake_socks_resolved(conn,
                                           RESOLVED_TYPE_ERROR,0,NULL,-1,-1);
  }
}

/** Send an answer to an AP connection that has requested a DNS lookup via
 * SOCKS.  The type should be one of RESOLVED_TYPE_(IPV4|IPV6|HOSTNAME) or -1
 * for unreachable; the answer should be in the format specified in the socks
 * extensions document.  <b>ttl</b> is the ttl for the answer, or -1 on
 * certain errors or for values that didn't come via DNS.  <b>expires</b> is
 * a time when the answer expires, or -1 or TIME_MAX if there's a good TTL.
 **/
/* XXXX the use of the ttl and expires fields is nutty.  Let's make this
 * interface and those that use it less ugly. */
MOCK_IMPL(void,
connection_ap_handshake_socks_resolved,(entry_connection_t *conn,
                                       int answer_type,
                                       size_t answer_len,
                                       const uint8_t *answer,
                                       int ttl,
                                       time_t expires))
{
  char buf[384];
  size_t replylen;

  if (ttl >= 0) {
    if (answer_type == RESOLVED_TYPE_IPV4 && answer_len == 4) {
      tor_addr_t a;
      tor_addr_from_ipv4n(&a, get_uint32(answer));
      if (! tor_addr_is_null(&a)) {
        client_dns_set_addressmap(conn,
                                  conn->socks_request->address, &a,
                                  conn->chosen_exit_name, ttl);
      }
    } else if (answer_type == RESOLVED_TYPE_IPV6 && answer_len == 16) {
      tor_addr_t a;
      tor_addr_from_ipv6_bytes(&a, (char*)answer);
      if (! tor_addr_is_null(&a)) {
        client_dns_set_addressmap(conn,
                                  conn->socks_request->address, &a,
                                  conn->chosen_exit_name, ttl);
      }
    } else if (answer_type == RESOLVED_TYPE_HOSTNAME && answer_len < 256) {
      char *cp = tor_strndup((char*)answer, answer_len);
      client_dns_set_reverse_addressmap(conn,
                                        conn->socks_request->address,
                                        cp,
                                        conn->chosen_exit_name, ttl);
      tor_free(cp);
    }
  }

  if (ENTRY_TO_EDGE_CONN(conn)->is_dns_request) {
    if (conn->dns_server_request) {
      /* We had a request on our DNS port: answer it. */
      dnsserv_resolved(conn, answer_type, answer_len, (char*)answer, ttl);
      conn->socks_request->has_finished = 1;
      return;
    } else {
      /* This must be a request from the controller. Since answers to those
       * requests are not cached, they do not generate an ADDRMAP event on
       * their own. */
      tell_controller_about_resolved_result(conn, answer_type, answer_len,
                                            (char*)answer, ttl, expires);
      conn->socks_request->has_finished = 1;
      return;
    }
    /* We shouldn't need to free conn here; it gets marked by the caller. */
  }

  if (conn->socks_request->socks_version == 4) {
    buf[0] = 0x00; /* version */
    if (answer_type == RESOLVED_TYPE_IPV4 && answer_len == 4) {
      buf[1] = SOCKS4_GRANTED;
      set_uint16(buf+2, 0);
      memcpy(buf+4, answer, 4); /* address */
      replylen = SOCKS4_NETWORK_LEN;
    } else { /* "error" */
      buf[1] = SOCKS4_REJECT;
      memset(buf+2, 0, 6);
      replylen = SOCKS4_NETWORK_LEN;
    }
  } else if (conn->socks_request->socks_version == 5) {
    /* SOCKS5 */
    buf[0] = 0x05; /* version */
    if (answer_type == RESOLVED_TYPE_IPV4 && answer_len == 4) {
      buf[1] = SOCKS5_SUCCEEDED;
      buf[2] = 0; /* reserved */
      buf[3] = 0x01; /* IPv4 address type */
      memcpy(buf+4, answer, 4); /* address */
      set_uint16(buf+8, 0); /* port == 0. */
      replylen = 10;
    } else if (answer_type == RESOLVED_TYPE_IPV6 && answer_len == 16) {
      buf[1] = SOCKS5_SUCCEEDED;
      buf[2] = 0; /* reserved */
      buf[3] = 0x04; /* IPv6 address type */
      memcpy(buf+4, answer, 16); /* address */
      set_uint16(buf+20, 0); /* port == 0. */
      replylen = 22;
    } else if (answer_type == RESOLVED_TYPE_HOSTNAME && answer_len < 256) {
      buf[1] = SOCKS5_SUCCEEDED;
      buf[2] = 0; /* reserved */
      buf[3] = 0x03; /* Domainname address type */
      buf[4] = (char)answer_len;
      memcpy(buf+5, answer, answer_len); /* address */
      set_uint16(buf+5+answer_len, 0); /* port == 0. */
      replylen = 5+answer_len+2;
    } else {
      buf[1] = SOCKS5_HOST_UNREACHABLE;
      memset(buf+2, 0, 8);
      replylen = 10;
    }
  } else {
    /* no socks version info; don't send anything back */
    return;
  }
  connection_ap_handshake_socks_reply(conn, buf, replylen,
          (answer_type == RESOLVED_TYPE_IPV4 ||
           answer_type == RESOLVED_TYPE_IPV6 ||
           answer_type == RESOLVED_TYPE_HOSTNAME) ?
                                      0 : END_STREAM_REASON_RESOLVEFAILED);
}

/** Send a socks reply to stream <b>conn</b>, using the appropriate
 * socks version, etc, and mark <b>conn</b> as completed with SOCKS
 * handshaking.
 *
 * If <b>reply</b> is defined, then write <b>replylen</b> bytes of it to conn
 * and return, else reply based on <b>endreason</b> (one of
 * END_STREAM_REASON_*). If <b>reply</b> is undefined, <b>endreason</b> can't
 * be 0 or REASON_DONE.  Send endreason to the controller, if appropriate.
 */
void
connection_ap_handshake_socks_reply(entry_connection_t *conn, char *reply,
                                    size_t replylen, int endreason)
{
  char buf[256];
  socks5_reply_status_t status =
    stream_end_reason_to_socks5_response(endreason);

  tor_assert(conn->socks_request); /* make sure it's an AP stream */

  if (!SOCKS_COMMAND_IS_RESOLVE(conn->socks_request->command)) {
    control_event_stream_status(conn, status==SOCKS5_SUCCEEDED ?
                                STREAM_EVENT_SUCCEEDED : STREAM_EVENT_FAILED,
                                endreason);
  }

  /* Flag this stream's circuit as having completed a stream successfully
   * (for path bias) */
  if (status == SOCKS5_SUCCEEDED ||
      endreason == END_STREAM_REASON_RESOLVEFAILED ||
      endreason == END_STREAM_REASON_CONNECTREFUSED ||
      endreason == END_STREAM_REASON_CONNRESET ||
      endreason == END_STREAM_REASON_NOROUTE ||
      endreason == END_STREAM_REASON_RESOURCELIMIT) {
    if (!conn->edge_.on_circuit ||
       !CIRCUIT_IS_ORIGIN(conn->edge_.on_circuit)) {
      if (endreason != END_STREAM_REASON_RESOLVEFAILED) {
        log_info(LD_BUG,
                 "No origin circuit for successful SOCKS stream %"PRIu64
                 ". Reason: %d",
                 (ENTRY_TO_CONN(conn)->global_identifier),
                 endreason);
      }
      /*
       * Else DNS remaps and failed hidden service lookups can send us
       * here with END_STREAM_REASON_RESOLVEFAILED; ignore it
       *
       * Perhaps we could make the test more precise; we can tell hidden
       * services by conn->edge_.renddata != NULL; anything analogous for
       * the DNS remap case?
       */
    } else {
      // XXX: Hrmm. It looks like optimistic data can't go through this
      // codepath, but someone should probably test it and make sure.
      // We don't want to mark optimistically opened streams as successful.
      pathbias_mark_use_success(TO_ORIGIN_CIRCUIT(conn->edge_.on_circuit));
    }
  }

  if (conn->socks_request->has_finished) {
    log_warn(LD_BUG, "(Harmless.) duplicate calls to "
             "connection_ap_handshake_socks_reply.");
    return;
  }
  if (replylen) { /* we already have a reply in mind */
    connection_buf_add(reply, replylen, ENTRY_TO_CONN(conn));
    conn->socks_request->has_finished = 1;
    return;
  }
  if (conn->socks_request->listener_type ==
       CONN_TYPE_AP_HTTP_CONNECT_LISTENER) {
    const char *response = end_reason_to_http_connect_response_line(endreason);
    if (!response) {
      response = "HTTP/1.0 400 Bad Request\r\n\r\n";
    }
    connection_buf_add(response, strlen(response), ENTRY_TO_CONN(conn));
  } else if (conn->socks_request->socks_version == 4) {
    memset(buf,0,SOCKS4_NETWORK_LEN);
    buf[1] = (status==SOCKS5_SUCCEEDED ? SOCKS4_GRANTED : SOCKS4_REJECT);
    /* leave version, destport, destip zero */
    connection_buf_add(buf, SOCKS4_NETWORK_LEN, ENTRY_TO_CONN(conn));
  } else if (conn->socks_request->socks_version == 5) {
    size_t buf_len;
    memset(buf,0,sizeof(buf));
    if (tor_addr_family(&conn->edge_.base_.addr) == AF_INET) {
      buf[0] = 5; /* version 5 */
      buf[1] = (char)status;
      buf[2] = 0;
      buf[3] = 1; /* ipv4 addr */
      /* 4 bytes for the header, 2 bytes for the port, 4 for the address. */
      buf_len = 10;
    } else { /* AF_INET6. */
      buf[0] = 5; /* version 5 */
      buf[1] = (char)status;
      buf[2] = 0;
      buf[3] = 4; /* ipv6 addr */
      /* 4 bytes for the header, 2 bytes for the port, 16 for the address. */
      buf_len = 22;
    }
    connection_buf_add(buf,buf_len,ENTRY_TO_CONN(conn));
  }
  /* If socks_version isn't 4 or 5, don't send anything.
   * This can happen in the case of AP bridges. */
  conn->socks_request->has_finished = 1;
  return;
}

/** Read a RELAY_BEGIN or RELAY_BEGIN_DIR cell from <b>cell</b>, decode it, and
 * place the result in <b>bcell</b>.  On success return 0; on failure return
 * <0 and set *<b>end_reason_out</b> to the end reason we should send back to
 * the client.
 *
 * Return -1 in the case where we want to send a RELAY_END cell, and < -1 when
 * we don't.
 **/
STATIC int
begin_cell_parse(const cell_t *cell, begin_cell_t *bcell,
                 uint8_t *end_reason_out)
{
  relay_header_t rh;
  const uint8_t *body, *nul;

  memset(bcell, 0, sizeof(*bcell));
  *end_reason_out = END_STREAM_REASON_MISC;

  relay_header_unpack(&rh, cell->payload);
  if (rh.length > RELAY_PAYLOAD_SIZE) {
    return -2; /*XXXX why not TORPROTOCOL? */
  }

  bcell->stream_id = rh.stream_id;

  if (rh.command == RELAY_COMMAND_BEGIN_DIR) {
    bcell->is_begindir = 1;
    return 0;
  } else if (rh.command != RELAY_COMMAND_BEGIN) {
    log_warn(LD_BUG, "Got an unexpected command %d", (int)rh.command);
    *end_reason_out = END_STREAM_REASON_INTERNAL;
    return -1;
  }

  body = cell->payload + RELAY_HEADER_SIZE;
  nul = memchr(body, 0, rh.length);
  if (! nul) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Relay begin cell has no \\0. Closing.");
    *end_reason_out = END_STREAM_REASON_TORPROTOCOL;
    return -1;
  }

  if (tor_addr_port_split(LOG_PROTOCOL_WARN,
                          (char*)(body),
                          &bcell->address,&bcell->port)<0) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Unable to parse addr:port in relay begin cell. Closing.");
    *end_reason_out = END_STREAM_REASON_TORPROTOCOL;
    return -1;
  }
  if (bcell->port == 0) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Missing port in relay begin cell. Closing.");
    tor_free(bcell->address);
    *end_reason_out = END_STREAM_REASON_TORPROTOCOL;
    return -1;
  }
  if (body + rh.length >= nul + 4)
    bcell->flags = ntohl(get_uint32(nul+1));

  return 0;
}

/** For the given <b>circ</b> and the edge connection <b>conn</b>, setup the
 * connection, attach it to the circ and connect it. Return 0 on success
 * or END_CIRC_AT_ORIGIN if we can't find the requested hidden service port
 * where the caller should close the circuit. */
static int
handle_hs_exit_conn(circuit_t *circ, edge_connection_t *conn)
{
  int ret;
  origin_circuit_t *origin_circ;

  assert_circuit_ok(circ);
  tor_assert(circ->purpose == CIRCUIT_PURPOSE_S_REND_JOINED);
  tor_assert(conn);

  log_debug(LD_REND, "Connecting the hidden service rendezvous circuit "
                     "to the service destination.");

  origin_circ = TO_ORIGIN_CIRCUIT(circ);
  conn->base_.address = tor_strdup("(rendezvous)");
  conn->base_.state = EXIT_CONN_STATE_CONNECTING;

  /* The circuit either has an hs identifier for v3+ or a rend_data for legacy
   * service. */
  if (origin_circ->rend_data) {
    conn->rend_data = rend_data_dup(origin_circ->rend_data);
    tor_assert(connection_edge_is_rendezvous_stream(conn));
    ret = rend_service_set_connection_addr_port(conn, origin_circ);
  } else if (origin_circ->hs_ident) {
    /* Setup the identifier to be the one for the circuit service. */
    conn->hs_ident =
      hs_ident_edge_conn_new(&origin_circ->hs_ident->identity_pk);
    tor_assert(connection_edge_is_rendezvous_stream(conn));
    ret = hs_service_set_conn_addr_port(origin_circ, conn);
  } else {
    /* We should never get here if the circuit's purpose is rendezvous. */
    tor_assert_nonfatal_unreached();
    return -1;
  }
  if (ret < 0) {
    log_info(LD_REND, "Didn't find rendezvous service (addr%s, port %d)",
             fmt_addr(&TO_CONN(conn)->addr), TO_CONN(conn)->port);
    /* Send back reason DONE because we want to make hidden service port
     * scanning harder thus instead of returning that the exit policy
     * didn't match, which makes it obvious that the port is closed,
     * return DONE and kill the circuit. That way, a user (malicious or
     * not) needs one circuit per bad port unless it matches the policy of
     * the hidden service. */
    relay_send_end_cell_from_edge(conn->stream_id, circ,
                                  END_STREAM_REASON_DONE,
                                  origin_circ->cpath->prev);
    connection_free_(TO_CONN(conn));

    /* Drop the circuit here since it might be someone deliberately
     * scanning the hidden service ports. Note that this mitigates port
     * scanning by adding more work on the attacker side to successfully
     * scan but does not fully solve it. */
    if (ret < -1) {
      return END_CIRC_AT_ORIGIN;
    } else {
      return 0;
    }
  }

  /* Link the circuit and the connection crypt path. */
  conn->cpath_layer = origin_circ->cpath->prev;

  /* Add it into the linked list of p_streams on this circuit */
  conn->next_stream = origin_circ->p_streams;
  origin_circ->p_streams = conn;
  conn->on_circuit = circ;
  assert_circuit_ok(circ);

  hs_inc_rdv_stream_counter(origin_circ);

  /* If it's an onion service connection, we might want to include the proxy
   * protocol header: */
  if (conn->hs_ident) {
    hs_circuit_id_protocol_t circuit_id_protocol =
      hs_service_exports_circuit_id(&conn->hs_ident->identity_pk);
    export_hs_client_circuit_id(conn, circuit_id_protocol);
  }

  /* Connect tor to the hidden service destination. */
  connection_exit_connect(conn);

  /* For path bias: This circuit was used successfully */
  pathbias_mark_use_success(origin_circ);
  return 0;
}

/** A relay 'begin' or 'begin_dir' cell has arrived, and either we are
 * an exit hop for the circuit, or we are the origin and it is a
 * rendezvous begin.
 *
 * Launch a new exit connection and initialize things appropriately.
 *
 * If it's a rendezvous stream, call connection_exit_connect() on
 * it.
 *
 * For general streams, call dns_resolve() on it first, and only call
 * connection_exit_connect() if the dns answer is already known.
 *
 * Note that we don't call connection_add() on the new stream! We wait
 * for connection_exit_connect() to do that.
 *
 * Return -(some circuit end reason) if we want to tear down <b>circ</b>.
 * Else return 0.
 */
int
connection_exit_begin_conn(cell_t *cell, circuit_t *circ)
{
  edge_connection_t *n_stream;
  relay_header_t rh;
  char *address = NULL;
  uint16_t port = 0;
  or_circuit_t *or_circ = NULL;
  origin_circuit_t *origin_circ = NULL;
  crypt_path_t *layer_hint = NULL;
  const or_options_t *options = get_options();
  begin_cell_t bcell;
  int rv;
  uint8_t end_reason=0;

  assert_circuit_ok(circ);
  if (!CIRCUIT_IS_ORIGIN(circ)) {
    or_circ = TO_OR_CIRCUIT(circ);
  } else {
    tor_assert(circ->purpose == CIRCUIT_PURPOSE_S_REND_JOINED);
    origin_circ = TO_ORIGIN_CIRCUIT(circ);
    layer_hint = origin_circ->cpath->prev;
  }

  relay_header_unpack(&rh, cell->payload);
  if (rh.length > RELAY_PAYLOAD_SIZE)
    return -END_CIRC_REASON_TORPROTOCOL;

  if (!server_mode(options) &&
      circ->purpose != CIRCUIT_PURPOSE_S_REND_JOINED) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Relay begin cell at non-server. Closing.");
    relay_send_end_cell_from_edge(rh.stream_id, circ,
                                  END_STREAM_REASON_EXITPOLICY, NULL);
    return 0;
  }

  rv = begin_cell_parse(cell, &bcell, &end_reason);
  if (rv < -1) {
    return -END_CIRC_REASON_TORPROTOCOL;
  } else if (rv == -1) {
    tor_free(bcell.address);
    relay_send_end_cell_from_edge(rh.stream_id, circ, end_reason, layer_hint);
    return 0;
  }

  if (! bcell.is_begindir) {
    /* Steal reference */
    address = bcell.address;
    port = bcell.port;

    if (or_circ && or_circ->p_chan) {
      const int client_chan = channel_is_client(or_circ->p_chan);
      if ((client_chan ||
           (!connection_or_digest_is_known_relay(
                or_circ->p_chan->identity_digest) &&
          should_refuse_unknown_exits(options)))) {
        /* Don't let clients use us as a single-hop proxy. It attracts
         * attackers and users who'd be better off with, well, single-hop
         * proxies. */
        log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
               "Attempt by %s to open a stream %s. Closing.",
               safe_str(channel_get_canonical_remote_descr(or_circ->p_chan)),
               client_chan ? "on first hop of circuit" :
                             "from unknown relay");
        relay_send_end_cell_from_edge(rh.stream_id, circ,
                                      client_chan ?
                                        END_STREAM_REASON_TORPROTOCOL :
                                        END_STREAM_REASON_MISC,
                                      NULL);
        tor_free(address);
        return 0;
      }
    }
  } else if (rh.command == RELAY_COMMAND_BEGIN_DIR) {
    if (!directory_permits_begindir_requests(options) ||
        circ->purpose != CIRCUIT_PURPOSE_OR) {
      relay_send_end_cell_from_edge(rh.stream_id, circ,
                                  END_STREAM_REASON_NOTDIRECTORY, layer_hint);
      return 0;
    }
    /* Make sure to get the 'real' address of the previous hop: the
     * caller might want to know whether the remote IP address has changed,
     * and we might already have corrected base_.addr[ess] for the relay's
     * canonical IP address. */
    if (or_circ && or_circ->p_chan)
      address = tor_strdup(channel_get_actual_remote_address(or_circ->p_chan));
    else
      address = tor_strdup("127.0.0.1");
    port = 1; /* XXXX This value is never actually used anywhere, and there
               * isn't "really" a connection here.  But we
               * need to set it to something nonzero. */
  } else {
    log_warn(LD_BUG, "Got an unexpected command %d", (int)rh.command);
    relay_send_end_cell_from_edge(rh.stream_id, circ,
                                  END_STREAM_REASON_INTERNAL, layer_hint);
    return 0;
  }

  if (! options->IPv6Exit) {
    /* I don't care if you prefer IPv6; I can't give you any. */
    bcell.flags &= ~BEGIN_FLAG_IPV6_PREFERRED;
    /* If you don't want IPv4, I can't help. */
    if (bcell.flags & BEGIN_FLAG_IPV4_NOT_OK) {
      tor_free(address);
      relay_send_end_cell_from_edge(rh.stream_id, circ,
                                    END_STREAM_REASON_EXITPOLICY, layer_hint);
      return 0;
    }
  }

  log_debug(LD_EXIT,"Creating new exit connection.");
  /* The 'AF_INET' here is temporary; we might need to change it later in
   * connection_exit_connect(). */
  n_stream = edge_connection_new(CONN_TYPE_EXIT, AF_INET);

  /* Remember the tunneled request ID in the new edge connection, so that
   * we can measure download times. */
  n_stream->dirreq_id = circ->dirreq_id;

  n_stream->base_.purpose = EXIT_PURPOSE_CONNECT;
  n_stream->begincell_flags = bcell.flags;
  n_stream->stream_id = rh.stream_id;
  n_stream->base_.port = port;
  /* leave n_stream->s at -1, because it's not yet valid */
  n_stream->package_window = STREAMWINDOW_START;
  n_stream->deliver_window = STREAMWINDOW_START;

  if (circ->purpose == CIRCUIT_PURPOSE_S_REND_JOINED) {
    int ret;
    tor_free(address);
    /* We handle this circuit and stream in this function for all supported
     * hidden service version. */
    ret = handle_hs_exit_conn(circ, n_stream);

    if (ret == 0) {
      /* This was a valid cell. Count it as delivered + overhead. */
      circuit_read_valid_data(origin_circ, rh.length);
    }
    return ret;
  }
  tor_strlower(address);
  n_stream->base_.address = address;
  n_stream->base_.state = EXIT_CONN_STATE_RESOLVEFAILED;
  /* default to failed, change in dns_resolve if it turns out not to fail */

  /* If we're hibernating or shutting down, we refuse to open new streams. */
  if (we_are_hibernating()) {
    relay_send_end_cell_from_edge(rh.stream_id, circ,
                                  END_STREAM_REASON_HIBERNATING, NULL);
    connection_free_(TO_CONN(n_stream));
    return 0;
  }

  n_stream->on_circuit = circ;

  if (rh.command == RELAY_COMMAND_BEGIN_DIR) {
    tor_addr_t tmp_addr;
    tor_assert(or_circ);
    if (or_circ->p_chan &&
        channel_get_addr_if_possible(or_circ->p_chan, &tmp_addr)) {
      tor_addr_copy(&n_stream->base_.addr, &tmp_addr);
    }
    return connection_exit_connect_dir(n_stream);
  }

  log_debug(LD_EXIT,"about to start the dns_resolve().");

  /* send it off to the gethostbyname farm */
  switch (dns_resolve(n_stream)) {
    case 1: /* resolve worked; now n_stream is attached to circ. */
      assert_circuit_ok(circ);
      log_debug(LD_EXIT,"about to call connection_exit_connect().");
      connection_exit_connect(n_stream);
      return 0;
    case -1: /* resolve failed */
      relay_send_end_cell_from_edge(rh.stream_id, circ,
                                    END_STREAM_REASON_RESOLVEFAILED, NULL);
      /* n_stream got freed. don't touch it. */
      break;
    case 0: /* resolve added to pending list */
      assert_circuit_ok(circ);
      break;
  }
  return 0;
}

/**
 * Called when we receive a RELAY_COMMAND_RESOLVE cell 'cell' along the
 * circuit <b>circ</b>;
 * begin resolving the hostname, and (eventually) reply with a RESOLVED cell.
 */
int
connection_exit_begin_resolve(cell_t *cell, or_circuit_t *circ)
{
  edge_connection_t *dummy_conn;
  relay_header_t rh;

  assert_circuit_ok(TO_CIRCUIT(circ));
  relay_header_unpack(&rh, cell->payload);
  if (rh.length > RELAY_PAYLOAD_SIZE)
    return -1;

  /* This 'dummy_conn' only exists to remember the stream ID
   * associated with the resolve request; and to make the
   * implementation of dns.c more uniform.  (We really only need to
   * remember the circuit, the stream ID, and the hostname to be
   * resolved; but if we didn't store them in a connection like this,
   * the housekeeping in dns.c would get way more complicated.)
   */
  dummy_conn = edge_connection_new(CONN_TYPE_EXIT, AF_INET);
  dummy_conn->stream_id = rh.stream_id;
  dummy_conn->base_.address = tor_strndup(
                                       (char*)cell->payload+RELAY_HEADER_SIZE,
                                       rh.length);
  dummy_conn->base_.port = 0;
  dummy_conn->base_.state = EXIT_CONN_STATE_RESOLVEFAILED;
  dummy_conn->base_.purpose = EXIT_PURPOSE_RESOLVE;

  dummy_conn->on_circuit = TO_CIRCUIT(circ);

  /* send it off to the gethostbyname farm */
  switch (dns_resolve(dummy_conn)) {
    case -1: /* Impossible to resolve; a resolved cell was sent. */
      /* Connection freed; don't touch it. */
      return 0;
    case 1: /* The result was cached; a resolved cell was sent. */
      if (!dummy_conn->base_.marked_for_close)
        connection_free_(TO_CONN(dummy_conn));
      return 0;
    case 0: /* resolve added to pending list */
      assert_circuit_ok(TO_CIRCUIT(circ));
      break;
  }
  return 0;
}

/** Helper: Return true and set *<b>why_rejected</b> to an optional clarifying
 * message message iff we do not allow connections to <b>addr</b>:<b>port</b>.
 */
static int
my_exit_policy_rejects(const tor_addr_t *addr,
                       uint16_t port,
                       const char **why_rejected)
{
  if (router_compare_to_my_exit_policy(addr, port)) {
    *why_rejected = "";
    return 1;
  } else if (tor_addr_family(addr) == AF_INET6 && !get_options()->IPv6Exit) {
    *why_rejected = " (IPv6 address without IPv6Exit configured)";
    return 1;
  }
  return 0;
}

/** Connect to conn's specified addr and port. If it worked, conn
 * has now been added to the connection_array.
 *
 * Send back a connected cell. Include the resolved IP of the destination
 * address, but <em>only</em> if it's a general exit stream. (Rendezvous
 * streams must not reveal what IP they connected to.)
 */
void
connection_exit_connect(edge_connection_t *edge_conn)
{
  const tor_addr_t *addr;
  uint16_t port;
  connection_t *conn = TO_CONN(edge_conn);
  int socket_error = 0, result;
  const char *why_failed_exit_policy = NULL;

  /* Apply exit policy to non-rendezvous connections. */
  if (! connection_edge_is_rendezvous_stream(edge_conn) &&
      my_exit_policy_rejects(&edge_conn->base_.addr,
                             edge_conn->base_.port,
                             &why_failed_exit_policy)) {
    if (BUG(!why_failed_exit_policy))
      why_failed_exit_policy = "";
    log_info(LD_EXIT,"%s:%d failed exit policy%s. Closing.",
             escaped_safe_str_client(conn->address), conn->port,
             why_failed_exit_policy);
    connection_edge_end(edge_conn, END_STREAM_REASON_EXITPOLICY);
    circuit_detach_stream(circuit_get_by_edge_conn(edge_conn), edge_conn);
    connection_free(conn);
    return;
  }

#ifdef HAVE_SYS_UN_H
  if (conn->socket_family != AF_UNIX) {
#else
  {
#endif /* defined(HAVE_SYS_UN_H) */
    addr = &conn->addr;
    port = conn->port;

    if (tor_addr_family(addr) == AF_INET6)
      conn->socket_family = AF_INET6;

    log_debug(LD_EXIT, "about to try connecting");
    result = connection_connect(conn, conn->address,
                                addr, port, &socket_error);
#ifdef HAVE_SYS_UN_H
  } else {
    /*
     * In the AF_UNIX case, we expect to have already had conn->port = 1,
     * tor_addr_make_unspec(conn->addr) (cf. the way we mark in the incoming
     * case in connection_handle_listener_read()), and conn->address should
     * have the socket path to connect to.
     */
    tor_assert(conn->address && strlen(conn->address) > 0);

    log_debug(LD_EXIT, "about to try connecting");
    result = connection_connect_unix(conn, conn->address, &socket_error);
#endif /* defined(HAVE_SYS_UN_H) */
  }

  switch (result) {
    case -1: {
      int reason = errno_to_stream_end_reason(socket_error);
      connection_edge_end(edge_conn, reason);
      circuit_detach_stream(circuit_get_by_edge_conn(edge_conn), edge_conn);
      connection_free(conn);
      return;
    }
    case 0:
      conn->state = EXIT_CONN_STATE_CONNECTING;

      connection_watch_events(conn, READ_EVENT | WRITE_EVENT);
      /* writable indicates finish;
       * readable/error indicates broken link in windows-land. */
      return;
    /* case 1: fall through */
  }

  conn->state = EXIT_CONN_STATE_OPEN;
  if (connection_get_outbuf_len(conn)) {
    /* in case there are any queued data cells, from e.g. optimistic data */
    connection_watch_events(conn, READ_EVENT|WRITE_EVENT);
  } else {
    connection_watch_events(conn, READ_EVENT);
  }

  /* also, deliver a 'connected' cell back through the circuit. */
  if (connection_edge_is_rendezvous_stream(edge_conn)) {
    /* don't send an address back! */
    connection_edge_send_command(edge_conn,
                                 RELAY_COMMAND_CONNECTED,
                                 NULL, 0);
  } else { /* normal stream */
    uint8_t connected_payload[MAX_CONNECTED_CELL_PAYLOAD_LEN];
    int connected_payload_len =
      connected_cell_format_payload(connected_payload, &conn->addr,
                                    edge_conn->address_ttl);
    if (connected_payload_len < 0) {
      connection_edge_end(edge_conn, END_STREAM_REASON_INTERNAL);
      circuit_detach_stream(circuit_get_by_edge_conn(edge_conn), edge_conn);
      connection_free(conn);
      return;
    }

    connection_edge_send_command(edge_conn,
                                 RELAY_COMMAND_CONNECTED,
                                 (char*)connected_payload,
                                 connected_payload_len);
  }
}

/** Given an exit conn that should attach to us as a directory server, open a
 * bridge connection with a linked connection pair, create a new directory
 * conn, and join them together.  Return 0 on success (or if there was an
 * error we could send back an end cell for).  Return -(some circuit end
 * reason) if the circuit needs to be torn down.  Either connects
 * <b>exitconn</b>, frees it, or marks it, as appropriate.
 */
static int
connection_exit_connect_dir(edge_connection_t *exitconn)
{
  dir_connection_t *dirconn = NULL;
  or_circuit_t *circ = TO_OR_CIRCUIT(exitconn->on_circuit);

  log_info(LD_EXIT, "Opening local connection for anonymized directory exit");

  exitconn->base_.state = EXIT_CONN_STATE_OPEN;

  dirconn = dir_connection_new(tor_addr_family(&exitconn->base_.addr));

  tor_addr_copy(&dirconn->base_.addr, &exitconn->base_.addr);
  dirconn->base_.port = 0;
  dirconn->base_.address = tor_strdup(exitconn->base_.address);
  dirconn->base_.type = CONN_TYPE_DIR;
  dirconn->base_.purpose = DIR_PURPOSE_SERVER;
  dirconn->base_.state = DIR_CONN_STATE_SERVER_COMMAND_WAIT;

  /* Note that the new dir conn belongs to the same tunneled request as
   * the edge conn, so that we can measure download times. */
  dirconn->dirreq_id = exitconn->dirreq_id;

  connection_link_connections(TO_CONN(dirconn), TO_CONN(exitconn));

  if (connection_add(TO_CONN(exitconn))<0) {
    connection_edge_end(exitconn, END_STREAM_REASON_RESOURCELIMIT);
    connection_free_(TO_CONN(exitconn));
    connection_free_(TO_CONN(dirconn));
    return 0;
  }

  /* link exitconn to circ, now that we know we can use it. */
  exitconn->next_stream = circ->n_streams;
  circ->n_streams = exitconn;

  if (connection_add(TO_CONN(dirconn))<0) {
    connection_edge_end(exitconn, END_STREAM_REASON_RESOURCELIMIT);
    connection_close_immediate(TO_CONN(exitconn));
    connection_mark_for_close(TO_CONN(exitconn));
    connection_free_(TO_CONN(dirconn));
    return 0;
  }

  connection_start_reading(TO_CONN(dirconn));
  connection_start_reading(TO_CONN(exitconn));

  if (connection_edge_send_command(exitconn,
                                   RELAY_COMMAND_CONNECTED, NULL, 0) < 0) {
    connection_mark_for_close(TO_CONN(exitconn));
    connection_mark_for_close(TO_CONN(dirconn));
    return 0;
  }

  return 0;
}

/** Return 1 if <b>conn</b> is a rendezvous stream, or 0 if
 * it is a general stream.
 */
int
connection_edge_is_rendezvous_stream(const edge_connection_t *conn)
{
  tor_assert(conn);
  /* It should not be possible to set both of these structs */
  tor_assert_nonfatal(!(conn->rend_data && conn->hs_ident));

  if (conn->rend_data || conn->hs_ident) {
    return 1;
  }
  return 0;
}

/** Return 1 if router <b>exit_node</b> is likely to allow stream <b>conn</b>
 * to exit from it, or 0 if it probably will not allow it.
 * (We might be uncertain if conn's destination address has not yet been
 * resolved.)
 */
int
connection_ap_can_use_exit(const entry_connection_t *conn,
                           const node_t *exit_node)
{
  const or_options_t *options = get_options();

  tor_assert(conn);
  tor_assert(conn->socks_request);
  tor_assert(exit_node);

  /* If a particular exit node has been requested for the new connection,
   * make sure the exit node of the existing circuit matches exactly.
   */
  if (conn->chosen_exit_name) {
    const node_t *chosen_exit =
      node_get_by_nickname(conn->chosen_exit_name, 0);
    if (!chosen_exit || tor_memneq(chosen_exit->identity,
                               exit_node->identity, DIGEST_LEN)) {
      /* doesn't match */
//      log_debug(LD_APP,"Requested node '%s', considering node '%s'. No.",
//                conn->chosen_exit_name, exit->nickname);
      return 0;
    }
  }

  if (conn->use_begindir) {
    /* Internal directory fetches do not count as exiting. */
    return 1;
  }

  if (conn->socks_request->command == SOCKS_COMMAND_CONNECT) {
    tor_addr_t addr, *addrp = NULL;
    addr_policy_result_t r;
    if (0 == tor_addr_parse(&addr, conn->socks_request->address)) {
      addrp = &addr;
    } else if (!conn->entry_cfg.ipv4_traffic && conn->entry_cfg.ipv6_traffic) {
      tor_addr_make_null(&addr, AF_INET6);
      addrp = &addr;
    } else if (conn->entry_cfg.ipv4_traffic && !conn->entry_cfg.ipv6_traffic) {
      tor_addr_make_null(&addr, AF_INET);
      addrp = &addr;
    }
    r = compare_tor_addr_to_node_policy(addrp, conn->socks_request->port,
                                        exit_node);
    if (r == ADDR_POLICY_REJECTED)
      return 0; /* We know the address, and the exit policy rejects it. */
    if (r == ADDR_POLICY_PROBABLY_REJECTED && !conn->chosen_exit_name)
      return 0; /* We don't know the addr, but the exit policy rejects most
                 * addresses with this port. Since the user didn't ask for
                 * this node, err on the side of caution. */
  } else if (SOCKS_COMMAND_IS_RESOLVE(conn->socks_request->command)) {
    /* Don't send DNS requests to non-exit servers by default. */
    if (!conn->chosen_exit_name && node_exit_policy_rejects_all(exit_node))
      return 0;
  }
  if (routerset_contains_node(options->ExcludeExitNodesUnion_, exit_node)) {
    /* Not a suitable exit. Refuse it. */
    return 0;
  }

  return 1;
}

/** If address is of the form "y.onion" with a well-formed handle y:
 *     Put a NUL after y, lower-case it, and return ONION_V2_HOSTNAME or
 *     ONION_V3_HOSTNAME depending on the HS version.
 *
 *  If address is of the form "x.y.onion" with a well-formed handle x:
 *     Drop "x.", put a NUL after y, lower-case it, and return
 *     ONION_V2_HOSTNAME or ONION_V3_HOSTNAME depending on the HS version.
 *
 * If address is of the form "y.onion" with a badly-formed handle y:
 *     Return BAD_HOSTNAME and log a message.
 *
 * If address is of the form "y.exit":
 *     Put a NUL after y and return EXIT_HOSTNAME.
 *
 * Otherwise:
 *     Return NORMAL_HOSTNAME and change nothing.
 */
hostname_type_t
parse_extended_hostname(char *address)
{
    char *s;
    char *q;
    char query[HS_SERVICE_ADDR_LEN_BASE32+1];

    s = strrchr(address,'.');
    if (!s)
      return NORMAL_HOSTNAME; /* no dot, thus normal */
    if (!strcmp(s+1,"exit")) {
      *s = 0; /* NUL-terminate it */
      return EXIT_HOSTNAME; /* .exit */
    }
    if (strcmp(s+1,"onion"))
      return NORMAL_HOSTNAME; /* neither .exit nor .onion, thus normal */

    /* so it is .onion */
    *s = 0; /* NUL-terminate it */
    /* locate a 'sub-domain' component, in order to remove it */
    q = strrchr(address, '.');
    if (q == address) {
      goto failed; /* reject sub-domain, as DNS does */
    }
    q = (NULL == q) ? address : q + 1;
    if (strlcpy(query, q, HS_SERVICE_ADDR_LEN_BASE32+1) >=
        HS_SERVICE_ADDR_LEN_BASE32+1)
      goto failed;
    if (q != address) {
      memmove(address, q, strlen(q) + 1 /* also get \0 */);
    }
    if (rend_valid_v2_service_id(query)) {
      return ONION_V2_HOSTNAME; /* success */
    }
    if (hs_address_is_valid(query)) {
      return ONION_V3_HOSTNAME;
    }
 failed:
    /* otherwise, return to previous state and return 0 */
    *s = '.';
    log_warn(LD_APP, "Invalid onion hostname %s; rejecting",
             safe_str_client(address));
    return BAD_HOSTNAME;
}

/** Return true iff the (possibly NULL) <b>alen</b>-byte chunk of memory at
 * <b>a</b> is equal to the (possibly NULL) <b>blen</b>-byte chunk of memory
 * at <b>b</b>. */
static int
memeq_opt(const char *a, size_t alen, const char *b, size_t blen)
{
  if (a == NULL) {
    return (b == NULL);
  } else if (b == NULL) {
    return 0;
  } else if (alen != blen) {
    return 0;
  } else {
    return tor_memeq(a, b, alen);
  }
}

/**
 * Return true iff none of the isolation flags and fields in <b>conn</b>
 * should prevent it from being attached to <b>circ</b>.
 */
int
connection_edge_compatible_with_circuit(const entry_connection_t *conn,
                                        const origin_circuit_t *circ)
{
  const uint8_t iso = conn->entry_cfg.isolation_flags;
  const socks_request_t *sr = conn->socks_request;

  /* If circ has never been used for an isolated connection, we can
   * totally use it for this one. */
  if (!circ->isolation_values_set)
    return 1;

  /* If circ has been used for connections having more than one value
   * for some field f, it will have the corresponding bit set in
   * isolation_flags_mixed.  If isolation_flags_mixed has any bits
   * in common with iso, then conn must be isolated from at least
   * one stream that has been attached to circ. */
  if ((iso & circ->isolation_flags_mixed) != 0) {
    /* For at least one field where conn is isolated, the circuit
     * already has mixed streams. */
    return 0;
  }

  if (! conn->original_dest_address) {
    log_warn(LD_BUG, "Reached connection_edge_compatible_with_circuit without "
             "having set conn->original_dest_address");
    ((entry_connection_t*)conn)->original_dest_address =
      tor_strdup(conn->socks_request->address);
  }

  if ((iso & ISO_STREAM) &&
      (circ->associated_isolated_stream_global_id !=
       ENTRY_TO_CONN(conn)->global_identifier))
    return 0;

  if ((iso & ISO_DESTPORT) && conn->socks_request->port != circ->dest_port)
    return 0;
  if ((iso & ISO_DESTADDR) &&
      strcasecmp(conn->original_dest_address, circ->dest_address))
    return 0;
  if ((iso & ISO_SOCKSAUTH) &&
      (! memeq_opt(sr->username, sr->usernamelen,
                   circ->socks_username, circ->socks_username_len) ||
       ! memeq_opt(sr->password, sr->passwordlen,
                   circ->socks_password, circ->socks_password_len)))
    return 0;
  if ((iso & ISO_CLIENTPROTO) &&
      (conn->socks_request->listener_type != circ->client_proto_type ||
       conn->socks_request->socks_version != circ->client_proto_socksver))
    return 0;
  if ((iso & ISO_CLIENTADDR) &&
      !tor_addr_eq(&ENTRY_TO_CONN(conn)->addr, &circ->client_addr))
    return 0;
  if ((iso & ISO_SESSIONGRP) &&
      conn->entry_cfg.session_group != circ->session_group)
    return 0;
  if ((iso & ISO_NYM_EPOCH) && conn->nym_epoch != circ->nym_epoch)
    return 0;

  return 1;
}

/**
 * If <b>dry_run</b> is false, update <b>circ</b>'s isolation flags and fields
 * to reflect having had <b>conn</b> attached to it, and return 0.  Otherwise,
 * if <b>dry_run</b> is true, then make no changes to <b>circ</b>, and return
 * a bitfield of isolation flags that we would have to set in
 * isolation_flags_mixed to add <b>conn</b> to <b>circ</b>, or -1 if
 * <b>circ</b> has had no streams attached to it.
 */
int
connection_edge_update_circuit_isolation(const entry_connection_t *conn,
                                         origin_circuit_t *circ,
                                         int dry_run)
{
  const socks_request_t *sr = conn->socks_request;
  if (! conn->original_dest_address) {
    log_warn(LD_BUG, "Reached connection_update_circuit_isolation without "
             "having set conn->original_dest_address");
    ((entry_connection_t*)conn)->original_dest_address =
      tor_strdup(conn->socks_request->address);
  }

  if (!circ->isolation_values_set) {
    if (dry_run)
      return -1;
    circ->associated_isolated_stream_global_id =
      ENTRY_TO_CONN(conn)->global_identifier;
    circ->dest_port = conn->socks_request->port;
    circ->dest_address = tor_strdup(conn->original_dest_address);
    circ->client_proto_type = conn->socks_request->listener_type;
    circ->client_proto_socksver = conn->socks_request->socks_version;
    tor_addr_copy(&circ->client_addr, &ENTRY_TO_CONN(conn)->addr);
    circ->session_group = conn->entry_cfg.session_group;
    circ->nym_epoch = conn->nym_epoch;
    circ->socks_username = sr->username ?
      tor_memdup(sr->username, sr->usernamelen) : NULL;
    circ->socks_password = sr->password ?
      tor_memdup(sr->password, sr->passwordlen) : NULL;
    circ->socks_username_len = sr->usernamelen;
    circ->socks_password_len = sr->passwordlen;

    circ->isolation_values_set = 1;
    return 0;
  } else {
    uint8_t mixed = 0;
    if (conn->socks_request->port != circ->dest_port)
      mixed |= ISO_DESTPORT;
    if (strcasecmp(conn->original_dest_address, circ->dest_address))
      mixed |= ISO_DESTADDR;
    if (!memeq_opt(sr->username, sr->usernamelen,
                   circ->socks_username, circ->socks_username_len) ||
        !memeq_opt(sr->password, sr->passwordlen,
                   circ->socks_password, circ->socks_password_len))
      mixed |= ISO_SOCKSAUTH;
    if ((conn->socks_request->listener_type != circ->client_proto_type ||
         conn->socks_request->socks_version != circ->client_proto_socksver))
      mixed |= ISO_CLIENTPROTO;
    if (!tor_addr_eq(&ENTRY_TO_CONN(conn)->addr, &circ->client_addr))
      mixed |= ISO_CLIENTADDR;
    if (conn->entry_cfg.session_group != circ->session_group)
      mixed |= ISO_SESSIONGRP;
    if (conn->nym_epoch != circ->nym_epoch)
      mixed |= ISO_NYM_EPOCH;

    if (dry_run)
      return mixed;

    if ((mixed & conn->entry_cfg.isolation_flags) != 0) {
      log_warn(LD_BUG, "Updating a circuit with seemingly incompatible "
               "isolation flags.");
    }
    circ->isolation_flags_mixed |= mixed;
    return 0;
  }
}

/**
 * Clear the isolation settings on <b>circ</b>.
 *
 * This only works on an open circuit that has never had a stream attached to
 * it, and whose isolation settings are hypothetical.  (We set hypothetical
 * isolation settings on circuits as we're launching them, so that we
 * know whether they can handle more streams or whether we need to launch
 * even more circuits.  Once the circuit is open, if it turns out that
 * we no longer have any streams to attach to it, we clear the isolation flags
 * and data so that other streams can have a chance.)
 */
void
circuit_clear_isolation(origin_circuit_t *circ)
{
  if (circ->isolation_any_streams_attached) {
    log_warn(LD_BUG, "Tried to clear the isolation status of a dirty circuit");
    return;
  }
  if (TO_CIRCUIT(circ)->state != CIRCUIT_STATE_OPEN) {
    log_warn(LD_BUG, "Tried to clear the isolation status of a non-open "
             "circuit");
    return;
  }

  circ->isolation_values_set = 0;
  circ->isolation_flags_mixed = 0;
  circ->associated_isolated_stream_global_id = 0;
  circ->client_proto_type = 0;
  circ->client_proto_socksver = 0;
  circ->dest_port = 0;
  tor_addr_make_unspec(&circ->client_addr);
  tor_free(circ->dest_address);
  circ->session_group = -1;
  circ->nym_epoch = 0;
  if (circ->socks_username) {
    memwipe(circ->socks_username, 0x11, circ->socks_username_len);
    tor_free(circ->socks_username);
  }
  if (circ->socks_password) {
    memwipe(circ->socks_password, 0x05, circ->socks_password_len);
    tor_free(circ->socks_password);
  }
  circ->socks_username_len = circ->socks_password_len = 0;
}

/** Free all storage held in module-scoped variables for connection_edge.c */
void
connection_edge_free_all(void)
{
  untried_pending_connections = 0;
  smartlist_free(pending_entry_connections);
  pending_entry_connections = NULL;
  mainloop_event_free(attach_pending_entry_connections_ev);
}
