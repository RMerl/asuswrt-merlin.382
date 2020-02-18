/* Copyright (c) 2016-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file hs_intropoint.c
 * \brief Implement next generation introductions point functionality
 **/

#define HS_INTROPOINT_PRIVATE

#include "core/or/or.h"
#include "app/config/config.h"
#include "core/or/circuitlist.h"
#include "core/or/circuituse.h"
#include "core/or/relay.h"
#include "feature/rend/rendmid.h"
#include "feature/stats/rephist.h"
#include "lib/crypt_ops/crypto_format.h"

/* Trunnel */
#include "trunnel/ed25519_cert.h"
#include "trunnel/hs/cell_common.h"
#include "trunnel/hs/cell_establish_intro.h"
#include "trunnel/hs/cell_introduce1.h"

#include "feature/hs/hs_circuitmap.h"
#include "feature/hs/hs_descriptor.h"
#include "feature/hs/hs_intropoint.h"
#include "feature/hs/hs_common.h"

#include "core/or/or_circuit_st.h"

/** Extract the authentication key from an ESTABLISH_INTRO or INTRODUCE1 using
 * the given <b>cell_type</b> from <b>cell</b> and place it in
 * <b>auth_key_out</b>. */
STATIC void
get_auth_key_from_cell(ed25519_public_key_t *auth_key_out,
                       unsigned int cell_type, const void *cell)
{
  size_t auth_key_len;
  const uint8_t *key_array;

  tor_assert(auth_key_out);
  tor_assert(cell);

  switch (cell_type) {
  case RELAY_COMMAND_ESTABLISH_INTRO:
  {
    const trn_cell_establish_intro_t *c_cell = cell;
    key_array = trn_cell_establish_intro_getconstarray_auth_key(c_cell);
    auth_key_len = trn_cell_establish_intro_getlen_auth_key(c_cell);
    break;
  }
  case RELAY_COMMAND_INTRODUCE1:
  {
    const trn_cell_introduce1_t *c_cell = cell;
    key_array = trn_cell_introduce1_getconstarray_auth_key(cell);
    auth_key_len = trn_cell_introduce1_getlen_auth_key(c_cell);
    break;
  }
  default:
    /* Getting here is really bad as it means we got a unknown cell type from
     * this file where every call has an hardcoded value. */
    tor_assert_unreached(); /* LCOV_EXCL_LINE */
  }
  tor_assert(key_array);
  tor_assert(auth_key_len == sizeof(auth_key_out->pubkey));
  memcpy(auth_key_out->pubkey, key_array, auth_key_len);
}

/** We received an ESTABLISH_INTRO <b>cell</b>. Verify its signature and MAC,
 *  given <b>circuit_key_material</b>. Return 0 on success else -1 on error. */
STATIC int
verify_establish_intro_cell(const trn_cell_establish_intro_t *cell,
                            const uint8_t *circuit_key_material,
                            size_t circuit_key_material_len)
{
  /* We only reach this function if the first byte of the cell is 0x02 which
   * means that auth_key_type is of ed25519 type, hence this check should
   * always pass. See hs_intro_received_establish_intro().  */
  if (BUG(cell->auth_key_type != TRUNNEL_HS_INTRO_AUTH_KEY_TYPE_ED25519)) {
    return -1;
  }

  /* Make sure the auth key length is of the right size for this type. For
   * EXTRA safety, we check both the size of the array and the length which
   * must be the same. Safety first!*/
  if (trn_cell_establish_intro_getlen_auth_key(cell) != ED25519_PUBKEY_LEN ||
      trn_cell_establish_intro_get_auth_key_len(cell) != ED25519_PUBKEY_LEN) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "ESTABLISH_INTRO auth key length is invalid");
    return -1;
  }

  const uint8_t *msg = cell->start_cell;

  /* Verify the sig */
  {
    ed25519_signature_t sig_struct;
    const uint8_t *sig_array =
      trn_cell_establish_intro_getconstarray_sig(cell);

    /* Make sure the signature length is of the right size. For EXTRA safety,
     * we check both the size of the array and the length which must be the
     * same. Safety first!*/
    if (trn_cell_establish_intro_getlen_sig(cell) != sizeof(sig_struct.sig) ||
        trn_cell_establish_intro_get_sig_len(cell) != sizeof(sig_struct.sig)) {
      log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
             "ESTABLISH_INTRO sig len is invalid");
      return -1;
    }
    /* We are now sure that sig_len is of the right size. */
    memcpy(sig_struct.sig, sig_array, cell->sig_len);

    ed25519_public_key_t auth_key;
    get_auth_key_from_cell(&auth_key, RELAY_COMMAND_ESTABLISH_INTRO, cell);

    const size_t sig_msg_len = cell->end_sig_fields - msg;
    int sig_mismatch = ed25519_checksig_prefixed(&sig_struct,
                                                 msg, sig_msg_len,
                                                 ESTABLISH_INTRO_SIG_PREFIX,
                                                 &auth_key);
    if (sig_mismatch) {
      log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
             "ESTABLISH_INTRO signature not as expected");
      return -1;
    }
  }

  /* Verify the MAC */
  {
    const size_t auth_msg_len = cell->end_mac_fields - msg;
    uint8_t mac[DIGEST256_LEN];
    crypto_mac_sha3_256(mac, sizeof(mac),
                        circuit_key_material, circuit_key_material_len,
                        msg, auth_msg_len);
    if (tor_memneq(mac, cell->handshake_mac, sizeof(mac))) {
      log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
             "ESTABLISH_INTRO handshake_auth not as expected");
      return -1;
    }
  }

  return 0;
}

/* Send an INTRO_ESTABLISHED cell to <b>circ</b>. */
MOCK_IMPL(int,
hs_intro_send_intro_established_cell,(or_circuit_t *circ))
{
  int ret;
  uint8_t *encoded_cell = NULL;
  ssize_t encoded_len, result_len;
  trn_cell_intro_established_t *cell;
  trn_cell_extension_t *ext;

  tor_assert(circ);

  /* Build the cell payload. */
  cell = trn_cell_intro_established_new();
  ext = trn_cell_extension_new();
  trn_cell_extension_set_num(ext, 0);
  trn_cell_intro_established_set_extensions(cell, ext);
  /* Encode the cell to binary format. */
  encoded_len = trn_cell_intro_established_encoded_len(cell);
  tor_assert(encoded_len > 0);
  encoded_cell = tor_malloc_zero(encoded_len);
  result_len = trn_cell_intro_established_encode(encoded_cell, encoded_len,
                                                cell);
  tor_assert(encoded_len == result_len);

  ret = relay_send_command_from_edge(0, TO_CIRCUIT(circ),
                                     RELAY_COMMAND_INTRO_ESTABLISHED,
                                     (char *) encoded_cell, encoded_len,
                                     NULL);
  /* On failure, the above function will close the circuit. */
  trn_cell_intro_established_free(cell);
  tor_free(encoded_cell);
  return ret;
}

/** We received an ESTABLISH_INTRO <b>parsed_cell</b> on <b>circ</b>. It's
 *  well-formed and passed our verifications. Perform appropriate actions to
 *  establish an intro point. */
static int
handle_verified_establish_intro_cell(or_circuit_t *circ,
                               const trn_cell_establish_intro_t *parsed_cell)
{
  /* Get the auth key of this intro point */
  ed25519_public_key_t auth_key;
  get_auth_key_from_cell(&auth_key, RELAY_COMMAND_ESTABLISH_INTRO,
                         parsed_cell);

  /* Then notify the hidden service that the intro point is established by
     sending an INTRO_ESTABLISHED cell */
  if (hs_intro_send_intro_established_cell(circ)) {
    log_warn(LD_PROTOCOL, "Couldn't send INTRO_ESTABLISHED cell.");
    return -1;
  }

  /* Associate intro point auth key with this circuit. */
  hs_circuitmap_register_intro_circ_v3_relay_side(circ, &auth_key);
  /* Repurpose this circuit into an intro circuit. */
  circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_INTRO_POINT);

  return 0;
}

/** We just received an ESTABLISH_INTRO cell in <b>circ</b> with payload in
 *  <b>request</b>. Handle it by making <b>circ</b> an intro circuit. Return 0
 *  if everything went well, or -1 if there were errors. */
static int
handle_establish_intro(or_circuit_t *circ, const uint8_t *request,
                       size_t request_len)
{
  int cell_ok, retval = -1;
  trn_cell_establish_intro_t *parsed_cell = NULL;

  tor_assert(circ);
  tor_assert(request);

  log_info(LD_REND, "Received an ESTABLISH_INTRO request on circuit %" PRIu32,
           circ->p_circ_id);

  /* Check that the circuit is in shape to become an intro point */
  if (!hs_intro_circuit_is_suitable_for_establish_intro(circ)) {
    goto err;
  }

  /* Parse the cell */
  ssize_t parsing_result = trn_cell_establish_intro_parse(&parsed_cell,
                                                         request, request_len);
  if (parsing_result < 0) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Rejecting %s ESTABLISH_INTRO cell.",
           parsing_result == -1 ? "invalid" : "truncated");
    goto err;
  }

  cell_ok = verify_establish_intro_cell(parsed_cell,
                                        (uint8_t *) circ->rend_circ_nonce,
                                        sizeof(circ->rend_circ_nonce));
  if (cell_ok < 0) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Failed to verify ESTABLISH_INTRO cell.");
    goto err;
  }

  /* This cell is legit. Take the appropriate actions. */
  cell_ok = handle_verified_establish_intro_cell(circ, parsed_cell);
  if (cell_ok < 0) {
    goto err;
  }

  /* We are done! */
  retval = 0;
  goto done;

 err:
  /* When sending the intro establish ack, on error the circuit can be marked
   * as closed so avoid a double close. */
  if (!TO_CIRCUIT(circ)->marked_for_close) {
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
  }

 done:
  trn_cell_establish_intro_free(parsed_cell);
  return retval;
}

/* Return True if circuit is suitable for being an intro circuit. */
static int
circuit_is_suitable_intro_point(const or_circuit_t *circ,
                                const char *log_cell_type_str)
{
  tor_assert(circ);
  tor_assert(log_cell_type_str);

  /* Basic circuit state sanity checks. */
  if (circ->base_.purpose != CIRCUIT_PURPOSE_OR) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Rejecting %s on non-OR circuit.", log_cell_type_str);
    return 0;
  }

  if (circ->base_.n_chan) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Rejecting %s on non-edge circuit.", log_cell_type_str);
    return 0;
  }

  /* Suitable. */
  return 1;
}

/* Return True if circuit is suitable for being service-side intro circuit. */
int
hs_intro_circuit_is_suitable_for_establish_intro(const or_circuit_t *circ)
{
  return circuit_is_suitable_intro_point(circ, "ESTABLISH_INTRO");
}

/* We just received an ESTABLISH_INTRO cell in <b>circ</b>. Figure out of it's
 * a legacy or a next gen cell, and pass it to the appropriate handler. */
int
hs_intro_received_establish_intro(or_circuit_t *circ, const uint8_t *request,
                            size_t request_len)
{
  tor_assert(circ);
  tor_assert(request);

  if (request_len == 0) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL, "Empty ESTABLISH_INTRO cell.");
    goto err;
  }

  /* Using the first byte of the cell, figure out the version of
   * ESTABLISH_INTRO and pass it to the appropriate cell handler */
  const uint8_t first_byte = request[0];
  switch (first_byte) {
    case TRUNNEL_HS_INTRO_AUTH_KEY_TYPE_LEGACY0:
    case TRUNNEL_HS_INTRO_AUTH_KEY_TYPE_LEGACY1:
      return rend_mid_establish_intro_legacy(circ, request, request_len);
    case TRUNNEL_HS_INTRO_AUTH_KEY_TYPE_ED25519:
      return handle_establish_intro(circ, request, request_len);
    default:
      log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
             "Unrecognized AUTH_KEY_TYPE %u.", first_byte);
      goto err;
  }

 err:
  circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
  return -1;
}

/* Send an INTRODUCE_ACK cell onto the circuit <b>circ</b> with the status
 * value in <b>status</b>. Depending on the status, it can be ACK or a NACK.
 * Return 0 on success else a negative value on error which will close the
 * circuit. */
static int
send_introduce_ack_cell(or_circuit_t *circ, uint16_t status)
{
  int ret = -1;
  uint8_t *encoded_cell = NULL;
  ssize_t encoded_len, result_len;
  trn_cell_introduce_ack_t *cell;
  trn_cell_extension_t *ext;

  tor_assert(circ);

  /* Setup the INTRODUCE_ACK cell. We have no extensions so the N_EXTENSIONS
   * field is set to 0 by default with a new object. */
  cell = trn_cell_introduce_ack_new();
  ret = trn_cell_introduce_ack_set_status(cell, status);
  /* We have no cell extensions in an INTRODUCE_ACK cell. */
  ext = trn_cell_extension_new();
  trn_cell_extension_set_num(ext, 0);
  trn_cell_introduce_ack_set_extensions(cell, ext);
  /* A wrong status is a very bad code flow error as this value is controlled
   * by the code in this file and not an external input. This means we use a
   * code that is not known by the trunnel ABI. */
  tor_assert(ret == 0);
  /* Encode the payload. We should never fail to get the encoded length. */
  encoded_len = trn_cell_introduce_ack_encoded_len(cell);
  tor_assert(encoded_len > 0);
  encoded_cell = tor_malloc_zero(encoded_len);
  result_len = trn_cell_introduce_ack_encode(encoded_cell, encoded_len, cell);
  tor_assert(encoded_len == result_len);

  ret = relay_send_command_from_edge(CONTROL_CELL_ID, TO_CIRCUIT(circ),
                                     RELAY_COMMAND_INTRODUCE_ACK,
                                     (char *) encoded_cell, encoded_len,
                                     NULL);
  /* On failure, the above function will close the circuit. */
  trn_cell_introduce_ack_free(cell);
  tor_free(encoded_cell);
  return ret;
}

/* Validate a parsed INTRODUCE1 <b>cell</b>. Return 0 if valid or else a
 * negative value for an invalid cell that should be NACKed. */
STATIC int
validate_introduce1_parsed_cell(const trn_cell_introduce1_t *cell)
{
  size_t legacy_key_id_len;
  const uint8_t *legacy_key_id;

  tor_assert(cell);

  /* This code path SHOULD NEVER be reached if the cell is a legacy type so
   * safety net here. The legacy ID must be zeroes in this case. */
  legacy_key_id_len = trn_cell_introduce1_getlen_legacy_key_id(cell);
  legacy_key_id = trn_cell_introduce1_getconstarray_legacy_key_id(cell);
  if (BUG(!tor_mem_is_zero((char *) legacy_key_id, legacy_key_id_len))) {
    goto invalid;
  }

  /* The auth key of an INTRODUCE1 should be of type ed25519 thus leading to a
   * known fixed length as well. */
  if (trn_cell_introduce1_get_auth_key_type(cell) !=
      TRUNNEL_HS_INTRO_AUTH_KEY_TYPE_ED25519) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Rejecting invalid INTRODUCE1 cell auth key type. "
           "Responding with NACK.");
    goto invalid;
  }
  if (trn_cell_introduce1_get_auth_key_len(cell) != ED25519_PUBKEY_LEN ||
      trn_cell_introduce1_getlen_auth_key(cell) != ED25519_PUBKEY_LEN) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Rejecting invalid INTRODUCE1 cell auth key length. "
           "Responding with NACK.");
    goto invalid;
  }
  if (trn_cell_introduce1_getlen_encrypted(cell) == 0) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Rejecting invalid INTRODUCE1 cell encrypted length. "
           "Responding with NACK.");
    goto invalid;
  }

  return 0;
 invalid:
  return -1;
}

/* We just received a non legacy INTRODUCE1 cell on <b>client_circ</b> with
 * the payload in <b>request</b> of size <b>request_len</b>. Return 0 if
 * everything went well, or -1 if an error occurred. This function is in charge
 * of sending back an INTRODUCE_ACK cell and will close client_circ on error.
 */
STATIC int
handle_introduce1(or_circuit_t *client_circ, const uint8_t *request,
                  size_t request_len)
{
  int ret = -1;
  or_circuit_t *service_circ;
  trn_cell_introduce1_t *parsed_cell;
  uint16_t status = TRUNNEL_HS_INTRO_ACK_STATUS_SUCCESS;

  tor_assert(client_circ);
  tor_assert(request);

  /* Parse cell. Note that we can only parse the non encrypted section for
   * which we'll use the authentication key to find the service introduction
   * circuit and relay the cell on it. */
  ssize_t cell_size = trn_cell_introduce1_parse(&parsed_cell, request,
                                               request_len);
  if (cell_size < 0) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Rejecting %s INTRODUCE1 cell. Responding with NACK.",
           cell_size == -1 ? "invalid" : "truncated");
    /* Inform client that the INTRODUCE1 has a bad format. */
    status = TRUNNEL_HS_INTRO_ACK_STATUS_BAD_FORMAT;
    goto send_ack;
  }

  /* Once parsed validate the cell format. */
  if (validate_introduce1_parsed_cell(parsed_cell) < 0) {
    /* Inform client that the INTRODUCE1 has bad format. */
    status = TRUNNEL_HS_INTRO_ACK_STATUS_BAD_FORMAT;
    goto send_ack;
  }

  /* Find introduction circuit through our circuit map. */
  {
    ed25519_public_key_t auth_key;
    get_auth_key_from_cell(&auth_key, RELAY_COMMAND_INTRODUCE1, parsed_cell);
    service_circ = hs_circuitmap_get_intro_circ_v3_relay_side(&auth_key);
    if (service_circ == NULL) {
      char b64_key[ED25519_BASE64_LEN + 1];
      ed25519_public_to_base64(b64_key, &auth_key);
      log_info(LD_REND, "No intro circuit found for INTRODUCE1 cell "
                        "with auth key %s from circuit %" PRIu32 ". "
                        "Responding with NACK.",
               safe_str(b64_key), client_circ->p_circ_id);
      /* Inform the client that we don't know the requested service ID. */
      status = TRUNNEL_HS_INTRO_ACK_STATUS_UNKNOWN_ID;
      goto send_ack;
    }
  }

  /* Relay the cell to the service on its intro circuit with an INTRODUCE2
   * cell which is the same exact payload. */
  if (relay_send_command_from_edge(CONTROL_CELL_ID, TO_CIRCUIT(service_circ),
                                   RELAY_COMMAND_INTRODUCE2,
                                   (char *) request, request_len, NULL)) {
    log_warn(LD_PROTOCOL, "Unable to send INTRODUCE2 cell to the service.");
    /* Inform the client that we can't relay the cell. Use the unknown ID
     * status code since it means that we do not know the service. */
    status = TRUNNEL_HS_INTRO_ACK_STATUS_UNKNOWN_ID;
    goto send_ack;
  }

  /* Success! Send an INTRODUCE_ACK success status onto the client circuit. */
  status = TRUNNEL_HS_INTRO_ACK_STATUS_SUCCESS;
  ret = 0;

 send_ack:
  /* Send INTRODUCE_ACK or INTRODUCE_NACK to client */
  if (send_introduce_ack_cell(client_circ, status) < 0) {
    log_warn(LD_PROTOCOL, "Unable to send an INTRODUCE ACK status %d "
                          "to client.", status);
    /* Circuit has been closed on failure of transmission. */
    goto done;
  }
 done:
  trn_cell_introduce1_free(parsed_cell);
  return ret;
}

/* Identify if the encoded cell we just received is a legacy one or not. The
 * <b>request</b> should be at least DIGEST_LEN bytes long. */
STATIC int
introduce1_cell_is_legacy(const uint8_t *request)
{
  tor_assert(request);

  /* If the first 20 bytes of the cell (DIGEST_LEN) are NOT zeroes, it
   * indicates a legacy cell (v2). */
  if (!tor_mem_is_zero((const char *) request, DIGEST_LEN)) {
    /* Legacy cell. */
    return 1;
  }
  /* Not a legacy cell. */
  return 0;
}

/* Return true iff the circuit <b>circ</b> is suitable for receiving an
 * INTRODUCE1 cell. */
STATIC int
circuit_is_suitable_for_introduce1(const or_circuit_t *circ)
{
  tor_assert(circ);

  /* Is this circuit an intro point circuit? */
  if (!circuit_is_suitable_intro_point(circ, "INTRODUCE1")) {
    return 0;
  }

  if (circ->already_received_introduce1) {
    log_fn(LOG_PROTOCOL_WARN, LD_REND,
           "Blocking multiple introductions on the same circuit. "
           "Someone might be trying to attack a hidden service through "
           "this relay.");
    return 0;
  }

  return 1;
}

/* We just received an INTRODUCE1 cell on <b>circ</b>. Figure out which type
 * it is and pass it to the appropriate handler. Return 0 on success else a
 * negative value and the circuit is closed. */
int
hs_intro_received_introduce1(or_circuit_t *circ, const uint8_t *request,
                             size_t request_len)
{
  int ret;

  tor_assert(circ);
  tor_assert(request);

  /* A cell that can't hold a DIGEST_LEN is invalid as we need to check if
   * it's a legacy cell or not using the first DIGEST_LEN bytes. */
  if (request_len < DIGEST_LEN) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL, "Invalid INTRODUCE1 cell length.");
    goto err;
  }

  /* Make sure we have a circuit that can have an INTRODUCE1 cell on it. */
  if (!circuit_is_suitable_for_introduce1(circ)) {
    /* We do not send a NACK because the circuit is not suitable for any kind
     * of response or transmission as it's a violation of the protocol. */
    goto err;
  }
  /* Mark the circuit that we got this cell. None are allowed after this as a
   * DoS mitigation since one circuit with one client can hammer a service. */
  circ->already_received_introduce1 = 1;

  /* We are sure here to have at least DIGEST_LEN bytes. */
  if (introduce1_cell_is_legacy(request)) {
    /* Handle a legacy cell. */
    ret = rend_mid_introduce_legacy(circ, request, request_len);
  } else {
    /* Handle a non legacy cell. */
    ret = handle_introduce1(circ, request, request_len);
  }
  return ret;

 err:
  circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
  return -1;
}

/* Clear memory allocated by the given intropoint object ip (but don't free the
 * object itself). */
void
hs_intropoint_clear(hs_intropoint_t *ip)
{
  if (ip == NULL) {
    return;
  }
  tor_cert_free(ip->auth_key_cert);
  SMARTLIST_FOREACH(ip->link_specifiers, hs_desc_link_specifier_t *, ls,
                    hs_desc_link_specifier_free(ls));
  smartlist_free(ip->link_specifiers);
  memset(ip, 0, sizeof(hs_intropoint_t));
}
