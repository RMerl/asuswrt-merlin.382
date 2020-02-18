/* Copyright (c) 2017-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file hs_cell.h
 * \brief Header file containing cell data for the whole HS subsytem.
 **/

#ifndef TOR_HS_CELL_H
#define TOR_HS_CELL_H

#include "core/or/or.h"
#include "feature/hs/hs_service.h"

/* An INTRODUCE1 cell requires at least this amount of bytes (see section
 * 3.2.2 of the specification). Below this value, the cell must be padded. */
#define HS_CELL_INTRODUCE1_MIN_SIZE 246

/* This data structure contains data that we need to build an INTRODUCE1 cell
 * used by the INTRODUCE1 build function. */
typedef struct hs_cell_introduce1_data_t {
  /* Is this a legacy introduction point? */
  unsigned int is_legacy : 1;
  /* (Legacy only) The encryption key for a legacy intro point. Only set if
   * is_legacy is true. */
  const crypto_pk_t *legacy_key;
  /* Introduction point authentication public key. */
  const ed25519_public_key_t *auth_pk;
  /* Introduction point encryption public key. */
  const curve25519_public_key_t *enc_pk;
  /* Subcredentials of the service. */
  const uint8_t *subcredential;
  /* Onion public key for the ntor handshake. */
  const curve25519_public_key_t *onion_pk;
  /* Rendezvous cookie. */
  const uint8_t *rendezvous_cookie;
  /* Public key put before the encrypted data (CLIENT_PK). */
  const curve25519_keypair_t *client_kp;
  /* Rendezvous point link specifiers. */
  smartlist_t *link_specifiers;
} hs_cell_introduce1_data_t;

/* This data structure contains data that we need to parse an INTRODUCE2 cell
 * which is used by the INTRODUCE2 cell parsing function. On a successful
 * parsing, the onion_pk and rendezvous_cookie will be populated with the
 * computed key material from the cell data. This structure is only used during
 * INTRO2 parsing and discarded after that. */
typedef struct hs_cell_introduce2_data_t {
  /*** Immutable Section: Set on structure init. ***/

  /* Introduction point authentication public key. Pointer owned by the
     introduction point object through which we received the INTRO2 cell. */
  const ed25519_public_key_t *auth_pk;
  /* Introduction point encryption keypair for the ntor handshake. Pointer
     owned by the introduction point object through which we received the
     INTRO2 cell*/
  const curve25519_keypair_t *enc_kp;
  /* Subcredentials of the service. Pointer owned by the descriptor that owns
     the introduction point through which we received the INTRO2 cell. */
  const uint8_t *subcredential;
  /* Payload of the received encoded cell. */
  const uint8_t *payload;
  /* Size of the payload of the received encoded cell. */
  size_t payload_len;

  /*** Mutable Section: Set upon parsing INTRODUCE2 cell. ***/

  /* Onion public key computed using the INTRODUCE2 encrypted section. */
  curve25519_public_key_t onion_pk;
  /* Rendezvous cookie taken from the INTRODUCE2 encrypted section. */
  uint8_t rendezvous_cookie[REND_COOKIE_LEN];
  /* Client public key from the INTRODUCE2 encrypted section. */
  curve25519_public_key_t client_pk;
  /* Link specifiers of the rendezvous point. Contains link_specifier_t. */
  smartlist_t *link_specifiers;
  /* Replay cache of the introduction point. */
  replaycache_t *replay_cache;
} hs_cell_introduce2_data_t;

/* Build cell API. */
ssize_t hs_cell_build_establish_intro(const char *circ_nonce,
                                      const hs_service_intro_point_t *ip,
                                      uint8_t *cell_out);
ssize_t hs_cell_build_rendezvous1(const uint8_t *rendezvous_cookie,
                                  size_t rendezvous_cookie_len,
                                  const uint8_t *rendezvous_handshake_info,
                                  size_t rendezvous_handshake_info_len,
                                  uint8_t *cell_out);
ssize_t hs_cell_build_introduce1(const hs_cell_introduce1_data_t *data,
                                 uint8_t *cell_out);
ssize_t hs_cell_build_establish_rendezvous(const uint8_t *rendezvous_cookie,
                                           uint8_t *cell_out);

/* Parse cell API. */
ssize_t hs_cell_parse_intro_established(const uint8_t *payload,
                                        size_t payload_len);
ssize_t hs_cell_parse_introduce2(hs_cell_introduce2_data_t *data,
                                 const origin_circuit_t *circ,
                                 const hs_service_t *service);
int hs_cell_parse_introduce_ack(const uint8_t *payload, size_t payload_len);
int hs_cell_parse_rendezvous2(const uint8_t *payload, size_t payload_len,
                              uint8_t *handshake_info,
                              size_t handshake_info_len);

/* Util API. */
void hs_cell_introduce1_data_clear(hs_cell_introduce1_data_t *data);

#endif /* !defined(TOR_HS_CELL_H) */

