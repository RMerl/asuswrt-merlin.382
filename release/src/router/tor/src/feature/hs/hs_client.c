/* Copyright (c) 2016-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file hs_client.c
 * \brief Implement next generation hidden service client functionality
 **/

#define HS_CLIENT_PRIVATE

#include "core/or/or.h"
#include "app/config/config.h"
#include "core/crypto/hs_ntor.h"
#include "core/mainloop/connection.h"
#include "core/or/circuitbuild.h"
#include "core/or/circuitlist.h"
#include "core/or/circuituse.h"
#include "core/or/connection_edge.h"
#include "core/or/reasons.h"
#include "feature/client/circpathbias.h"
#include "feature/dirclient/dirclient.h"
#include "feature/dircommon/directory.h"
#include "feature/hs/hs_cache.h"
#include "feature/hs/hs_cell.h"
#include "feature/hs/hs_circuit.h"
#include "feature/hs/hs_circuitmap.h"
#include "feature/hs/hs_client.h"
#include "feature/hs/hs_control.h"
#include "feature/hs/hs_descriptor.h"
#include "feature/hs/hs_ident.h"
#include "feature/nodelist/describe.h"
#include "feature/nodelist/networkstatus.h"
#include "feature/nodelist/nodelist.h"
#include "feature/nodelist/routerset.h"
#include "feature/rend/rendclient.h"
#include "lib/crypt_ops/crypto_format.h"
#include "lib/crypt_ops/crypto_rand.h"
#include "lib/crypt_ops/crypto_util.h"

#include "core/or/cpath_build_state_st.h"
#include "feature/dircommon/dir_connection_st.h"
#include "core/or/entry_connection_st.h"
#include "core/or/extend_info_st.h"
#include "core/or/origin_circuit_st.h"

/* Client-side authorizations for hidden services; map of service identity
 * public key to hs_client_service_authorization_t *. */
static digest256map_t *client_auths = NULL;

/* Return a human-readable string for the client fetch status code. */
static const char *
fetch_status_to_string(hs_client_fetch_status_t status)
{
  switch (status) {
  case HS_CLIENT_FETCH_ERROR:
    return "Internal error";
  case HS_CLIENT_FETCH_LAUNCHED:
    return "Descriptor fetch launched";
  case HS_CLIENT_FETCH_HAVE_DESC:
    return "Already have descriptor";
  case HS_CLIENT_FETCH_NO_HSDIRS:
    return "No more HSDir available to query";
  case HS_CLIENT_FETCH_NOT_ALLOWED:
    return "Fetching descriptors is not allowed";
  case HS_CLIENT_FETCH_MISSING_INFO:
    return "Missing directory information";
  case HS_CLIENT_FETCH_PENDING:
    return "Pending descriptor fetch";
  default:
    return "(Unknown client fetch status code)";
  }
}

/* Return true iff tor should close the SOCKS request(s) for the descriptor
 * fetch that ended up with this given status code. */
static int
fetch_status_should_close_socks(hs_client_fetch_status_t status)
{
  switch (status) {
  case HS_CLIENT_FETCH_NO_HSDIRS:
    /* No more HSDir to query, we can't complete the SOCKS request(s). */
  case HS_CLIENT_FETCH_ERROR:
    /* The fetch triggered an internal error. */
  case HS_CLIENT_FETCH_NOT_ALLOWED:
    /* Client is not allowed to fetch (FetchHidServDescriptors 0). */
    goto close;
  case HS_CLIENT_FETCH_MISSING_INFO:
  case HS_CLIENT_FETCH_HAVE_DESC:
  case HS_CLIENT_FETCH_PENDING:
  case HS_CLIENT_FETCH_LAUNCHED:
    /* The rest doesn't require tor to close the SOCKS request(s). */
    goto no_close;
  }

 no_close:
  return 0;
 close:
  return 1;
}

/* Cancel all descriptor fetches currently in progress. */
static void
cancel_descriptor_fetches(void)
{
  smartlist_t *conns =
    connection_list_by_type_state(CONN_TYPE_DIR, DIR_PURPOSE_FETCH_HSDESC);
  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, conn) {
    const hs_ident_dir_conn_t *ident = TO_DIR_CONN(conn)->hs_ident;
    if (BUG(ident == NULL)) {
      /* A directory connection fetching a service descriptor can't have an
       * empty hidden service identifier. */
      continue;
    }
    log_debug(LD_REND, "Marking for close a directory connection fetching "
                       "a hidden service descriptor for service %s.",
              safe_str_client(ed25519_fmt(&ident->identity_pk)));
    connection_mark_for_close(conn);
  } SMARTLIST_FOREACH_END(conn);

  /* No ownership of the objects in this list. */
  smartlist_free(conns);
  log_info(LD_REND, "Hidden service client descriptor fetches cancelled.");
}

/* Get all connections that are waiting on a circuit and flag them back to
 * waiting for a hidden service descriptor for the given service key
 * service_identity_pk. */
static void
flag_all_conn_wait_desc(const ed25519_public_key_t *service_identity_pk)
{
  tor_assert(service_identity_pk);

  smartlist_t *conns =
    connection_list_by_type_state(CONN_TYPE_AP, AP_CONN_STATE_CIRCUIT_WAIT);

  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, conn) {
    edge_connection_t *edge_conn;
    if (BUG(!CONN_IS_EDGE(conn))) {
      continue;
    }
    edge_conn = TO_EDGE_CONN(conn);
    if (edge_conn->hs_ident &&
        ed25519_pubkey_eq(&edge_conn->hs_ident->identity_pk,
                          service_identity_pk)) {
      connection_ap_mark_as_waiting_for_renddesc(TO_ENTRY_CONN(conn));
    }
  } SMARTLIST_FOREACH_END(conn);

  smartlist_free(conns);
}

/* Remove tracked HSDir requests from our history for this hidden service
 * identity public key. */
static void
purge_hid_serv_request(const ed25519_public_key_t *identity_pk)
{
  char base64_blinded_pk[ED25519_BASE64_LEN + 1];
  ed25519_public_key_t blinded_pk;

  tor_assert(identity_pk);

  /* Get blinded pubkey of hidden service. It is possible that we just moved
   * to a new time period meaning that we won't be able to purge the request
   * from the previous time period. That is fine because they will expire at
   * some point and we don't care about those anymore. */
  hs_build_blinded_pubkey(identity_pk, NULL, 0,
                          hs_get_time_period_num(0), &blinded_pk);
  if (BUG(ed25519_public_to_base64(base64_blinded_pk, &blinded_pk) < 0)) {
    return;
  }
  /* Purge last hidden service request from cache for this blinded key. */
  hs_purge_hid_serv_from_last_hid_serv_requests(base64_blinded_pk);
}

/* Return true iff there is at least one pending directory descriptor request
 * for the service identity_pk. */
static int
directory_request_is_pending(const ed25519_public_key_t *identity_pk)
{
  int ret = 0;
  smartlist_t *conns =
    connection_list_by_type_purpose(CONN_TYPE_DIR, DIR_PURPOSE_FETCH_HSDESC);

  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, conn) {
    const hs_ident_dir_conn_t *ident = TO_DIR_CONN(conn)->hs_ident;
    if (BUG(ident == NULL)) {
      /* A directory connection fetching a service descriptor can't have an
       * empty hidden service identifier. */
      continue;
    }
    if (!ed25519_pubkey_eq(identity_pk, &ident->identity_pk)) {
      continue;
    }
    ret = 1;
    break;
  } SMARTLIST_FOREACH_END(conn);

  /* No ownership of the objects in this list. */
  smartlist_free(conns);
  return ret;
}

/* Helper function that changes the state of an entry connection to waiting
 * for a circuit. For this to work properly, the connection timestamps are set
 * to now and the connection is then marked as pending for a circuit. */
static void
mark_conn_as_waiting_for_circuit(connection_t *conn, time_t now)
{
  tor_assert(conn);

  /* Because the connection can now proceed to opening circuit and ultimately
   * connect to the service, reset those timestamp so the connection is
   * considered "fresh" and can continue without being closed too early. */
  conn->timestamp_created = now;
  conn->timestamp_last_read_allowed = now;
  conn->timestamp_last_write_allowed = now;
  /* Change connection's state into waiting for a circuit. */
  conn->state = AP_CONN_STATE_CIRCUIT_WAIT;

  connection_ap_mark_as_pending_circuit(TO_ENTRY_CONN(conn));
}

/* We failed to fetch a descriptor for the service with <b>identity_pk</b>
 * because of <b>status</b>. Find all pending SOCKS connections for this
 * service that are waiting on the descriptor and close them with
 * <b>reason</b>. */
static void
close_all_socks_conns_waiting_for_desc(const ed25519_public_key_t *identity_pk,
                                       hs_client_fetch_status_t status,
                                       int reason)
{
  unsigned int count = 0;
  time_t now = approx_time();
  smartlist_t *conns =
    connection_list_by_type_state(CONN_TYPE_AP, AP_CONN_STATE_RENDDESC_WAIT);

  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, base_conn) {
    entry_connection_t *entry_conn = TO_ENTRY_CONN(base_conn);
    const edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(entry_conn);

    /* Only consider the entry connections that matches the service for which
     * we tried to get the descriptor */
    if (!edge_conn->hs_ident ||
        !ed25519_pubkey_eq(identity_pk,
                           &edge_conn->hs_ident->identity_pk)) {
      continue;
    }
    assert_connection_ok(base_conn, now);
    /* Unattach the entry connection which will close for the reason. */
    connection_mark_unattached_ap(entry_conn, reason);
    count++;
  } SMARTLIST_FOREACH_END(base_conn);

  if (count > 0) {
    char onion_address[HS_SERVICE_ADDR_LEN_BASE32 + 1];
    hs_build_address(identity_pk, HS_VERSION_THREE, onion_address);
    log_notice(LD_REND, "Closed %u streams for service %s.onion "
                        "for reason %s. Fetch status: %s.",
               count, safe_str_client(onion_address),
               stream_end_reason_to_string(reason),
               fetch_status_to_string(status));
  }

  /* No ownership of the object(s) in this list. */
  smartlist_free(conns);
}

/* Find all pending SOCKS connection waiting for a descriptor and retry them
 * all. This is called when the directory information changed. */
STATIC void
retry_all_socks_conn_waiting_for_desc(void)
{
  smartlist_t *conns =
    connection_list_by_type_state(CONN_TYPE_AP, AP_CONN_STATE_RENDDESC_WAIT);

  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, base_conn) {
    hs_client_fetch_status_t status;
    const edge_connection_t *edge_conn =
      ENTRY_TO_EDGE_CONN(TO_ENTRY_CONN(base_conn));

    /* Ignore non HS or non v3 connection. */
    if (edge_conn->hs_ident == NULL) {
      continue;
    }
    /* In this loop, we will possibly try to fetch a descriptor for the
     * pending connections because we just got more directory information.
     * However, the refetch process can cleanup all SOCKS request to the same
     * service if an internal error happens. Thus, we can end up with closed
     * connections in our list. */
    if (base_conn->marked_for_close) {
      continue;
    }

    /* XXX: There is an optimization we could do which is that for a service
     * key, we could check if we can fetch and remember that decision. */

    /* Order a refetch in case it works this time. */
    status = hs_client_refetch_hsdesc(&edge_conn->hs_ident->identity_pk);
    if (status == HS_CLIENT_FETCH_HAVE_DESC) {
      /* This is a rare case where a SOCKS connection is in state waiting for
       * a descriptor but we do have it in the cache.
       *
       * This can happen is tor comes back from suspend where it previously
       * had the descriptor but the intro points were not usuable. Once it
       * came back to life, the intro point failure cache was cleaned up and
       * thus the descriptor became usable again leaving us in this code path.
       *
       * We'll mark the connection as waiting for a circuit so the descriptor
       * can be retried. This is safe because a connection in state waiting
       * for a descriptor can not be in the entry connection pending list. */
      mark_conn_as_waiting_for_circuit(base_conn, approx_time());
      continue;
    }
    /* In the case of an error, either all SOCKS connections have been
     * closed or we are still missing directory information. Leave the
     * connection in renddesc wait state so when we get more info, we'll be
     * able to try it again. */
  } SMARTLIST_FOREACH_END(base_conn);

  /* We don't have ownership of those objects. */
  smartlist_free(conns);
}

/* A v3 HS circuit successfully connected to the hidden service. Update the
 * stream state at <b>hs_conn_ident</b> appropriately. */
static void
note_connection_attempt_succeeded(const hs_ident_edge_conn_t *hs_conn_ident)
{
  tor_assert(hs_conn_ident);

  /* Remove from the hid serv cache all requests for that service so we can
   * query the HSDir again later on for various reasons. */
  purge_hid_serv_request(&hs_conn_ident->identity_pk);

  /* The v2 subsystem cleans up the intro point time out flag at this stage.
   * We don't try to do it here because we still need to keep intact the intro
   * point state for future connections. Even though we are able to connect to
   * the service, doesn't mean we should reset the timed out intro points.
   *
   * It is not possible to have successfully connected to an intro point
   * present in our cache that was on error or timed out. Every entry in that
   * cache have a 2 minutes lifetime so ultimately the intro point(s) state
   * will be reset and thus possible to be retried. */
}

/* Given the pubkey of a hidden service in <b>onion_identity_pk</b>, fetch its
 * descriptor by launching a dir connection to <b>hsdir</b>. Return a
 * hs_client_fetch_status_t status code depending on how it went. */
static hs_client_fetch_status_t
directory_launch_v3_desc_fetch(const ed25519_public_key_t *onion_identity_pk,
                               const routerstatus_t *hsdir)
{
  uint64_t current_time_period = hs_get_time_period_num(0);
  ed25519_public_key_t blinded_pubkey;
  char base64_blinded_pubkey[ED25519_BASE64_LEN + 1];
  hs_ident_dir_conn_t hs_conn_dir_ident;
  int retval;

  tor_assert(hsdir);
  tor_assert(onion_identity_pk);

  /* Get blinded pubkey */
  hs_build_blinded_pubkey(onion_identity_pk, NULL, 0,
                          current_time_period, &blinded_pubkey);
  /* ...and base64 it. */
  retval = ed25519_public_to_base64(base64_blinded_pubkey, &blinded_pubkey);
  if (BUG(retval < 0)) {
    return HS_CLIENT_FETCH_ERROR;
  }

  /* Copy onion pk to a dir_ident so that we attach it to the dir conn */
  hs_ident_dir_conn_init(onion_identity_pk, &blinded_pubkey,
                         &hs_conn_dir_ident);

  /* Setup directory request */
  directory_request_t *req =
    directory_request_new(DIR_PURPOSE_FETCH_HSDESC);
  directory_request_set_routerstatus(req, hsdir);
  directory_request_set_indirection(req, DIRIND_ANONYMOUS);
  directory_request_set_resource(req, base64_blinded_pubkey);
  directory_request_fetch_set_hs_ident(req, &hs_conn_dir_ident);
  directory_initiate_request(req);
  directory_request_free(req);

  log_info(LD_REND, "Descriptor fetch request for service %s with blinded "
                    "key %s to directory %s",
           safe_str_client(ed25519_fmt(onion_identity_pk)),
           safe_str_client(base64_blinded_pubkey),
           safe_str_client(routerstatus_describe(hsdir)));

  /* Fire a REQUESTED event on the control port. */
  hs_control_desc_event_requested(onion_identity_pk, base64_blinded_pubkey,
                                  hsdir);

  /* Cleanup memory. */
  memwipe(&blinded_pubkey, 0, sizeof(blinded_pubkey));
  memwipe(base64_blinded_pubkey, 0, sizeof(base64_blinded_pubkey));
  memwipe(&hs_conn_dir_ident, 0, sizeof(hs_conn_dir_ident));

  return HS_CLIENT_FETCH_LAUNCHED;
}

/** Return the HSDir we should use to fetch the descriptor of the hidden
 *  service with identity key <b>onion_identity_pk</b>. */
STATIC routerstatus_t *
pick_hsdir_v3(const ed25519_public_key_t *onion_identity_pk)
{
  int retval;
  char base64_blinded_pubkey[ED25519_BASE64_LEN + 1];
  uint64_t current_time_period = hs_get_time_period_num(0);
  smartlist_t *responsible_hsdirs = NULL;
  ed25519_public_key_t blinded_pubkey;
  routerstatus_t *hsdir_rs = NULL;

  tor_assert(onion_identity_pk);

  /* Get blinded pubkey of hidden service */
  hs_build_blinded_pubkey(onion_identity_pk, NULL, 0,
                          current_time_period, &blinded_pubkey);
  /* ...and base64 it. */
  retval = ed25519_public_to_base64(base64_blinded_pubkey, &blinded_pubkey);
  if (BUG(retval < 0)) {
    return NULL;
  }

  /* Get responsible hsdirs of service for this time period */
  responsible_hsdirs = smartlist_new();

  hs_get_responsible_hsdirs(&blinded_pubkey, current_time_period,
                            0, 1, responsible_hsdirs);

  log_debug(LD_REND, "Found %d responsible HSDirs and about to pick one.",
           smartlist_len(responsible_hsdirs));

  /* Pick an HSDir from the responsible ones. The ownership of
   * responsible_hsdirs is given to this function so no need to free it. */
  hsdir_rs = hs_pick_hsdir(responsible_hsdirs, base64_blinded_pubkey);

  return hsdir_rs;
}

/** Fetch a v3 descriptor using the given <b>onion_identity_pk</b>.
 *
 * On success, HS_CLIENT_FETCH_LAUNCHED is returned. Otherwise, an error from
 * hs_client_fetch_status_t is returned. */
MOCK_IMPL(STATIC hs_client_fetch_status_t,
fetch_v3_desc, (const ed25519_public_key_t *onion_identity_pk))
{
  routerstatus_t *hsdir_rs =NULL;

  tor_assert(onion_identity_pk);

  hsdir_rs = pick_hsdir_v3(onion_identity_pk);
  if (!hsdir_rs) {
    log_info(LD_REND, "Couldn't pick a v3 hsdir.");
    return HS_CLIENT_FETCH_NO_HSDIRS;
  }

  return directory_launch_v3_desc_fetch(onion_identity_pk, hsdir_rs);
}

/* Make sure that the given v3 origin circuit circ is a valid correct
 * introduction circuit. This will BUG() on any problems and hard assert if
 * the anonymity of the circuit is not ok. Return 0 on success else -1 where
 * the circuit should be mark for closed immediately. */
static int
intro_circ_is_ok(const origin_circuit_t *circ)
{
  int ret = 0;

  tor_assert(circ);

  if (BUG(TO_CIRCUIT(circ)->purpose != CIRCUIT_PURPOSE_C_INTRODUCING &&
          TO_CIRCUIT(circ)->purpose != CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT &&
          TO_CIRCUIT(circ)->purpose != CIRCUIT_PURPOSE_C_INTRODUCE_ACKED)) {
    ret = -1;
  }
  if (BUG(circ->hs_ident == NULL)) {
    ret = -1;
  }
  if (BUG(!hs_ident_intro_circ_is_valid(circ->hs_ident))) {
    ret = -1;
  }

  /* This can stop the tor daemon but we want that since if we don't have
   * anonymity on this circuit, something went really wrong. */
  assert_circ_anonymity_ok(circ, get_options());
  return ret;
}

/* Find a descriptor intro point object that matches the given ident in the
 * given descriptor desc. Return NULL if not found. */
static const hs_desc_intro_point_t *
find_desc_intro_point_by_ident(const hs_ident_circuit_t *ident,
                               const hs_descriptor_t *desc)
{
  const hs_desc_intro_point_t *intro_point = NULL;

  tor_assert(ident);
  tor_assert(desc);

  SMARTLIST_FOREACH_BEGIN(desc->encrypted_data.intro_points,
                          const hs_desc_intro_point_t *, ip) {
    if (ed25519_pubkey_eq(&ident->intro_auth_pk,
                          &ip->auth_key_cert->signed_key)) {
      intro_point = ip;
      break;
    }
  } SMARTLIST_FOREACH_END(ip);

  return intro_point;
}

/* Find a descriptor intro point object from the descriptor object desc that
 * matches the given legacy identity digest in legacy_id. Return NULL if not
 * found. */
static hs_desc_intro_point_t *
find_desc_intro_point_by_legacy_id(const char *legacy_id,
                                   const hs_descriptor_t *desc)
{
  hs_desc_intro_point_t *ret_ip = NULL;

  tor_assert(legacy_id);
  tor_assert(desc);

  /* We will go over every intro point and try to find which one is linked to
   * that circuit. Those lists are small so it's not that expensive. */
  SMARTLIST_FOREACH_BEGIN(desc->encrypted_data.intro_points,
                          hs_desc_intro_point_t *, ip) {
    SMARTLIST_FOREACH_BEGIN(ip->link_specifiers,
                            const hs_desc_link_specifier_t *, lspec) {
      /* Not all tor node have an ed25519 identity key so we still rely on the
       * legacy identity digest. */
      if (lspec->type != LS_LEGACY_ID) {
        continue;
      }
      if (fast_memneq(legacy_id, lspec->u.legacy_id, DIGEST_LEN)) {
        break;
      }
      /* Found it. */
      ret_ip = ip;
      goto end;
    } SMARTLIST_FOREACH_END(lspec);
  } SMARTLIST_FOREACH_END(ip);

 end:
  return ret_ip;
}

/* Send an INTRODUCE1 cell along the intro circuit and populate the rend
 * circuit identifier with the needed key material for the e2e encryption.
 * Return 0 on success, -1 if there is a transient error such that an action
 * has been taken to recover and -2 if there is a permanent error indicating
 * that both circuits were closed. */
static int
send_introduce1(origin_circuit_t *intro_circ,
                origin_circuit_t *rend_circ)
{
  int status;
  char onion_address[HS_SERVICE_ADDR_LEN_BASE32 + 1];
  const ed25519_public_key_t *service_identity_pk = NULL;
  const hs_desc_intro_point_t *ip;

  tor_assert(rend_circ);
  if (intro_circ_is_ok(intro_circ) < 0) {
    goto perm_err;
  }

  service_identity_pk = &intro_circ->hs_ident->identity_pk;
  /* For logging purposes. There will be a time where the hs_ident will have a
   * version number but for now there is none because it's all v3. */
  hs_build_address(service_identity_pk, HS_VERSION_THREE, onion_address);

  log_info(LD_REND, "Sending INTRODUCE1 cell to service %s on circuit %u",
           safe_str_client(onion_address), TO_CIRCUIT(intro_circ)->n_circ_id);

  /* 1) Get descriptor from our cache. */
  const hs_descriptor_t *desc =
    hs_cache_lookup_as_client(service_identity_pk);
  if (desc == NULL || !hs_client_any_intro_points_usable(service_identity_pk,
                                                         desc)) {
    log_info(LD_REND, "Request to %s %s. Trying to fetch a new descriptor.",
             safe_str_client(onion_address),
             (desc) ? "didn't have usable intro points" :
             "didn't have a descriptor");
    hs_client_refetch_hsdesc(service_identity_pk);
    /* We just triggered a refetch, make sure every connections are back
     * waiting for that descriptor. */
    flag_all_conn_wait_desc(service_identity_pk);
    /* We just asked for a refetch so this is a transient error. */
    goto tran_err;
  }

  /* We need to find which intro point in the descriptor we are connected to
   * on intro_circ. */
  ip = find_desc_intro_point_by_ident(intro_circ->hs_ident, desc);
  if (BUG(ip == NULL)) {
    /* If we can find a descriptor from this introduction circuit ident, we
     * must have a valid intro point object. Permanent error. */
    goto perm_err;
  }

  /* Send the INTRODUCE1 cell. */
  if (hs_circ_send_introduce1(intro_circ, rend_circ, ip,
                              desc->subcredential) < 0) {
    if (TO_CIRCUIT(intro_circ)->marked_for_close) {
      /* If the introduction circuit was closed, we were unable to send the
       * cell for some reasons. In any case, the intro circuit has to be
       * closed by the above function. We'll return a transient error so tor
       * can recover and pick a new intro point. To avoid picking that same
       * intro point, we'll note down the intro point failure so it doesn't
       * get reused. */
      hs_cache_client_intro_state_note(service_identity_pk,
                                       &intro_circ->hs_ident->intro_auth_pk,
                                       INTRO_POINT_FAILURE_GENERIC);
    }
    /* It is also possible that the rendezvous circuit was closed due to being
     * unable to use the rendezvous point node_t so in that case, we also want
     * to recover and let tor pick a new one. */
    goto tran_err;
  }

  /* Cell has been sent successfully. Copy the introduction point
   * authentication and encryption key in the rendezvous circuit identifier so
   * we can compute the ntor keys when we receive the RENDEZVOUS2 cell. */
  memcpy(&rend_circ->hs_ident->intro_enc_pk, &ip->enc_key,
         sizeof(rend_circ->hs_ident->intro_enc_pk));
  ed25519_pubkey_copy(&rend_circ->hs_ident->intro_auth_pk,
                      &intro_circ->hs_ident->intro_auth_pk);

  /* Now, we wait for an ACK or NAK on this circuit. */
  circuit_change_purpose(TO_CIRCUIT(intro_circ),
                         CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT);
  /* Set timestamp_dirty, because circuit_expire_building expects it to
   * specify when a circuit entered the _C_INTRODUCE_ACK_WAIT state. */
  TO_CIRCUIT(intro_circ)->timestamp_dirty = time(NULL);
  pathbias_count_use_attempt(intro_circ);

  /* Success. */
  status = 0;
  goto end;

 perm_err:
  /* Permanent error: it is possible that the intro circuit was closed prior
   * because we weren't able to send the cell. Make sure we don't double close
   * it which would result in a warning. */
  if (!TO_CIRCUIT(intro_circ)->marked_for_close) {
    circuit_mark_for_close(TO_CIRCUIT(intro_circ), END_CIRC_REASON_INTERNAL);
  }
  circuit_mark_for_close(TO_CIRCUIT(rend_circ), END_CIRC_REASON_INTERNAL);
  status = -2;
  goto end;

 tran_err:
  status = -1;

 end:
  memwipe(onion_address, 0, sizeof(onion_address));
  return status;
}

/* Using the introduction circuit circ, setup the authentication key of the
 * intro point this circuit has extended to. */
static void
setup_intro_circ_auth_key(origin_circuit_t *circ)
{
  const hs_descriptor_t *desc;
  const hs_desc_intro_point_t *ip;

  tor_assert(circ);

  desc = hs_cache_lookup_as_client(&circ->hs_ident->identity_pk);
  if (BUG(desc == NULL)) {
    /* Opening intro circuit without the descriptor is no good... */
    goto end;
  }

  /* We will go over every intro point and try to find which one is linked to
   * that circuit. Those lists are small so it's not that expensive. */
  ip = find_desc_intro_point_by_legacy_id(
                       circ->build_state->chosen_exit->identity_digest, desc);
  if (ip) {
    /* We got it, copy its authentication key to the identifier. */
    ed25519_pubkey_copy(&circ->hs_ident->intro_auth_pk,
                        &ip->auth_key_cert->signed_key);
    goto end;
  }

  /* Reaching this point means we didn't find any intro point for this circuit
   * which is not suppose to happen. */
  tor_assert_nonfatal_unreached();

 end:
  return;
}

/* Called when an introduction circuit has opened. */
static void
client_intro_circ_has_opened(origin_circuit_t *circ)
{
  tor_assert(circ);
  tor_assert(TO_CIRCUIT(circ)->purpose == CIRCUIT_PURPOSE_C_INTRODUCING);
  log_info(LD_REND, "Introduction circuit %u has opened. Attaching streams.",
           (unsigned int) TO_CIRCUIT(circ)->n_circ_id);

  /* This is an introduction circuit so we'll attach the correct
   * authentication key to the circuit identifier so it can be identified
   * properly later on. */
  setup_intro_circ_auth_key(circ);

  connection_ap_attach_pending(1);
}

/* Called when a rendezvous circuit has opened. */
static void
client_rendezvous_circ_has_opened(origin_circuit_t *circ)
{
  tor_assert(circ);
  tor_assert(TO_CIRCUIT(circ)->purpose == CIRCUIT_PURPOSE_C_ESTABLISH_REND);

  const extend_info_t *rp_ei = circ->build_state->chosen_exit;

  /* Check that we didn't accidentally choose a node that does not understand
   * the v3 rendezvous protocol */
  if (rp_ei) {
    const node_t *rp_node = node_get_by_id(rp_ei->identity_digest);
    if (rp_node) {
      if (BUG(!node_supports_v3_rendezvous_point(rp_node))) {
        return;
      }
    }
  }

  log_info(LD_REND, "Rendezvous circuit has opened to %s.",
           safe_str_client(extend_info_describe(rp_ei)));

  /* Ignore returned value, nothing we can really do. On failure, the circuit
   * will be marked for close. */
  hs_circ_send_establish_rendezvous(circ);

  /* Register rend circuit in circuitmap if it's still alive. */
  if (!TO_CIRCUIT(circ)->marked_for_close) {
    hs_circuitmap_register_rend_circ_client_side(circ,
                                     circ->hs_ident->rendezvous_cookie);
  }
}

/* This is an helper function that convert a descriptor intro point object ip
 * to a newly allocated extend_info_t object fully initialized. Return NULL if
 * we can't convert it for which chances are that we are missing or malformed
 * link specifiers. */
STATIC extend_info_t *
desc_intro_point_to_extend_info(const hs_desc_intro_point_t *ip)
{
  extend_info_t *ei;
  smartlist_t *lspecs = smartlist_new();

  tor_assert(ip);

  /* We first encode the descriptor link specifiers into the binary
   * representation which is a trunnel object. */
  SMARTLIST_FOREACH_BEGIN(ip->link_specifiers,
                          const hs_desc_link_specifier_t *, desc_lspec) {
    link_specifier_t *lspec = hs_desc_lspec_to_trunnel(desc_lspec);
    smartlist_add(lspecs, lspec);
  } SMARTLIST_FOREACH_END(desc_lspec);

  /* Explicitly put the direct connection option to 0 because this is client
   * side and there is no such thing as a non anonymous client. */
  ei = hs_get_extend_info_from_lspecs(lspecs, &ip->onion_key, 0);

  SMARTLIST_FOREACH(lspecs, link_specifier_t *, ls, link_specifier_free(ls));
  smartlist_free(lspecs);
  return ei;
}

/* Return true iff the intro point ip for the service service_pk is usable.
 * This function checks if the intro point is in the client intro state cache
 * and checks at the failures. It is considered usable if:
 *   - No error happened (INTRO_POINT_FAILURE_GENERIC)
 *   - It is not flagged as timed out (INTRO_POINT_FAILURE_TIMEOUT)
 *   - The unreachable count is lower than
 *     MAX_INTRO_POINT_REACHABILITY_FAILURES (INTRO_POINT_FAILURE_UNREACHABLE)
 */
static int
intro_point_is_usable(const ed25519_public_key_t *service_pk,
                      const hs_desc_intro_point_t *ip)
{
  const hs_cache_intro_state_t *state;

  tor_assert(service_pk);
  tor_assert(ip);

  state = hs_cache_client_intro_state_find(service_pk,
                                           &ip->auth_key_cert->signed_key);
  if (state == NULL) {
    /* This means we've never encountered any problem thus usable. */
    goto usable;
  }
  if (state->error) {
    log_info(LD_REND, "Intro point with auth key %s had an error. Not usable",
             safe_str_client(ed25519_fmt(&ip->auth_key_cert->signed_key)));
    goto not_usable;
  }
  if (state->timed_out) {
    log_info(LD_REND, "Intro point with auth key %s timed out. Not usable",
             safe_str_client(ed25519_fmt(&ip->auth_key_cert->signed_key)));
    goto not_usable;
  }
  if (state->unreachable_count >= MAX_INTRO_POINT_REACHABILITY_FAILURES) {
    log_info(LD_REND, "Intro point with auth key %s unreachable. Not usable",
             safe_str_client(ed25519_fmt(&ip->auth_key_cert->signed_key)));
    goto not_usable;
  }

 usable:
  return 1;
 not_usable:
  return 0;
}

/* Using a descriptor desc, return a newly allocated extend_info_t object of a
 * randomly picked introduction point from its list. Return NULL if none are
 * usable. */
STATIC extend_info_t *
client_get_random_intro(const ed25519_public_key_t *service_pk)
{
  extend_info_t *ei = NULL, *ei_excluded = NULL;
  smartlist_t *usable_ips = NULL;
  const hs_descriptor_t *desc;
  const hs_desc_encrypted_data_t *enc_data;
  const or_options_t *options = get_options();
  /* Calculate the onion address for logging purposes */
  char onion_address[HS_SERVICE_ADDR_LEN_BASE32 + 1];

  tor_assert(service_pk);

  desc = hs_cache_lookup_as_client(service_pk);
  /* Assume the service is v3 if the descriptor is missing. This is ok,
   * because we only use the address in log messages */
  hs_build_address(service_pk,
                   desc ? desc->plaintext_data.version : HS_VERSION_THREE,
                   onion_address);
  if (desc == NULL || !hs_client_any_intro_points_usable(service_pk,
                                                         desc)) {
    log_info(LD_REND, "Unable to randomly select an introduction point "
             "for service %s because descriptor %s. We can't connect.",
             safe_str_client(onion_address),
             (desc) ? "doesn't have any usable intro points"
                    : "is missing (assuming v3 onion address)");
    goto end;
  }

  enc_data = &desc->encrypted_data;
  usable_ips = smartlist_new();
  smartlist_add_all(usable_ips, enc_data->intro_points);
  while (smartlist_len(usable_ips) != 0) {
    int idx;
    const hs_desc_intro_point_t *ip;

    /* Pick a random intro point and immediately remove it from the usable
     * list so we don't pick it again if we have to iterate more. */
    idx = crypto_rand_int(smartlist_len(usable_ips));
    ip = smartlist_get(usable_ips, idx);
    smartlist_del(usable_ips, idx);

    /* We need to make sure we have a usable intro points which is in a good
     * state in our cache. */
    if (!intro_point_is_usable(service_pk, ip)) {
      continue;
    }

    /* Generate an extend info object from the intro point object. */
    ei = desc_intro_point_to_extend_info(ip);
    if (ei == NULL) {
      /* We can get here for instance if the intro point is a private address
       * and we aren't allowed to extend to those. */
      log_info(LD_REND, "Unable to select introduction point with auth key %s "
               "for service %s, because we could not extend to it.",
               safe_str_client(ed25519_fmt(&ip->auth_key_cert->signed_key)),
               safe_str_client(onion_address));
      continue;
    }

    /* Test the pick against ExcludeNodes. */
    if (routerset_contains_extendinfo(options->ExcludeNodes, ei)) {
      /* If this pick is in the ExcludeNodes list, we keep its reference so if
       * we ever end up not being able to pick anything else and StrictNodes is
       * unset, we'll use it. */
      if (ei_excluded) {
        /* If something was already here free it. After the loop is gone we
         * will examine the last excluded intro point, and that's fine since
         * that's random anyway */
        extend_info_free(ei_excluded);
      }
      ei_excluded = ei;
      continue;
    }

    /* Good pick! Let's go with this. */
    goto end;
  }

  /* Reaching this point means a couple of things. Either we can't use any of
   * the intro point listed because the IP address can't be extended to or it
   * is listed in the ExcludeNodes list. In the later case, if StrictNodes is
   * set, we are forced to not use anything. */
  ei = ei_excluded;
  if (options->StrictNodes) {
    log_warn(LD_REND, "Every introduction point for service %s is in the "
             "ExcludeNodes set and StrictNodes is set. We can't connect.",
             safe_str_client(onion_address));
    extend_info_free(ei);
    ei = NULL;
  } else {
    log_fn(LOG_PROTOCOL_WARN, LD_REND, "Every introduction point for service "
           "%s is unusable or we can't extend to it. We can't connect.",
           safe_str_client(onion_address));
  }

 end:
  smartlist_free(usable_ips);
  memwipe(onion_address, 0, sizeof(onion_address));
  return ei;
}

/* For this introduction circuit, we'll look at if we have any usable
 * introduction point left for this service. If so, we'll use the circuit to
 * re-extend to a new intro point. Else, we'll close the circuit and its
 * corresponding rendezvous circuit. Return 0 if we are re-extending else -1
 * if we are closing the circuits.
 *
 * This is called when getting an INTRODUCE_ACK cell with a NACK. */
static int
close_or_reextend_intro_circ(origin_circuit_t *intro_circ)
{
  int ret = -1;
  const hs_descriptor_t *desc;
  origin_circuit_t *rend_circ;

  tor_assert(intro_circ);

  desc = hs_cache_lookup_as_client(&intro_circ->hs_ident->identity_pk);
  if (BUG(desc == NULL)) {
    /* We can't continue without a descriptor. */
    goto close;
  }
  /* We still have the descriptor, great! Let's try to see if we can
   * re-extend by looking up if there are any usable intro points. */
  if (!hs_client_any_intro_points_usable(&intro_circ->hs_ident->identity_pk,
                                         desc)) {
    goto close;
  }
  /* Try to re-extend now. */
  if (hs_client_reextend_intro_circuit(intro_circ) < 0) {
    goto close;
  }
  /* Success on re-extending. Don't return an error. */
  ret = 0;
  goto end;

 close:
  /* Change the intro circuit purpose before so we don't report an intro point
   * failure again triggering an extra descriptor fetch. The circuit can
   * already be closed on failure to re-extend. */
  if (!TO_CIRCUIT(intro_circ)->marked_for_close) {
    circuit_change_purpose(TO_CIRCUIT(intro_circ),
                           CIRCUIT_PURPOSE_C_INTRODUCE_ACKED);
    circuit_mark_for_close(TO_CIRCUIT(intro_circ), END_CIRC_REASON_FINISHED);
  }
  /* Close the related rendezvous circuit. */
  rend_circ = hs_circuitmap_get_rend_circ_client_side(
                                     intro_circ->hs_ident->rendezvous_cookie);
  /* The rendezvous circuit might have collapsed while the INTRODUCE_ACK was
   * inflight so we can't expect one every time. */
  if (rend_circ) {
    circuit_mark_for_close(TO_CIRCUIT(rend_circ), END_CIRC_REASON_FINISHED);
  }

 end:
  return ret;
}

/* Called when we get an INTRODUCE_ACK success status code. Do the appropriate
 * actions for the rendezvous point and finally close intro_circ. */
static void
handle_introduce_ack_success(origin_circuit_t *intro_circ)
{
  origin_circuit_t *rend_circ = NULL;

  tor_assert(intro_circ);

  log_info(LD_REND, "Received INTRODUCE_ACK ack! Informing rendezvous");

  /* Get the rendezvous circuit for this rendezvous cookie. */
  uint8_t *rendezvous_cookie = intro_circ->hs_ident->rendezvous_cookie;
  rend_circ =
  hs_circuitmap_get_established_rend_circ_client_side(rendezvous_cookie);
  if (rend_circ == NULL) {
    log_warn(LD_REND, "Can't find any rendezvous circuit. Stopping");
    goto end;
  }

  assert_circ_anonymity_ok(rend_circ, get_options());

  /* It is possible to get a RENDEZVOUS2 cell before the INTRODUCE_ACK which
   * means that the circuit will be joined and already transmitting data. In
   * that case, simply skip the purpose change and close the intro circuit
   * like it should be. */
  if (TO_CIRCUIT(rend_circ)->purpose == CIRCUIT_PURPOSE_C_REND_JOINED) {
    goto end;
  }
  circuit_change_purpose(TO_CIRCUIT(rend_circ),
                         CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED);
  /* Set timestamp_dirty, because circuit_expire_building expects it to
   * specify when a circuit entered the
   * CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED state. */
  TO_CIRCUIT(rend_circ)->timestamp_dirty = time(NULL);

 end:
  /* We don't need the intro circuit anymore. It did what it had to do! */
  circuit_change_purpose(TO_CIRCUIT(intro_circ),
                         CIRCUIT_PURPOSE_C_INTRODUCE_ACKED);
  circuit_mark_for_close(TO_CIRCUIT(intro_circ), END_CIRC_REASON_FINISHED);

  /* XXX: Close pending intro circuits we might have in parallel. */
  return;
}

/* Called when we get an INTRODUCE_ACK failure status code. Depending on our
 * failure cache status, either close the circuit or re-extend to a new
 * introduction point. */
static void
handle_introduce_ack_bad(origin_circuit_t *circ, int status)
{
  tor_assert(circ);

  log_info(LD_REND, "Received INTRODUCE_ACK nack by %s. Reason: %u",
      safe_str_client(extend_info_describe(circ->build_state->chosen_exit)),
      status);

  /* It's a NAK. The introduction point didn't relay our request. */
  circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_C_INTRODUCING);

  /* Note down this failure in the intro point failure cache. Depending on how
   * many times we've tried this intro point, close it or reextend. */
  hs_cache_client_intro_state_note(&circ->hs_ident->identity_pk,
                                   &circ->hs_ident->intro_auth_pk,
                                   INTRO_POINT_FAILURE_GENERIC);
}

/* Called when we get an INTRODUCE_ACK on the intro circuit circ. The encoded
 * cell is in payload of length payload_len. Return 0 on success else a
 * negative value. The circuit is either close or reuse to re-extend to a new
 * introduction point. */
static int
handle_introduce_ack(origin_circuit_t *circ, const uint8_t *payload,
                     size_t payload_len)
{
  int status, ret = -1;

  tor_assert(circ);
  tor_assert(circ->build_state);
  tor_assert(circ->build_state->chosen_exit);
  assert_circ_anonymity_ok(circ, get_options());
  tor_assert(payload);

  status = hs_cell_parse_introduce_ack(payload, payload_len);
  switch (status) {
  case HS_CELL_INTRO_ACK_SUCCESS:
    ret = 0;
    handle_introduce_ack_success(circ);
    goto end;
  case HS_CELL_INTRO_ACK_FAILURE:
  case HS_CELL_INTRO_ACK_BADFMT:
  case HS_CELL_INTRO_ACK_NORELAY:
    handle_introduce_ack_bad(circ, status);
    /* We are going to see if we have to close the circuits (IP and RP) or we
     * can re-extend to a new intro point. */
    ret = close_or_reextend_intro_circ(circ);
    break;
  default:
    log_info(LD_PROTOCOL, "Unknown INTRODUCE_ACK status code %u from %s",
        status,
        safe_str_client(extend_info_describe(circ->build_state->chosen_exit)));
    break;
  }

 end:
  return ret;
}

/* Called when we get a RENDEZVOUS2 cell on the rendezvous circuit circ. The
 * encoded cell is in payload of length payload_len. Return 0 on success or a
 * negative value on error. On error, the circuit is marked for close. */
STATIC int
handle_rendezvous2(origin_circuit_t *circ, const uint8_t *payload,
                   size_t payload_len)
{
  int ret = -1;
  curve25519_public_key_t server_pk;
  uint8_t auth_mac[DIGEST256_LEN] = {0};
  uint8_t handshake_info[CURVE25519_PUBKEY_LEN + sizeof(auth_mac)] = {0};
  hs_ntor_rend_cell_keys_t keys;
  const hs_ident_circuit_t *ident;

  tor_assert(circ);
  tor_assert(payload);

  /* Make things easier. */
  ident = circ->hs_ident;
  tor_assert(ident);

  if (hs_cell_parse_rendezvous2(payload, payload_len, handshake_info,
                                sizeof(handshake_info)) < 0) {
    goto err;
  }
  /* Get from the handshake info the SERVER_PK and AUTH_MAC. */
  memcpy(&server_pk, handshake_info, CURVE25519_PUBKEY_LEN);
  memcpy(auth_mac, handshake_info + CURVE25519_PUBKEY_LEN, sizeof(auth_mac));

  /* Generate the handshake info. */
  if (hs_ntor_client_get_rendezvous1_keys(&ident->intro_auth_pk,
                                          &ident->rendezvous_client_kp,
                                          &ident->intro_enc_pk, &server_pk,
                                          &keys) < 0) {
    log_info(LD_REND, "Unable to compute the rendezvous keys.");
    goto err;
  }

  /* Critical check, make sure that the MAC matches what we got with what we
   * computed just above. */
  if (!hs_ntor_client_rendezvous2_mac_is_good(&keys, auth_mac)) {
    log_info(LD_REND, "Invalid MAC in RENDEZVOUS2. Rejecting cell.");
    goto err;
  }

  /* Setup the e2e encryption on the circuit and finalize its state. */
  if (hs_circuit_setup_e2e_rend_circ(circ, keys.ntor_key_seed,
                                     sizeof(keys.ntor_key_seed), 0) < 0) {
    log_info(LD_REND, "Unable to setup the e2e encryption.");
    goto err;
  }
  /* Success. Hidden service connection finalized! */
  ret = 0;
  goto end;

 err:
  circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
 end:
  memwipe(&keys, 0, sizeof(keys));
  return ret;
}

/* Return true iff the client can fetch a descriptor for this service public
 * identity key and status_out if not NULL is untouched. If the client can
 * _not_ fetch the descriptor and if status_out is not NULL, it is set with
 * the fetch status code. */
static unsigned int
can_client_refetch_desc(const ed25519_public_key_t *identity_pk,
                        hs_client_fetch_status_t *status_out)
{
  hs_client_fetch_status_t status;

  tor_assert(identity_pk);

  /* Are we configured to fetch descriptors? */
  if (!get_options()->FetchHidServDescriptors) {
    log_warn(LD_REND, "We received an onion address for a hidden service "
                      "descriptor but we are configured to not fetch.");
    status = HS_CLIENT_FETCH_NOT_ALLOWED;
    goto cannot;
  }

  /* Without a live consensus we can't do any client actions. It is needed to
   * compute the hashring for a service. */
  if (!networkstatus_get_live_consensus(approx_time())) {
    log_info(LD_REND, "Can't fetch descriptor for service %s because we "
                      "are missing a live consensus. Stalling connection.",
             safe_str_client(ed25519_fmt(identity_pk)));
    status = HS_CLIENT_FETCH_MISSING_INFO;
    goto cannot;
  }

  if (!router_have_minimum_dir_info()) {
    log_info(LD_REND, "Can't fetch descriptor for service %s because we "
                      "dont have enough descriptors. Stalling connection.",
             safe_str_client(ed25519_fmt(identity_pk)));
    status = HS_CLIENT_FETCH_MISSING_INFO;
    goto cannot;
  }

  /* Check if fetching a desc for this HS is useful to us right now */
  {
    const hs_descriptor_t *cached_desc = NULL;
    cached_desc = hs_cache_lookup_as_client(identity_pk);
    if (cached_desc && hs_client_any_intro_points_usable(identity_pk,
                                                         cached_desc)) {
      log_info(LD_GENERAL, "We would fetch a v3 hidden service descriptor "
                           "but we already have a usable descriptor.");
      status = HS_CLIENT_FETCH_HAVE_DESC;
      goto cannot;
    }
  }

  /* Don't try to refetch while we have a pending request for it. */
  if (directory_request_is_pending(identity_pk)) {
    log_info(LD_REND, "Already a pending directory request. Waiting on it.");
    status = HS_CLIENT_FETCH_PENDING;
    goto cannot;
  }

  /* Yes, client can fetch! */
  return 1;
 cannot:
  if (status_out) {
    *status_out = status;
  }
  return 0;
}

/* Return the client auth in the map using the service identity public key.
 * Return NULL if it does not exist in the map. */
static hs_client_service_authorization_t *
find_client_auth(const ed25519_public_key_t *service_identity_pk)
{
  /* If the map is not allocated, we can assume that we do not have any client
   * auth information. */
  if (!client_auths) {
    return NULL;
  }
  return digest256map_get(client_auths, service_identity_pk->pubkey);
}

/* ========== */
/* Public API */
/* ========== */

/** A circuit just finished connecting to a hidden service that the stream
 *  <b>conn</b> has been waiting for. Let the HS subsystem know about this. */
void
hs_client_note_connection_attempt_succeeded(const edge_connection_t *conn)
{
  tor_assert(connection_edge_is_rendezvous_stream(conn));

  if (BUG(conn->rend_data && conn->hs_ident)) {
    log_warn(LD_BUG, "Stream had both rend_data and hs_ident..."
             "Prioritizing hs_ident");
  }

  if (conn->hs_ident) { /* It's v3: pass it to the prop224 handler */
    note_connection_attempt_succeeded(conn->hs_ident);
    return;
  } else if (conn->rend_data) { /* It's v2: pass it to the legacy handler */
    rend_client_note_connection_attempt_ended(conn->rend_data);
    return;
  }
}

/* With the given encoded descriptor in desc_str and the service key in
 * service_identity_pk, decode the descriptor and set the desc pointer with a
 * newly allocated descriptor object.
 *
 * Return 0 on success else a negative value and desc is set to NULL. */
int
hs_client_decode_descriptor(const char *desc_str,
                            const ed25519_public_key_t *service_identity_pk,
                            hs_descriptor_t **desc)
{
  int ret;
  uint8_t subcredential[DIGEST256_LEN];
  ed25519_public_key_t blinded_pubkey;
  hs_client_service_authorization_t *client_auth = NULL;
  curve25519_secret_key_t *client_auht_sk = NULL;

  tor_assert(desc_str);
  tor_assert(service_identity_pk);
  tor_assert(desc);

  /* Check if we have a client authorization for this service in the map. */
  client_auth = find_client_auth(service_identity_pk);
  if (client_auth) {
    client_auht_sk = &client_auth->enc_seckey;
  }

  /* Create subcredential for this HS so that we can decrypt */
  {
    uint64_t current_time_period = hs_get_time_period_num(0);
    hs_build_blinded_pubkey(service_identity_pk, NULL, 0, current_time_period,
                            &blinded_pubkey);
    hs_get_subcredential(service_identity_pk, &blinded_pubkey, subcredential);
  }

  /* Parse descriptor */
  ret = hs_desc_decode_descriptor(desc_str, subcredential,
                                  client_auht_sk, desc);
  memwipe(subcredential, 0, sizeof(subcredential));
  if (ret < 0) {
    goto err;
  }

  /* Make sure the descriptor signing key cross certifies with the computed
   * blinded key. Without this validation, anyone knowing the subcredential
   * and onion address can forge a descriptor. */
  tor_cert_t *cert = (*desc)->plaintext_data.signing_key_cert;
  if (tor_cert_checksig(cert,
                        &blinded_pubkey, approx_time()) < 0) {
    log_warn(LD_GENERAL, "Descriptor signing key certificate signature "
             "doesn't validate with computed blinded key: %s",
             tor_cert_describe_signature_status(cert));
    goto err;
  }

  return 0;
 err:
  return -1;
}

/* Return true iff there are at least one usable intro point in the service
 * descriptor desc. */
int
hs_client_any_intro_points_usable(const ed25519_public_key_t *service_pk,
                                  const hs_descriptor_t *desc)
{
  tor_assert(service_pk);
  tor_assert(desc);

  SMARTLIST_FOREACH_BEGIN(desc->encrypted_data.intro_points,
                          const hs_desc_intro_point_t *, ip) {
    if (intro_point_is_usable(service_pk, ip)) {
      goto usable;
    }
  } SMARTLIST_FOREACH_END(ip);

  return 0;
 usable:
  return 1;
}

/** Launch a connection to a hidden service directory to fetch a hidden
 * service descriptor using <b>identity_pk</b> to get the necessary keys.
 *
 * A hs_client_fetch_status_t code is returned. */
int
hs_client_refetch_hsdesc(const ed25519_public_key_t *identity_pk)
{
  hs_client_fetch_status_t status;

  tor_assert(identity_pk);

  if (!can_client_refetch_desc(identity_pk, &status)) {
    return status;
  }

  /* Try to fetch the desc and if we encounter an unrecoverable error, mark
   * the desc as unavailable for now. */
  status = fetch_v3_desc(identity_pk);
  if (fetch_status_should_close_socks(status)) {
    close_all_socks_conns_waiting_for_desc(identity_pk, status,
                                           END_STREAM_REASON_RESOLVEFAILED);
    /* Remove HSDir fetch attempts so that we can retry later if the user
     * wants us to regardless of if we closed any connections. */
    purge_hid_serv_request(identity_pk);
  }
  return status;
}

/* This is called when we are trying to attach an AP connection to these
 * hidden service circuits from connection_ap_handshake_attach_circuit().
 * Return 0 on success, -1 for a transient error that is actions were
 * triggered to recover or -2 for a permenent error where both circuits will
 * marked for close.
 *
 * The following supports every hidden service version. */
int
hs_client_send_introduce1(origin_circuit_t *intro_circ,
                          origin_circuit_t *rend_circ)
{
  return (intro_circ->hs_ident) ? send_introduce1(intro_circ, rend_circ) :
                                  rend_client_send_introduction(intro_circ,
                                                                rend_circ);
}

/* Called when the client circuit circ has been established. It can be either
 * an introduction or rendezvous circuit. This function handles all hidden
 * service versions. */
void
hs_client_circuit_has_opened(origin_circuit_t *circ)
{
  tor_assert(circ);

  /* Handle both version. v2 uses rend_data and v3 uses the hs circuit
   * identifier hs_ident. Can't be both. */
  switch (TO_CIRCUIT(circ)->purpose) {
  case CIRCUIT_PURPOSE_C_INTRODUCING:
    if (circ->hs_ident) {
      client_intro_circ_has_opened(circ);
    } else {
      rend_client_introcirc_has_opened(circ);
    }
    break;
  case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
    if (circ->hs_ident) {
      client_rendezvous_circ_has_opened(circ);
    } else {
      rend_client_rendcirc_has_opened(circ);
    }
    break;
  default:
    tor_assert_nonfatal_unreached();
  }
}

/* Called when we receive a RENDEZVOUS_ESTABLISHED cell. Change the state of
 * the circuit to CIRCUIT_PURPOSE_C_REND_READY. Return 0 on success else a
 * negative value and the circuit marked for close. */
int
hs_client_receive_rendezvous_acked(origin_circuit_t *circ,
                                   const uint8_t *payload, size_t payload_len)
{
  tor_assert(circ);
  tor_assert(payload);

  (void) payload_len;

  if (TO_CIRCUIT(circ)->purpose != CIRCUIT_PURPOSE_C_ESTABLISH_REND) {
    log_warn(LD_PROTOCOL, "Got a RENDEZVOUS_ESTABLISHED but we were not "
                          "expecting one. Closing circuit.");
    goto err;
  }

  log_info(LD_REND, "Received an RENDEZVOUS_ESTABLISHED. This circuit is "
                    "now ready for rendezvous.");
  circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_C_REND_READY);

  /* Set timestamp_dirty, because circuit_expire_building expects it to
   * specify when a circuit entered the _C_REND_READY state. */
  TO_CIRCUIT(circ)->timestamp_dirty = time(NULL);

  /* From a path bias point of view, this circuit is now successfully used.
   * Waiting any longer opens us up to attacks from malicious hidden services.
   * They could induce the client to attempt to connect to their hidden
   * service and never reply to the client's rend requests */
  pathbias_mark_use_success(circ);

  /* If we already have the introduction circuit built, make sure we send
   * the INTRODUCE cell _now_ */
  connection_ap_attach_pending(1);

  return 0;
 err:
  circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
  return -1;
}

#define client_service_authorization_free(auth)                      \
  FREE_AND_NULL(hs_client_service_authorization_t,                   \
                client_service_authorization_free_, (auth))

static void
client_service_authorization_free_(hs_client_service_authorization_t *auth)
{
  if (auth) {
    memwipe(auth, 0, sizeof(*auth));
  }
  tor_free(auth);
}

/** Helper for digest256map_free. */
static void
client_service_authorization_free_void(void *auth)
{
  client_service_authorization_free_(auth);
}

static void
client_service_authorization_free_all(void)
{
  if (!client_auths) {
    return;
  }
  digest256map_free(client_auths, client_service_authorization_free_void);
}

/* Check if the auth key file name is valid or not. Return 1 if valid,
 * otherwise return 0. */
STATIC int
auth_key_filename_is_valid(const char *filename)
{
  int ret = 1;
  const char *valid_extension = ".auth_private";

  tor_assert(filename);

  /* The length of the filename must be greater than the length of the
   * extension and the valid extension must be at the end of filename. */
  if (!strcmpend(filename, valid_extension) &&
      strlen(filename) != strlen(valid_extension)) {
    ret = 1;
  } else {
    ret = 0;
  }

  return ret;
}

STATIC hs_client_service_authorization_t *
parse_auth_file_content(const char *client_key_str)
{
  char *onion_address = NULL;
  char *auth_type = NULL;
  char *key_type = NULL;
  char *seckey_b32 = NULL;
  hs_client_service_authorization_t *auth = NULL;
  smartlist_t *fields = smartlist_new();

  tor_assert(client_key_str);

  smartlist_split_string(fields, client_key_str, ":",
                         SPLIT_SKIP_SPACE, 0);
  /* Wrong number of fields. */
  if (smartlist_len(fields) != 4) {
    goto err;
  }

  onion_address = smartlist_get(fields, 0);
  auth_type = smartlist_get(fields, 1);
  key_type = smartlist_get(fields, 2);
  seckey_b32 = smartlist_get(fields, 3);

  /* Currently, the only supported auth type is "descriptor" and the only
   * supported key type is "x25519". */
  if (strcmp(auth_type, "descriptor") || strcmp(key_type, "x25519")) {
    goto err;
  }

  if (strlen(seckey_b32) != BASE32_NOPAD_LEN(CURVE25519_PUBKEY_LEN)) {
    log_warn(LD_REND, "Client authorization encoded base32 private key "
                      "length is invalid: %s", seckey_b32);
    goto err;
  }

  auth = tor_malloc_zero(sizeof(hs_client_service_authorization_t));
  if (base32_decode((char *) auth->enc_seckey.secret_key,
                    sizeof(auth->enc_seckey.secret_key),
                    seckey_b32, strlen(seckey_b32)) < 0) {
    goto err;
  }
  strncpy(auth->onion_address, onion_address, HS_SERVICE_ADDR_LEN_BASE32);

  /* Success. */
  goto done;

 err:
  client_service_authorization_free(auth);
 done:
  /* It is also a good idea to wipe the private key. */
  if (seckey_b32) {
    memwipe(seckey_b32, 0, strlen(seckey_b32));
  }
  tor_assert(fields);
  SMARTLIST_FOREACH(fields, char *, s, tor_free(s));
  smartlist_free(fields);
  return auth;
}

/* From a set of <b>options</b>, setup every client authorization detail
 * found. Return 0 on success or -1 on failure. If <b>validate_only</b>
 * is set, parse, warn and return as normal, but don't actually change
 * the configuration. */
int
hs_config_client_authorization(const or_options_t *options,
                               int validate_only)
{
  int ret = -1;
  digest256map_t *auths = digest256map_new();
  char *key_dir = NULL;
  smartlist_t *file_list = NULL;
  char *client_key_str = NULL;
  char *client_key_file_path = NULL;

  tor_assert(options);

  /* There is no client auth configured. We can just silently ignore this
   * function. */
  if (!options->ClientOnionAuthDir) {
    ret = 0;
    goto end;
  }

  key_dir = tor_strdup(options->ClientOnionAuthDir);

  /* Make sure the directory exists and is private enough. */
  if (check_private_dir(key_dir, 0, options->User) < 0) {
    goto end;
  }

  file_list = tor_listdir(key_dir);
  if (file_list == NULL) {
    log_warn(LD_REND, "Client authorization key directory %s can't be listed.",
             key_dir);
    goto end;
  }

  SMARTLIST_FOREACH_BEGIN(file_list, char *, filename) {

    hs_client_service_authorization_t *auth = NULL;
    ed25519_public_key_t identity_pk;
    log_info(LD_REND, "Loading a client authorization key file %s...",
             filename);

    if (!auth_key_filename_is_valid(filename)) {
      log_notice(LD_REND, "Client authorization unrecognized filename %s. "
                          "File must end in .auth_private. Ignoring.",
                 filename);
      continue;
    }

    /* Create a full path for a file. */
    client_key_file_path = hs_path_from_filename(key_dir, filename);
    client_key_str = read_file_to_str(client_key_file_path, 0, NULL);
    /* Free the file path immediately after using it. */
    tor_free(client_key_file_path);

    /* If we cannot read the file, continue with the next file. */
    if (!client_key_str) {
      log_warn(LD_REND, "The file %s cannot be read.", filename);
      continue;
    }

    auth = parse_auth_file_content(client_key_str);
    /* Free immediately after using it. */
    tor_free(client_key_str);

    if (auth) {
      /* Parse the onion address to get an identity public key and use it
       * as a key of global map in the future. */
      if (hs_parse_address(auth->onion_address, &identity_pk,
                           NULL, NULL) < 0) {
        log_warn(LD_REND, "The onion address \"%s\" is invalid in "
                          "file %s", filename, auth->onion_address);
        client_service_authorization_free(auth);
        continue;
      }

      if (digest256map_get(auths, identity_pk.pubkey)) {
        log_warn(LD_REND, "Duplicate authorization for the same hidden "
                          "service address %s.",
                 safe_str_client_opts(options, auth->onion_address));
        client_service_authorization_free(auth);
        goto end;
      }

      digest256map_set(auths, identity_pk.pubkey, auth);
      log_info(LD_REND, "Loaded a client authorization key file %s.",
               filename);
    }
  } SMARTLIST_FOREACH_END(filename);

  /* Success. */
  ret = 0;

 end:
  tor_free(key_dir);
  tor_free(client_key_str);
  tor_free(client_key_file_path);
  if (file_list) {
    SMARTLIST_FOREACH(file_list, char *, s, tor_free(s));
    smartlist_free(file_list);
  }

  if (!validate_only && ret == 0) {
    client_service_authorization_free_all();
    client_auths = auths;
  } else {
    digest256map_free(auths, client_service_authorization_free_void);
  }

  return ret;
}

/* This is called when a descriptor has arrived following a fetch request and
 * has been stored in the client cache. Every entry connection that matches
 * the service identity key in the ident will get attached to the hidden
 * service circuit. */
void
hs_client_desc_has_arrived(const hs_ident_dir_conn_t *ident)
{
  time_t now = time(NULL);
  smartlist_t *conns = NULL;

  tor_assert(ident);

  conns = connection_list_by_type_state(CONN_TYPE_AP,
                                        AP_CONN_STATE_RENDDESC_WAIT);
  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, base_conn) {
    const hs_descriptor_t *desc;
    entry_connection_t *entry_conn = TO_ENTRY_CONN(base_conn);
    const edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(entry_conn);

    /* Only consider the entry connections that matches the service for which
     * we just fetched its descriptor. */
    if (!edge_conn->hs_ident ||
        !ed25519_pubkey_eq(&ident->identity_pk,
                           &edge_conn->hs_ident->identity_pk)) {
      continue;
    }
    assert_connection_ok(base_conn, now);

    /* We were just called because we stored the descriptor for this service
     * so not finding a descriptor means we have a bigger problem. */
    desc = hs_cache_lookup_as_client(&ident->identity_pk);
    if (BUG(desc == NULL)) {
      goto end;
    }

    if (!hs_client_any_intro_points_usable(&ident->identity_pk, desc)) {
      log_info(LD_REND, "Hidden service descriptor is unusable. "
                        "Closing streams.");
      connection_mark_unattached_ap(entry_conn,
                                    END_STREAM_REASON_RESOLVEFAILED);
      /* We are unable to use the descriptor so remove the directory request
       * from the cache so the next connection can try again. */
      note_connection_attempt_succeeded(edge_conn->hs_ident);
      continue;
    }

    log_info(LD_REND, "Descriptor has arrived. Launching circuits.");

    /* Mark connection as waiting for a circuit since we do have a usable
     * descriptor now. */
    mark_conn_as_waiting_for_circuit(base_conn, now);
  } SMARTLIST_FOREACH_END(base_conn);

 end:
  /* We don't have ownership of the objects in this list. */
  smartlist_free(conns);
}

/* Return a newly allocated extend_info_t for a randomly chosen introduction
 * point for the given edge connection identifier ident. Return NULL if we
 * can't pick any usable introduction points. */
extend_info_t *
hs_client_get_random_intro_from_edge(const edge_connection_t *edge_conn)
{
  tor_assert(edge_conn);

  return (edge_conn->hs_ident) ?
    client_get_random_intro(&edge_conn->hs_ident->identity_pk) :
    rend_client_get_random_intro(edge_conn->rend_data);
}

/* Called when get an INTRODUCE_ACK cell on the introduction circuit circ.
 * Return 0 on success else a negative value is returned. The circuit will be
 * closed or reuse to extend again to another intro point. */
int
hs_client_receive_introduce_ack(origin_circuit_t *circ,
                                const uint8_t *payload, size_t payload_len)
{
  int ret = -1;

  tor_assert(circ);
  tor_assert(payload);

  if (TO_CIRCUIT(circ)->purpose != CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT) {
    log_warn(LD_PROTOCOL, "Unexpected INTRODUCE_ACK on circuit %u.",
             (unsigned int) TO_CIRCUIT(circ)->n_circ_id);
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
    goto end;
  }

  ret = (circ->hs_ident) ? handle_introduce_ack(circ, payload, payload_len) :
                           rend_client_introduction_acked(circ, payload,
                                                          payload_len);
  /* For path bias: This circuit was used successfully. NACK or ACK counts. */
  pathbias_mark_use_success(circ);

 end:
  return ret;
}

/* Called when get a RENDEZVOUS2 cell on the rendezvous circuit circ.  Return
 * 0 on success else a negative value is returned. The circuit will be closed
 * on error. */
int
hs_client_receive_rendezvous2(origin_circuit_t *circ,
                              const uint8_t *payload, size_t payload_len)
{
  int ret = -1;

  tor_assert(circ);
  tor_assert(payload);

  /* Circuit can possibly be in both state because we could receive a
   * RENDEZVOUS2 cell before the INTRODUCE_ACK has been received. */
  if (TO_CIRCUIT(circ)->purpose != CIRCUIT_PURPOSE_C_REND_READY &&
      TO_CIRCUIT(circ)->purpose != CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED) {
    log_warn(LD_PROTOCOL, "Unexpected RENDEZVOUS2 cell on circuit %u. "
                          "Closing circuit.",
             (unsigned int) TO_CIRCUIT(circ)->n_circ_id);
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
    goto end;
  }

  log_info(LD_REND, "Got RENDEZVOUS2 cell from hidden service on circuit %u.",
           TO_CIRCUIT(circ)->n_circ_id);

  ret = (circ->hs_ident) ? handle_rendezvous2(circ, payload, payload_len) :
                           rend_client_receive_rendezvous(circ, payload,
                                                          payload_len);
 end:
  return ret;
}

/* Extend the introduction circuit circ to another valid introduction point
 * for the hidden service it is trying to connect to, or mark it and launch a
 * new circuit if we can't extend it.  Return 0 on success or possible
 * success. Return -1 and mark the introduction circuit for close on permanent
 * failure.
 *
 * On failure, the caller is responsible for marking the associated rendezvous
 * circuit for close. */
int
hs_client_reextend_intro_circuit(origin_circuit_t *circ)
{
  int ret = -1;
  extend_info_t *ei;

  tor_assert(circ);

  ei = (circ->hs_ident) ?
    client_get_random_intro(&circ->hs_ident->identity_pk) :
    rend_client_get_random_intro(circ->rend_data);
  if (ei == NULL) {
    log_warn(LD_REND, "No usable introduction points left. Closing.");
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_INTERNAL);
    goto end;
  }

  if (circ->remaining_relay_early_cells) {
    log_info(LD_REND, "Re-extending circ %u, this time to %s.",
             (unsigned int) TO_CIRCUIT(circ)->n_circ_id,
             safe_str_client(extend_info_describe(ei)));
    ret = circuit_extend_to_new_exit(circ, ei);
    if (ret == 0) {
      /* We were able to extend so update the timestamp so we avoid expiring
       * this circuit too early. The intro circuit is short live so the
       * linkability issue is minimized, we just need the circuit to hold a
       * bit longer so we can introduce. */
      TO_CIRCUIT(circ)->timestamp_dirty = time(NULL);
    }
  } else {
    log_info(LD_REND, "Closing intro circ %u (out of RELAY_EARLY cells).",
             (unsigned int) TO_CIRCUIT(circ)->n_circ_id);
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_FINISHED);
    /* connection_ap_handshake_attach_circuit will launch a new intro circ. */
    ret = 0;
  }

 end:
  extend_info_free(ei);
  return ret;
}

/* Close all client introduction circuits related to the given descriptor.
 * This is called with a descriptor that is about to get replaced in the
 * client cache.
 *
 * Even though the introduction point might be exactly the same, we'll rebuild
 * them if needed but the odds are very low that an existing matching
 * introduction circuit exists at that stage. */
void
hs_client_close_intro_circuits_from_desc(const hs_descriptor_t *desc)
{
  origin_circuit_t *ocirc = NULL;

  tor_assert(desc);

  /* We iterate over all client intro circuits because they aren't kept in the
   * HS circuitmap. That is probably something we want to do one day. */
  while ((ocirc = circuit_get_next_intro_circ(ocirc, true))) {
    if (ocirc->hs_ident == NULL) {
      /* Not a v3 circuit, ignore it. */
      continue;
    }

    /* Does it match any IP in the given descriptor? If not, ignore. */
    if (find_desc_intro_point_by_ident(ocirc->hs_ident, desc) == NULL) {
      continue;
    }

    /* We have a match. Close the circuit as consider it expired. */
    circuit_mark_for_close(TO_CIRCUIT(ocirc), END_CIRC_REASON_FINISHED);
  }
}

/* Release all the storage held by the client subsystem. */
void
hs_client_free_all(void)
{
  /* Purge the hidden service request cache. */
  hs_purge_last_hid_serv_requests();
  client_service_authorization_free_all();
}

/* Purge all potentially remotely-detectable state held in the hidden
 * service client code. Called on SIGNAL NEWNYM. */
void
hs_client_purge_state(void)
{
  /* v2 subsystem. */
  rend_client_purge_state();

  /* Cancel all descriptor fetches. Do this first so once done we are sure
   * that our descriptor cache won't modified. */
  cancel_descriptor_fetches();
  /* Purge the introduction point state cache. */
  hs_cache_client_intro_state_purge();
  /* Purge the descriptor cache. */
  hs_cache_purge_as_client();
  /* Purge the last hidden service request cache. */
  hs_purge_last_hid_serv_requests();

  log_info(LD_REND, "Hidden service client state has been purged.");
}

/* Called when our directory information has changed. */
void
hs_client_dir_info_changed(void)
{
  /* We have possibly reached the minimum directory information or new
   * consensus so retry all pending SOCKS connection in
   * AP_CONN_STATE_RENDDESC_WAIT state in order to fetch the descriptor. */
  retry_all_socks_conn_waiting_for_desc();
}

#ifdef TOR_UNIT_TESTS

STATIC digest256map_t *
get_hs_client_auths_map(void)
{
  return client_auths;
}

#endif /* defined(TOR_UNIT_TESTS) */
