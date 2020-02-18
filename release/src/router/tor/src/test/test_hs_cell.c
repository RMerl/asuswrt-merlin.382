/* Copyright (c) 2017-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file test_hs_cell.c
 * \brief Test hidden service cell functionality.
 */

#define HS_INTROPOINT_PRIVATE
#define HS_SERVICE_PRIVATE

#include "test/test.h"
#include "test/test_helpers.h"
#include "test/log_test_helpers.h"

#include "lib/crypt_ops/crypto_ed25519.h"
#include "lib/crypt_ops/crypto_rand.h"
#include "feature/hs/hs_cell.h"
#include "feature/hs/hs_intropoint.h"
#include "feature/hs/hs_service.h"

/* Trunnel. */
#include "trunnel/hs/cell_establish_intro.h"

/** We simulate the creation of an outgoing ESTABLISH_INTRO cell, and then we
 *  parse it from the receiver side. */
static void
test_gen_establish_intro_cell(void *arg)
{
  (void) arg;
  ssize_t ret;
  char circ_nonce[DIGEST_LEN] = {0};
  uint8_t buf[RELAY_PAYLOAD_SIZE];
  trn_cell_establish_intro_t *cell_in = NULL;

  crypto_rand(circ_nonce, sizeof(circ_nonce));

  /* Create outgoing ESTABLISH_INTRO cell and extract its payload so that we
     attempt to parse it. */
  {
    /* We only need the auth key pair here. */
    hs_service_intro_point_t *ip = service_intro_point_new(NULL, 0, 0);
    /* Auth key pair is generated in the constructor so we are all set for
     * using this IP object. */
    ret = hs_cell_build_establish_intro(circ_nonce, ip, buf);
    service_intro_point_free(ip);
    tt_u64_op(ret, OP_GT, 0);
  }

  /* Check the contents of the cell */
  {
    /* First byte is the auth key type: make sure its correct */
    tt_int_op(buf[0], OP_EQ, TRUNNEL_HS_INTRO_AUTH_KEY_TYPE_ED25519);
    /* Next two bytes is auth key len */
    tt_int_op(ntohs(get_uint16(buf+1)), OP_EQ, ED25519_PUBKEY_LEN);
    /* Skip to the number of extensions: no extensions */
    tt_int_op(buf[35], OP_EQ, 0);
    /* Skip to the sig len. Make sure it's the size of an ed25519 sig */
    tt_int_op(ntohs(get_uint16(buf+35+1+32)), OP_EQ, ED25519_SIG_LEN);
  }

  /* Parse it as the receiver */
  {
    ret = trn_cell_establish_intro_parse(&cell_in, buf, sizeof(buf));
    tt_u64_op(ret, OP_GT, 0);

    ret = verify_establish_intro_cell(cell_in,
                                      (const uint8_t *) circ_nonce,
                                      sizeof(circ_nonce));
    tt_u64_op(ret, OP_EQ, 0);
  }

 done:
  trn_cell_establish_intro_free(cell_in);
}

/* Mocked ed25519_sign_prefixed() function that always fails :) */
static int
mock_ed25519_sign_prefixed(ed25519_signature_t *signature_out,
                           const uint8_t *msg, size_t msg_len,
                           const char *prefix_str,
                           const ed25519_keypair_t *keypair) {
  (void) signature_out;
  (void) msg;
  (void) msg_len;
  (void) prefix_str;
  (void) keypair;
  return -1;
}

/** We simulate a failure to create an ESTABLISH_INTRO cell */
static void
test_gen_establish_intro_cell_bad(void *arg)
{
  (void) arg;
  ssize_t cell_len = 0;
  trn_cell_establish_intro_t *cell = NULL;
  char circ_nonce[DIGEST_LEN] = {0};
  hs_service_intro_point_t *ip = NULL;

  MOCK(ed25519_sign_prefixed, mock_ed25519_sign_prefixed);

  crypto_rand(circ_nonce, sizeof(circ_nonce));

  setup_full_capture_of_logs(LOG_WARN);
  /* Easiest way to make that function fail is to mock the
     ed25519_sign_prefixed() function and make it fail. */
  cell = trn_cell_establish_intro_new();
  tt_assert(cell);
  ip = service_intro_point_new(NULL, 0, 0);
  cell_len = hs_cell_build_establish_intro(circ_nonce, ip, NULL);
  service_intro_point_free(ip);
  expect_log_msg_containing("Unable to make signature for "
                            "ESTABLISH_INTRO cell.");
  teardown_capture_of_logs();
  tt_i64_op(cell_len, OP_EQ, -1);

 done:
  trn_cell_establish_intro_free(cell);
  UNMOCK(ed25519_sign_prefixed);
}

struct testcase_t hs_cell_tests[] = {
  { "gen_establish_intro_cell", test_gen_establish_intro_cell, TT_FORK,
    NULL, NULL },
  { "gen_establish_intro_cell_bad", test_gen_establish_intro_cell_bad, TT_FORK,
    NULL, NULL },

  END_OF_TESTCASES
};

