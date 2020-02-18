/* Copyright (c) 2014-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"

#define CIRCUITLIST_PRIVATE
#define CIRCUITBUILD_PRIVATE
#define STATEFILE_PRIVATE
#define ENTRYNODES_PRIVATE
#define ROUTERLIST_PRIVATE
#define DIRCLIENT_PRIVATE

#include "core/or/or.h"
#include "test/test.h"

#include "feature/client/bridges.h"
#include "core/or/circuitlist.h"
#include "core/or/circuitbuild.h"
#include "app/config/config.h"
#include "app/config/confparse.h"
#include "lib/crypt_ops/crypto_rand.h"
#include "feature/dircommon/directory.h"
#include "feature/dirclient/dirclient.h"
#include "feature/client/entrynodes.h"
#include "feature/nodelist/nodelist.h"
#include "feature/nodelist/networkstatus.h"
#include "core/or/policies.h"
#include "feature/nodelist/routerlist.h"
#include "feature/nodelist/routerset.h"
#include "app/config/statefile.h"

#include "core/or/cpath_build_state_st.h"
#include "core/or/crypt_path_st.h"
#include "feature/dircommon/dir_connection_st.h"
#include "feature/nodelist/microdesc_st.h"
#include "feature/nodelist/networkstatus_st.h"
#include "feature/nodelist/node_st.h"
#include "core/or/origin_circuit_st.h"
#include "app/config/or_state_st.h"
#include "feature/nodelist/routerinfo_st.h"
#include "feature/nodelist/routerstatus_st.h"

#include "test/test_helpers.h"
#include "test/log_test_helpers.h"

#include "lib/container/bloomfilt.h"
#include "lib/encoding/confline.h"

/* TODO:
 * choose_random_entry() test with state set.
 *
 * parse_state() tests with more than one guards.
 *
 * More tests for set_from_config(): Multiple nodes, use fingerprints,
 *                                   use country codes.
 */

/** Dummy Tor state used in unittests. */
static or_state_t *dummy_state = NULL;
static or_state_t *
get_or_state_replacement(void)
{
  return dummy_state;
}

static networkstatus_t *dummy_consensus = NULL;

static smartlist_t *big_fake_net_nodes = NULL;

static smartlist_t *
bfn_mock_nodelist_get_list(void)
{
  return big_fake_net_nodes;
}

static networkstatus_t *
bfn_mock_networkstatus_get_reasonably_live_consensus(time_t now, int flavor)
{
  (void)now;
  (void)flavor;
  return dummy_consensus;
}

static const node_t *
bfn_mock_node_get_by_id(const char *id)
{
  SMARTLIST_FOREACH(big_fake_net_nodes, node_t *, n,
                    if (fast_memeq(n->identity, id, 20))
                      return n);

  return NULL;
}

/* Helper function to free a test node. */
static void
test_node_free(node_t *n)
{
  tor_free(n->rs);
  tor_free(n->md->onion_curve25519_pkey);
  short_policy_free(n->md->exit_policy);
  tor_free(n->md);
  tor_free(n);
}

/* Unittest cleanup function: Cleanup the fake network. */
static int
big_fake_network_cleanup(const struct testcase_t *testcase, void *ptr)
{
  (void) testcase;
  (void) ptr;

  if (big_fake_net_nodes) {
    SMARTLIST_FOREACH(big_fake_net_nodes, node_t *, n, {
      test_node_free(n);
    });
    smartlist_free(big_fake_net_nodes);
  }

  UNMOCK(nodelist_get_list);
  UNMOCK(node_get_by_id);
  UNMOCK(get_or_state);
  UNMOCK(networkstatus_get_reasonably_live_consensus);
  or_state_free(dummy_state);
  dummy_state = NULL;
  tor_free(dummy_consensus);

  return 1; /* NOP */
}

/* Unittest setup function: Setup a fake network. */
static void *
big_fake_network_setup(const struct testcase_t *testcase)
{
  int i;

  /* These are minimal node_t objects that only contain the aspects of node_t
   * that we need for entrynodes.c. */
  const int N_NODES = 271;

  const char *argument = testcase->setup_data;
  int reasonably_live_consensus = 0;
  if (argument) {
    reasonably_live_consensus = strstr(argument, "reasonably-live") != NULL;
  }

  big_fake_net_nodes = smartlist_new();
  for (i = 0; i < N_NODES; ++i) {
    curve25519_secret_key_t curve25519_secret_key;

    node_t *n = tor_malloc_zero(sizeof(node_t));
    n->md = tor_malloc_zero(sizeof(microdesc_t));

    /* Generate curve25519 key for this node */
    n->md->onion_curve25519_pkey =
      tor_malloc_zero(sizeof(curve25519_public_key_t));
    curve25519_secret_key_generate(&curve25519_secret_key, 0);
    curve25519_public_key_generate(n->md->onion_curve25519_pkey,
                                   &curve25519_secret_key);

    crypto_rand(n->identity, sizeof(n->identity));
    n->rs = tor_malloc_zero(sizeof(routerstatus_t));

    memcpy(n->rs->identity_digest, n->identity, DIGEST_LEN);

    n->is_running = n->is_valid = n->is_fast = n->is_stable = 1;

    /* Note: all these guards have the same address, so you'll need to
     * disable EnforceDistinctSubnets when a restriction is applied. */
    n->rs->addr = 0x04020202;
    n->rs->or_port = 1234;
    n->rs->is_v2_dir = 1;
    n->rs->has_bandwidth = 1;
    n->rs->bandwidth_kb = 30;

    /* Make a random nickname for each node */
    {
      char nickname_binary[8];
      crypto_rand(nickname_binary, sizeof(nickname_binary));
      base32_encode(n->rs->nickname, sizeof(n->rs->nickname),
                    nickname_binary, sizeof(nickname_binary));
    }

    /* Call half of the nodes a possible guard. */
    if (i % 2 == 0) {
      n->is_possible_guard = 1;
      n->rs->guardfraction_percentage = 100;
      n->rs->has_guardfraction = 1;
      n->rs->is_possible_guard = 1;
    }

    /* Make some of these nodes a possible exit */
    if (i % 7 == 0) {
      n->md->exit_policy = parse_short_policy("accept 443");
    }

    smartlist_add(big_fake_net_nodes, n);
  }

  dummy_state = tor_malloc_zero(sizeof(or_state_t));
  dummy_consensus = tor_malloc_zero(sizeof(networkstatus_t));
  if (reasonably_live_consensus) {
    /* Make the dummy consensus valid from 4 hours ago, but expired an hour
     * ago. */
    dummy_consensus->valid_after = approx_time() - 4*3600;
    dummy_consensus->valid_until = approx_time() - 3600;
  } else {
    /* Make the dummy consensus valid for an hour either side of now. */
    dummy_consensus->valid_after = approx_time() - 3600;
    dummy_consensus->valid_until = approx_time() + 3600;
  }

  MOCK(nodelist_get_list, bfn_mock_nodelist_get_list);
  MOCK(node_get_by_id, bfn_mock_node_get_by_id);
  MOCK(get_or_state,
       get_or_state_replacement);
  MOCK(networkstatus_get_reasonably_live_consensus,
       bfn_mock_networkstatus_get_reasonably_live_consensus);
  /* Return anything but NULL (it's interpreted as test fail) */
  return (void*)testcase;
}

static time_t
mock_randomize_time_no_randomization(time_t a, time_t b)
{
  (void) b;
  return a;
}

static or_options_t mocked_options;

static const or_options_t *
mock_get_options(void)
{
  return &mocked_options;
}

#define TEST_IPV4_ADDR "123.45.67.89"
#define TEST_IPV6_ADDR "[1234:5678:90ab:cdef::]"

static void
test_node_preferred_orport(void *arg)
{
  (void)arg;
  tor_addr_t ipv4_addr;
  const uint16_t ipv4_port = 4444;
  tor_addr_t ipv6_addr;
  const uint16_t ipv6_port = 6666;
  routerinfo_t node_ri;
  node_t node;
  tor_addr_port_t ap;

  /* Setup options */
  memset(&mocked_options, 0, sizeof(mocked_options));
  /* We don't test ClientPreferIPv6ORPort here, because it's used in
   * nodelist_set_consensus to setup node.ipv6_preferred, which we set
   * directly. */
  MOCK(get_options, mock_get_options);

  /* Setup IP addresses */
  tor_addr_parse(&ipv4_addr, TEST_IPV4_ADDR);
  tor_addr_parse(&ipv6_addr, TEST_IPV6_ADDR);

  /* Setup node_ri */
  memset(&node_ri, 0, sizeof(node_ri));
  node_ri.addr = tor_addr_to_ipv4h(&ipv4_addr);
  node_ri.or_port = ipv4_port;
  tor_addr_copy(&node_ri.ipv6_addr, &ipv6_addr);
  node_ri.ipv6_orport = ipv6_port;

  /* Setup node */
  memset(&node, 0, sizeof(node));
  node.ri = &node_ri;

  /* Check the preferred address is IPv4 if we're only using IPv4, regardless
   * of whether we prefer it or not */
  mocked_options.ClientUseIPv4 = 1;
  mocked_options.ClientUseIPv6 = 0;
  node.ipv6_preferred = 0;
  node_get_pref_orport(&node, &ap);
  tt_assert(tor_addr_eq(&ap.addr, &ipv4_addr));
  tt_assert(ap.port == ipv4_port);

  node.ipv6_preferred = 1;
  node_get_pref_orport(&node, &ap);
  tt_assert(tor_addr_eq(&ap.addr, &ipv4_addr));
  tt_assert(ap.port == ipv4_port);

  /* Check the preferred address is IPv4 if we're using IPv4 and IPv6, but
   * don't prefer the IPv6 address */
  mocked_options.ClientUseIPv4 = 1;
  mocked_options.ClientUseIPv6 = 1;
  node.ipv6_preferred = 0;
  node_get_pref_orport(&node, &ap);
  tt_assert(tor_addr_eq(&ap.addr, &ipv4_addr));
  tt_assert(ap.port == ipv4_port);

  /* Check the preferred address is IPv6 if we prefer it and
   * ClientUseIPv6 is 1, regardless of ClientUseIPv4 */
  mocked_options.ClientUseIPv4 = 1;
  mocked_options.ClientUseIPv6 = 1;
  node.ipv6_preferred = 1;
  node_get_pref_orport(&node, &ap);
  tt_assert(tor_addr_eq(&ap.addr, &ipv6_addr));
  tt_assert(ap.port == ipv6_port);

  mocked_options.ClientUseIPv4 = 0;
  node_get_pref_orport(&node, &ap);
  tt_assert(tor_addr_eq(&ap.addr, &ipv6_addr));
  tt_assert(ap.port == ipv6_port);

  /* Check the preferred address is IPv6 if we don't prefer it, but
   * ClientUseIPv4 is 0 */
  mocked_options.ClientUseIPv4 = 0;
  mocked_options.ClientUseIPv6 = 1;
  node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(&mocked_options);
  node_get_pref_orport(&node, &ap);
  tt_assert(tor_addr_eq(&ap.addr, &ipv6_addr));
  tt_assert(ap.port == ipv6_port);

 done:
  UNMOCK(get_options);
}

static void
test_entry_guard_describe(void *arg)
{
  (void)arg;
  entry_guard_t g;
  memset(&g, 0, sizeof(g));
  strlcpy(g.nickname, "okefenokee", sizeof(g.nickname));
  memcpy(g.identity, "theforestprimeval---", DIGEST_LEN);

  tt_str_op(entry_guard_describe(&g), OP_EQ,
            "okefenokee ($746865666F726573747072696D6576616C2D2D2D)");

 done:
  ;
}

static void
test_entry_guard_randomize_time(void *arg)
{
  const time_t now = 1479153573;
  const int delay = 86400;
  const int N = 1000;
  (void)arg;

  time_t t;
  int i;
  for (i = 0; i < N; ++i) {
    t = randomize_time(now, delay);
    tt_int_op(t, OP_LE, now);
    tt_int_op(t, OP_GE, now-delay);
  }

  /* now try the corner cases */
  for (i = 0; i < N; ++i) {
    t = randomize_time(100, delay);
    tt_int_op(t, OP_GE, 1);
    tt_int_op(t, OP_LE, 100);

    t = randomize_time(0, delay);
    tt_int_op(t, OP_EQ, 1);
  }

 done:
  ;
}

static void
test_entry_guard_encode_for_state_minimal(void *arg)
{
  (void) arg;
  entry_guard_t *eg = tor_malloc_zero(sizeof(entry_guard_t));

  eg->selection_name = tor_strdup("wubwub");
  memcpy(eg->identity, "plurpyflurpyslurpydo", DIGEST_LEN);
  eg->sampled_on_date = 1479081600;
  eg->confirmed_idx = -1;

  char *s = NULL;
  s = entry_guard_encode_for_state(eg);

  tt_str_op(s, OP_EQ,
            "in=wubwub "
            "rsa_id=706C75727079666C75727079736C75727079646F "
            "sampled_on=2016-11-14T00:00:00 "
            "listed=0");

 done:
  entry_guard_free(eg);
  tor_free(s);
}

static void
test_entry_guard_encode_for_state_maximal(void *arg)
{
  (void) arg;
  entry_guard_t *eg = tor_malloc_zero(sizeof(entry_guard_t));

  strlcpy(eg->nickname, "Fred", sizeof(eg->nickname));
  eg->selection_name = tor_strdup("default");
  memcpy(eg->identity, "plurpyflurpyslurpydo", DIGEST_LEN);
  eg->bridge_addr = tor_malloc_zero(sizeof(tor_addr_port_t));
  tor_addr_from_ipv4h(&eg->bridge_addr->addr, 0x08080404);
  eg->bridge_addr->port = 9999;
  eg->sampled_on_date = 1479081600;
  eg->sampled_by_version = tor_strdup("1.2.3");
  eg->unlisted_since_date = 1479081645;
  eg->currently_listed = 1;
  eg->confirmed_on_date = 1479081690;
  eg->confirmed_idx = 333;
  eg->extra_state_fields = tor_strdup("and the green grass grew all around");

  char *s = NULL;
  s = entry_guard_encode_for_state(eg);

  tt_str_op(s, OP_EQ,
            "in=default "
            "rsa_id=706C75727079666C75727079736C75727079646F "
            "bridge_addr=8.8.4.4:9999 "
            "nickname=Fred "
            "sampled_on=2016-11-14T00:00:00 "
            "sampled_by=1.2.3 "
            "unlisted_since=2016-11-14T00:00:45 "
            "listed=1 "
            "confirmed_on=2016-11-14T00:01:30 "
            "confirmed_idx=333 "
            "and the green grass grew all around");

 done:
  entry_guard_free(eg);
  tor_free(s);
}

static void
test_entry_guard_parse_from_state_minimal(void *arg)
{
  (void)arg;
  char *mem_op_hex_tmp = NULL;
  entry_guard_t *eg = NULL;
  time_t t = approx_time();

  eg = entry_guard_parse_from_state(
                 "in=default_plus "
                 "rsa_id=596f75206d6179206e656564206120686f626279");
  tt_assert(eg);

  tt_str_op(eg->selection_name, OP_EQ, "default_plus");
  test_mem_op_hex(eg->identity, OP_EQ,
                  "596f75206d6179206e656564206120686f626279");
  tt_str_op(eg->nickname, OP_EQ, "$596F75206D6179206E656564206120686F626279");
  tt_ptr_op(eg->bridge_addr, OP_EQ, NULL);
  tt_i64_op(eg->sampled_on_date, OP_GE, t);
  tt_i64_op(eg->sampled_on_date, OP_LE, t+86400);
  tt_i64_op(eg->unlisted_since_date, OP_EQ, 0);
  tt_ptr_op(eg->sampled_by_version, OP_EQ, NULL);
  tt_int_op(eg->currently_listed, OP_EQ, 0);
  tt_i64_op(eg->confirmed_on_date, OP_EQ, 0);
  tt_int_op(eg->confirmed_idx, OP_EQ, -1);

  tt_int_op(eg->last_tried_to_connect, OP_EQ, 0);
  tt_int_op(eg->is_reachable, OP_EQ, GUARD_REACHABLE_MAYBE);

 done:
  entry_guard_free(eg);
  tor_free(mem_op_hex_tmp);
}

static void
test_entry_guard_parse_from_state_maximal(void *arg)
{
  (void)arg;
  char *mem_op_hex_tmp = NULL;
  entry_guard_t *eg = NULL;

  eg = entry_guard_parse_from_state(
            "in=fred "
            "rsa_id=706C75727079666C75727079736C75727079646F "
            "bridge_addr=[1::3]:9999 "
            "nickname=Fred "
            "sampled_on=2016-11-14T00:00:00 "
            "sampled_by=1.2.3 "
            "unlisted_since=2016-11-14T00:00:45 "
            "listed=1 "
            "confirmed_on=2016-11-14T00:01:30 "
            "confirmed_idx=333 "
            "and the green grass grew all around "
            "rsa_id=all,around");
  tt_assert(eg);

  test_mem_op_hex(eg->identity, OP_EQ,
                  "706C75727079666C75727079736C75727079646F");
  tt_str_op(fmt_addr(&eg->bridge_addr->addr), OP_EQ, "1::3");
  tt_int_op(eg->bridge_addr->port, OP_EQ, 9999);
  tt_str_op(eg->nickname, OP_EQ, "Fred");
  tt_i64_op(eg->sampled_on_date, OP_EQ, 1479081600);
  tt_i64_op(eg->unlisted_since_date, OP_EQ, 1479081645);
  tt_str_op(eg->sampled_by_version, OP_EQ, "1.2.3");
  tt_int_op(eg->currently_listed, OP_EQ, 1);
  tt_i64_op(eg->confirmed_on_date, OP_EQ, 1479081690);
  tt_int_op(eg->confirmed_idx, OP_EQ, 333);
  tt_str_op(eg->extra_state_fields, OP_EQ,
            "and the green grass grew all around rsa_id=all,around");

  tt_int_op(eg->last_tried_to_connect, OP_EQ, 0);
  tt_int_op(eg->is_reachable, OP_EQ, GUARD_REACHABLE_MAYBE);

 done:
  entry_guard_free(eg);
  tor_free(mem_op_hex_tmp);
}

static void
test_entry_guard_parse_from_state_failure(void *arg)
{
  (void)arg;
  entry_guard_t *eg = NULL;

  /* no selection */
  eg = entry_guard_parse_from_state(
                 "rsa_id=596f75206d6179206e656564206120686f626270");
  tt_ptr_op(eg, OP_EQ, NULL);

  /* no RSA ID. */
  eg = entry_guard_parse_from_state("in=default nickname=Fred");
  tt_ptr_op(eg, OP_EQ, NULL);

  /* Bad RSA ID: bad character. */
  eg = entry_guard_parse_from_state(
                 "in=default "
                 "rsa_id=596f75206d6179206e656564206120686f62627q");
  tt_ptr_op(eg, OP_EQ, NULL);

  /* Bad RSA ID: too long.*/
  eg = entry_guard_parse_from_state(
                 "in=default "
                 "rsa_id=596f75206d6179206e656564206120686f6262703");
  tt_ptr_op(eg, OP_EQ, NULL);

  /* Bad RSA ID: too short.*/
  eg = entry_guard_parse_from_state(
                 "in=default "
                 "rsa_id=596f75206d6179206e65656420612");
  tt_ptr_op(eg, OP_EQ, NULL);

 done:
  entry_guard_free(eg);
}

static void
test_entry_guard_parse_from_state_partial_failure(void *arg)
{
  (void)arg;
  char *mem_op_hex_tmp = NULL;
  entry_guard_t *eg = NULL;
  time_t t = approx_time();

  eg = entry_guard_parse_from_state(
            "in=default "
            "rsa_id=706C75727079666C75727079736C75727079646F "
            "bridge_addr=1.2.3.3.4:5 "
            "nickname=FredIsANodeWithAStrangeNicknameThatIsTooLong "
            "sampled_on=2016-11-14T00:00:99 "
            "sampled_by=1.2.3 stuff in the middle "
            "unlisted_since=2016-xx-14T00:00:45 "
            "listed=0 "
            "confirmed_on=2016-11-14T00:01:30zz "
            "confirmed_idx=idx "
            "and the green grass grew all around "
            "rsa_id=all,around");
  tt_assert(eg);

  test_mem_op_hex(eg->identity, OP_EQ,
                  "706C75727079666C75727079736C75727079646F");
  tt_str_op(eg->nickname, OP_EQ, "FredIsANodeWithAStrangeNicknameThatIsTooL");
  tt_ptr_op(eg->bridge_addr, OP_EQ, NULL);
  tt_i64_op(eg->sampled_on_date, OP_EQ, t);
  tt_i64_op(eg->unlisted_since_date, OP_EQ, 0);
  tt_str_op(eg->sampled_by_version, OP_EQ, "1.2.3");
  tt_int_op(eg->currently_listed, OP_EQ, 0);
  tt_i64_op(eg->confirmed_on_date, OP_EQ, 0);
  tt_int_op(eg->confirmed_idx, OP_EQ, -1);
  tt_str_op(eg->extra_state_fields, OP_EQ,
            "stuff in the middle and the green grass grew all around "
            "rsa_id=all,around");

  tt_int_op(eg->last_tried_to_connect, OP_EQ, 0);
  tt_int_op(eg->is_reachable, OP_EQ, GUARD_REACHABLE_MAYBE);

 done:
  entry_guard_free(eg);
  tor_free(mem_op_hex_tmp);
}

static int
mock_entry_guard_is_listed(guard_selection_t *gs, const entry_guard_t *guard)
{
  (void)gs;
  (void)guard;
  return 1;
}

static void
test_entry_guard_parse_from_state_full(void *arg)
{
  (void)arg;
  /* Here's a state I made while testing.  The identities and locations for
   * the bridges are redacted. */
  const char STATE[] =
  "Guard in=default rsa_id=214F44BD5B638E8C817D47FF7C97397790BF0345 "
    "nickname=TotallyNinja sampled_on=2016-11-12T19:32:49 "
    "sampled_by=0.3.0.0-alpha-dev "
    "listed=1\n"
  "Guard in=default rsa_id=052900AB0EA3ED54BAB84AE8A99E74E8693CE2B2 "
    "nickname=5OfNovember sampled_on=2016-11-20T04:32:05 "
    "sampled_by=0.3.0.0-alpha-dev "
    "listed=1 confirmed_on=2016-11-22T08:13:28 confirmed_idx=0 "
    "pb_circ_attempts=4.000000 pb_circ_successes=2.000000 "
    "pb_successful_circuits_closed=2.000000\n"
  "Guard in=default rsa_id=7B700C0C207EBD0002E00F499BE265519AC3C25A "
    "nickname=dc6jgk11 sampled_on=2016-11-28T11:50:13 "
    "sampled_by=0.3.0.0-alpha-dev "
    "listed=1 confirmed_on=2016-11-24T08:45:30 confirmed_idx=4 "
    "pb_circ_attempts=5.000000 pb_circ_successes=5.000000 "
    "pb_successful_circuits_closed=5.000000\n"
  "Guard in=wobblesome rsa_id=7B700C0C207EBD0002E00F499BE265519AC3C25A "
    "nickname=dc6jgk11 sampled_on=2016-11-28T11:50:13 "
    "sampled_by=0.3.0.0-alpha-dev "
    "listed=1\n"
  "Guard in=default rsa_id=E9025AD60D86875D5F11548D536CC6AF60F0EF5E "
    "nickname=maibrunn sampled_on=2016-11-25T22:36:38 "
    "sampled_by=0.3.0.0-alpha-dev listed=1\n"
  "Guard in=default rsa_id=DCD30B90BA3A792DA75DC54A327EF353FB84C38E "
    "nickname=Unnamed sampled_on=2016-11-25T14:34:00 "
    "sampled_by=0.3.0.0-alpha-dev listed=1\n"
  "Guard in=bridges rsa_id=8FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF2E "
    "bridge_addr=24.1.1.1:443 sampled_on=2016-11-25T06:44:14 "
    "sampled_by=0.3.0.0-alpha-dev listed=1 "
    "confirmed_on=2016-11-29T10:36:06 confirmed_idx=0 "
    "pb_circ_attempts=8.000000 pb_circ_successes=8.000000 "
    "pb_successful_circuits_closed=13.000000\n"
  "Guard in=bridges rsa_id=5800000000000000000000000000000000000000 "
    "bridge_addr=37.218.246.143:28366 "
    "sampled_on=2016-11-18T15:07:34 sampled_by=0.3.0.0-alpha-dev listed=1\n";

  config_line_t *lines = NULL;
  or_state_t *state = tor_malloc_zero(sizeof(or_state_t));
  int r = config_get_lines(STATE, &lines, 0);
  char *msg = NULL;
  smartlist_t *text = smartlist_new();
  char *joined = NULL;

  // So nodes aren't expired. This is Tue, 13 Dec 2016 09:37:14 GMT
  update_approx_time(1481621834);

  MOCK(entry_guard_is_listed, mock_entry_guard_is_listed);

  dummy_state = state;
  MOCK(get_or_state,
       get_or_state_replacement);

  tt_int_op(r, OP_EQ, 0);
  tt_assert(lines);

  state->Guard = lines;

  /* Try it first without setting the result. */
  r = entry_guards_parse_state(state, 0, &msg);
  tt_int_op(r, OP_EQ, 0);
  guard_selection_t *gs_br =
    get_guard_selection_by_name("bridges", GS_TYPE_BRIDGE, 0);
  tt_ptr_op(gs_br, OP_EQ, NULL);

  r = entry_guards_parse_state(state, 1, &msg);
  tt_int_op(r, OP_EQ, 0);
  gs_br = get_guard_selection_by_name("bridges", GS_TYPE_BRIDGE, 0);
  guard_selection_t *gs_df =
    get_guard_selection_by_name("default", GS_TYPE_NORMAL, 0);
  guard_selection_t *gs_wb =
    get_guard_selection_by_name("wobblesome", GS_TYPE_NORMAL, 0);

  tt_assert(gs_br);
  tt_assert(gs_df);
  tt_assert(gs_wb);

  tt_int_op(smartlist_len(gs_df->sampled_entry_guards), OP_EQ, 5);
  tt_int_op(smartlist_len(gs_br->sampled_entry_guards), OP_EQ, 2);
  tt_int_op(smartlist_len(gs_wb->sampled_entry_guards), OP_EQ, 1);

  /* Try again; make sure it doesn't double-add the guards. */
  r = entry_guards_parse_state(state, 1, &msg);
  tt_int_op(r, OP_EQ, 0);
  gs_br = get_guard_selection_by_name("bridges", GS_TYPE_BRIDGE, 0);
  gs_df = get_guard_selection_by_name("default", GS_TYPE_NORMAL, 0);
  tt_assert(gs_br);
  tt_assert(gs_df);
  tt_int_op(smartlist_len(gs_df->sampled_entry_guards), OP_EQ, 5);
  tt_int_op(smartlist_len(gs_br->sampled_entry_guards), OP_EQ, 2);

  /* Re-encode; it should be the same... almost. */
  {
    /* (Make a guard nonpersistent first) */
    entry_guard_t *g = smartlist_get(gs_df->sampled_entry_guards, 0);
    g->is_persistent = 0;
  }
  config_free_lines(lines);
  lines = state->Guard = NULL; // to prevent double-free.
  entry_guards_update_state(state);
  tt_assert(state->Guard);
  lines = state->Guard;

  config_line_t *ln;
  for (ln = lines; ln; ln = ln->next) {
    smartlist_add_asprintf(text, "%s %s\n",ln->key, ln->value);
  }
  joined = smartlist_join_strings(text, "", 0, NULL);
  tt_str_op(joined, OP_EQ,
  "Guard in=default rsa_id=052900AB0EA3ED54BAB84AE8A99E74E8693CE2B2 "
    "nickname=5OfNovember sampled_on=2016-11-20T04:32:05 "
    "sampled_by=0.3.0.0-alpha-dev "
    "listed=1 confirmed_on=2016-11-22T08:13:28 confirmed_idx=0 "
    "pb_circ_attempts=4.000000 pb_circ_successes=2.000000 "
    "pb_successful_circuits_closed=2.000000\n"
  "Guard in=default rsa_id=7B700C0C207EBD0002E00F499BE265519AC3C25A "
    "nickname=dc6jgk11 sampled_on=2016-11-28T11:50:13 "
    "sampled_by=0.3.0.0-alpha-dev "
    "listed=1 confirmed_on=2016-11-24T08:45:30 confirmed_idx=1 "
    "pb_circ_attempts=5.000000 pb_circ_successes=5.000000 "
    "pb_successful_circuits_closed=5.000000\n"
  "Guard in=default rsa_id=E9025AD60D86875D5F11548D536CC6AF60F0EF5E "
    "nickname=maibrunn sampled_on=2016-11-25T22:36:38 "
    "sampled_by=0.3.0.0-alpha-dev listed=1\n"
  "Guard in=default rsa_id=DCD30B90BA3A792DA75DC54A327EF353FB84C38E "
    "nickname=Unnamed sampled_on=2016-11-25T14:34:00 "
    "sampled_by=0.3.0.0-alpha-dev listed=1\n"
  "Guard in=wobblesome rsa_id=7B700C0C207EBD0002E00F499BE265519AC3C25A "
    "nickname=dc6jgk11 sampled_on=2016-11-28T11:50:13 "
    "sampled_by=0.3.0.0-alpha-dev "
    "listed=1\n"
  "Guard in=bridges rsa_id=8FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF2E "
    "bridge_addr=24.1.1.1:443 sampled_on=2016-11-25T06:44:14 "
    "sampled_by=0.3.0.0-alpha-dev listed=1 "
    "confirmed_on=2016-11-29T10:36:06 confirmed_idx=0 "
    "pb_circ_attempts=8.000000 pb_circ_successes=8.000000 "
    "pb_successful_circuits_closed=13.000000\n"
  "Guard in=bridges rsa_id=5800000000000000000000000000000000000000 "
    "bridge_addr=37.218.246.143:28366 "
    "sampled_on=2016-11-18T15:07:34 sampled_by=0.3.0.0-alpha-dev listed=1\n");

 done:
  config_free_lines(lines);
  tor_free(state);
  tor_free(msg);
  UNMOCK(get_or_state);
  UNMOCK(entry_guard_is_listed);
  SMARTLIST_FOREACH(text, char *, cp, tor_free(cp));
  smartlist_free(text);
  tor_free(joined);
}

static void
test_entry_guard_parse_from_state_broken(void *arg)
{
  (void)arg;
  /* Here's a variation on the previous state. Every line but the first is
   * busted somehow. */
  const char STATE[] =
  /* Okay. */
  "Guard in=default rsa_id=214F44BD5B638E8C817D47FF7C97397790BF0345 "
    "nickname=TotallyNinja sampled_on=2016-11-12T19:32:49 "
    "sampled_by=0.3.0.0-alpha-dev "
    "listed=1\n"
  /* No selection listed. */
  "Guard rsa_id=052900AB0EA3ED54BAB84AE8A99E74E8693CE2B2 "
    "nickname=5OfNovember sampled_on=2016-11-20T04:32:05 "
    "sampled_by=0.3.0.0-alpha-dev "
    "listed=1 confirmed_on=2016-11-22T08:13:28 confirmed_idx=0 "
    "pb_circ_attempts=4.000000 pb_circ_successes=2.000000 "
    "pb_successful_circuits_closed=2.000000\n"
  /* Selection is "legacy"!! */
  "Guard in=legacy rsa_id=7B700C0C207EBD0002E00F499BE265519AC3C25A "
    "nickname=dc6jgk11 sampled_on=2016-11-28T11:50:13 "
    "sampled_by=0.3.0.0-alpha-dev "
    "listed=1 confirmed_on=2016-11-24T08:45:30 confirmed_idx=4 "
    "pb_circ_attempts=5.000000 pb_circ_successes=5.000000 "
    "pb_successful_circuits_closed=5.000000\n";

  config_line_t *lines = NULL;
  or_state_t *state = tor_malloc_zero(sizeof(or_state_t));
  int r = config_get_lines(STATE, &lines, 0);
  char *msg = NULL;

  dummy_state = state;
  MOCK(get_or_state,
       get_or_state_replacement);

  tt_int_op(r, OP_EQ, 0);
  tt_assert(lines);

  state->Guard = lines;

  /* First, no-set case. we should get an error. */
  r = entry_guards_parse_state(state, 0, &msg);
  tt_int_op(r, OP_LT, 0);
  tt_ptr_op(msg, OP_NE, NULL);
  /* And we shouldn't have made anything. */
  guard_selection_t *gs_df =
    get_guard_selection_by_name("default", GS_TYPE_NORMAL, 0);
  tt_ptr_op(gs_df, OP_EQ, NULL);
  tor_free(msg);

  /* Now see about the set case (which shouldn't happen IRL) */
  r = entry_guards_parse_state(state, 1, &msg);
  tt_int_op(r, OP_LT, 0);
  tt_ptr_op(msg, OP_NE, NULL);
  gs_df = get_guard_selection_by_name("default", GS_TYPE_NORMAL, 0);
  tt_ptr_op(gs_df, OP_NE, NULL);
  tt_int_op(smartlist_len(gs_df->sampled_entry_guards), OP_EQ, 1);

 done:
  config_free_lines(lines);
  tor_free(state);
  tor_free(msg);
  UNMOCK(get_or_state);
}

static void
test_entry_guard_get_guard_selection_by_name(void *arg)
{
  (void)arg;
  guard_selection_t *gs1, *gs2, *gs3;

  gs1 = get_guard_selection_by_name("unlikely", GS_TYPE_NORMAL, 0);
  tt_ptr_op(gs1, OP_EQ, NULL);
  gs1 = get_guard_selection_by_name("unlikely", GS_TYPE_NORMAL, 1);
  tt_ptr_op(gs1, OP_NE, NULL);
  gs2 = get_guard_selection_by_name("unlikely", GS_TYPE_NORMAL, 1);
  tt_assert(gs2 == gs1);
  gs2 = get_guard_selection_by_name("unlikely", GS_TYPE_NORMAL, 0);
  tt_assert(gs2 == gs1);

  gs2 = get_guard_selection_by_name("implausible", GS_TYPE_NORMAL, 0);
  tt_ptr_op(gs2, OP_EQ, NULL);
  gs2 = get_guard_selection_by_name("implausible", GS_TYPE_NORMAL, 1);
  tt_ptr_op(gs2, OP_NE, NULL);
  tt_assert(gs2 != gs1);
  gs3 = get_guard_selection_by_name("implausible", GS_TYPE_NORMAL, 0);
  tt_assert(gs3 == gs2);

  gs3 = get_guard_selection_by_name("default", GS_TYPE_NORMAL, 0);
  tt_ptr_op(gs3, OP_EQ, NULL);
  gs3 = get_guard_selection_by_name("default", GS_TYPE_NORMAL, 1);
  tt_ptr_op(gs3, OP_NE, NULL);
  tt_assert(gs3 != gs2);
  tt_assert(gs3 != gs1);
  tt_assert(gs3 == get_guard_selection_info());

 done:
  entry_guards_free_all();
}

static void
test_entry_guard_choose_selection_initial(void *arg)
{
  /* Tests for picking our initial guard selection (based on having had
   * no previous selection */
  (void)arg;
  guard_selection_type_t type = GS_TYPE_INFER;
  const char *name = choose_guard_selection(get_options(),
                                            dummy_consensus, NULL, &type);
  tt_str_op(name, OP_EQ, "default");
  tt_int_op(type, OP_EQ, GS_TYPE_NORMAL);

  /* If we're using bridges, we get the bridge selection. */
  get_options_mutable()->UseBridges = 1;
  name = choose_guard_selection(get_options(),
                                dummy_consensus, NULL, &type);
  tt_str_op(name, OP_EQ, "bridges");
  tt_int_op(type, OP_EQ, GS_TYPE_BRIDGE);
  get_options_mutable()->UseBridges = 0;

  /* If we discard >99% of our guards, though, we should be in the restricted
   * set. */
  tt_assert(get_options_mutable()->EntryNodes == NULL);
  get_options_mutable()->EntryNodes = routerset_new();
  routerset_parse(get_options_mutable()->EntryNodes, "1.0.0.0/8", "foo");
  name = choose_guard_selection(get_options(),
                                dummy_consensus, NULL, &type);
  tt_str_op(name, OP_EQ, "restricted");
  tt_int_op(type, OP_EQ, GS_TYPE_RESTRICTED);

 done:
  ;
}

static void
test_entry_guard_add_single_guard(void *arg)
{
  (void)arg;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);

  /* 1: Add a single guard to the sample. */
  node_t *n1 = smartlist_get(big_fake_net_nodes, 0);
  time_t now = approx_time();
  tt_assert(n1->is_possible_guard == 1);
  entry_guard_t *g1 = entry_guard_add_to_sample(gs, n1);
  tt_assert(g1);

  /* Make sure its fields look right. */
  tt_mem_op(n1->identity, OP_EQ, g1->identity, DIGEST_LEN);
  tt_i64_op(g1->sampled_on_date, OP_GE, now - 12*86400);
  tt_i64_op(g1->sampled_on_date, OP_LE, now);
  tt_str_op(g1->sampled_by_version, OP_EQ, VERSION);
  tt_uint_op(g1->currently_listed, OP_EQ, 1);
  tt_i64_op(g1->confirmed_on_date, OP_EQ, 0);
  tt_int_op(g1->confirmed_idx, OP_EQ, -1);
  tt_int_op(g1->last_tried_to_connect, OP_EQ, 0);
  tt_uint_op(g1->is_reachable, OP_EQ, GUARD_REACHABLE_MAYBE);
  tt_i64_op(g1->failing_since, OP_EQ, 0);
  tt_uint_op(g1->is_filtered_guard, OP_EQ, 1);
  tt_uint_op(g1->is_usable_filtered_guard, OP_EQ, 1);
  tt_uint_op(g1->is_primary, OP_EQ, 0);
  tt_ptr_op(g1->extra_state_fields, OP_EQ, NULL);

  /* Make sure it got added. */
  tt_int_op(1, OP_EQ, smartlist_len(gs->sampled_entry_guards));
  tt_ptr_op(g1, OP_EQ, smartlist_get(gs->sampled_entry_guards, 0));
  tt_ptr_op(g1, OP_EQ, get_sampled_guard_with_id(gs, (uint8_t*)n1->identity));
  const uint8_t bad_id[20] = {0};
  tt_ptr_op(NULL, OP_EQ, get_sampled_guard_with_id(gs, bad_id));

 done:
  guard_selection_free(gs);
}

static void
test_entry_guard_node_filter(void *arg)
{
  (void)arg;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  bridge_line_t *bl = NULL;

  /* Initialize a bunch of node objects that are all guards. */
#define NUM 7
  node_t *n[NUM];
  entry_guard_t *g[NUM];
  int i;
  for (i=0; i < NUM; ++i) {
    n[i] = smartlist_get(big_fake_net_nodes, i*2); // even ones are guards.
    g[i] = entry_guard_add_to_sample(gs, n[i]);

    // everything starts out filtered-in
    tt_uint_op(g[i]->is_filtered_guard, OP_EQ, 1);
    tt_uint_op(g[i]->is_usable_filtered_guard, OP_EQ, 1);
  }
  tt_int_op(num_reachable_filtered_guards(gs, NULL), OP_EQ, NUM);

  /* Make sure refiltering doesn't hurt */
  entry_guards_update_filtered_sets(gs);
  for (i = 0; i < NUM; ++i) {
    tt_uint_op(g[i]->is_filtered_guard, OP_EQ, 1);
    tt_uint_op(g[i]->is_usable_filtered_guard, OP_EQ, 1);
  }
  tt_int_op(num_reachable_filtered_guards(gs, NULL), OP_EQ, NUM);

  /* Now start doing things to make the guards get filtered out, 1 by 1. */

  /* 0: Not listed. */
  g[0]->currently_listed = 0;

  /* 1: path bias says this guard is maybe eeeevil. */
  g[1]->pb.path_bias_disabled = 1;

  /* 2: Unreachable address. */
  n[2]->rs->addr = 0;

  /* 3: ExcludeNodes */
  n[3]->rs->addr = 0x90902020;
  routerset_free(get_options_mutable()->ExcludeNodes);
  get_options_mutable()->ExcludeNodes = routerset_new();
  routerset_parse(get_options_mutable()->ExcludeNodes, "144.144.0.0/16", "");

  /* 4: Bridge. */
  get_options_mutable()->UseBridges = 1;
  sweep_bridge_list();
  bl = tor_malloc_zero(sizeof(bridge_line_t));
  tor_addr_from_ipv4h(&bl->addr, n[4]->rs->addr);
  bl->port = n[4]->rs->or_port;
  memcpy(bl->digest, n[4]->identity, 20);
  bridge_add_from_config(bl);
  bl = NULL; // prevent free.
  get_options_mutable()->UseBridges = 0;

  /* 5: Unreachable. This stays in the filter, but isn't in usable-filtered */
  g[5]->last_tried_to_connect = approx_time(); // prevent retry.
  g[5]->is_reachable = GUARD_REACHABLE_NO;

  /* 6: no change. */

  /* Now refilter and inspect. */
  entry_guards_update_filtered_sets(gs);
  for (i = 0; i < NUM; ++i) {
    tt_assert(g[i]->is_filtered_guard == (i == 5 || i == 6));
    tt_assert(g[i]->is_usable_filtered_guard == (i == 6));
  }
  tt_int_op(num_reachable_filtered_guards(gs, NULL), OP_EQ, 1);

  /* Now make sure we have no live consensus, and no nodes.  Nothing should
   * pass the filter any more. */
  tor_free(dummy_consensus);
  dummy_consensus = NULL;
  SMARTLIST_FOREACH(big_fake_net_nodes, node_t *, node, {
    memset(node->identity, 0xff, 20);
  });
  entry_guards_update_filtered_sets(gs);
  for (i = 0; i < NUM; ++i) {
    tt_uint_op(g[i]->is_filtered_guard, OP_EQ, 0);
    tt_uint_op(g[i]->is_usable_filtered_guard, OP_EQ, 0);
  }
  tt_int_op(num_reachable_filtered_guards(gs, NULL), OP_EQ, 0);

 done:
  guard_selection_free(gs);
  tor_free(bl);
#undef NUM
}

static void
test_entry_guard_expand_sample(void *arg)
{
  (void)arg;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  digestmap_t *node_by_id = digestmap_new();

  entry_guard_t *guard = entry_guards_expand_sample(gs);
  tt_assert(guard); // the last guard returned.

  // Every sampled guard here should be filtered and reachable for now.
  tt_int_op(smartlist_len(gs->sampled_entry_guards), OP_EQ,
            num_reachable_filtered_guards(gs, NULL));

  /* Make sure we got the right number. */
  tt_int_op(DFLT_MIN_FILTERED_SAMPLE_SIZE, OP_EQ,
            num_reachable_filtered_guards(gs, NULL));

  // Make sure everything we got was from our fake node list, and everything
  // was unique.
  SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, g) {
    const node_t *n = bfn_mock_node_get_by_id(g->identity);
    tt_assert(n);
    tt_ptr_op(NULL, OP_EQ, digestmap_get(node_by_id, g->identity));
    digestmap_set(node_by_id, g->identity, (void*) n);
    int idx = smartlist_pos(big_fake_net_nodes, n);
    // The even ones are the guards; make sure we got guards.
    tt_int_op(idx & 1, OP_EQ, 0);
  } SMARTLIST_FOREACH_END(g);

  // Nothing became unusable/unfiltered, so a subsequent expand should
  // make no changes.
  guard = entry_guards_expand_sample(gs);
  tt_ptr_op(guard, OP_EQ, NULL); // no guard was added.
  tt_int_op(DFLT_MIN_FILTERED_SAMPLE_SIZE, OP_EQ,
            num_reachable_filtered_guards(gs, NULL));

  // Make a few guards unreachable.
  guard = smartlist_get(gs->sampled_entry_guards, 0);
  guard->is_usable_filtered_guard = 0;
  guard = smartlist_get(gs->sampled_entry_guards, 1);
  guard->is_usable_filtered_guard = 0;
  guard = smartlist_get(gs->sampled_entry_guards, 2);
  guard->is_usable_filtered_guard = 0;
  tt_int_op(DFLT_MIN_FILTERED_SAMPLE_SIZE - 3, OP_EQ,
            num_reachable_filtered_guards(gs, NULL));

  // This time, expanding the sample will add some more guards.
  guard = entry_guards_expand_sample(gs);
  tt_assert(guard); // no guard was added.
  tt_int_op(DFLT_MIN_FILTERED_SAMPLE_SIZE, OP_EQ,
            num_reachable_filtered_guards(gs, NULL));
  tt_int_op(smartlist_len(gs->sampled_entry_guards), OP_EQ,
            num_reachable_filtered_guards(gs, NULL)+3);

  // Still idempotent.
  guard = entry_guards_expand_sample(gs);
  tt_ptr_op(guard, OP_EQ, NULL); // no guard was added.
  tt_int_op(DFLT_MIN_FILTERED_SAMPLE_SIZE, OP_EQ,
            num_reachable_filtered_guards(gs, NULL));

  // Now, do a nasty trick: tell the filter to exclude 31/32 of the guards.
  // This will cause the sample size to get reeeeally huge, while the
  // filtered sample size grows only slowly.
  routerset_free(get_options_mutable()->ExcludeNodes);
  get_options_mutable()->ExcludeNodes = routerset_new();
  routerset_parse(get_options_mutable()->ExcludeNodes, "144.144.0.0/16", "");
  SMARTLIST_FOREACH(big_fake_net_nodes, node_t *, n, {
    if (n_sl_idx % 64 != 0) {
      n->rs->addr = 0x90903030;
    }
  });
  entry_guards_update_filtered_sets(gs);

  // Surely (p ~ 1-2**-60), one of our guards has been excluded.
  tt_int_op(num_reachable_filtered_guards(gs, NULL), OP_LT,
            DFLT_MIN_FILTERED_SAMPLE_SIZE);

  // Try to regenerate the guards.
  guard = entry_guards_expand_sample(gs);
  tt_assert(guard); // no guard was added.

  /* this time, it's possible that we didn't add enough sampled guards. */
  tt_int_op(num_reachable_filtered_guards(gs, NULL), OP_LE,
            DFLT_MIN_FILTERED_SAMPLE_SIZE);
  /* but we definitely didn't exceed the sample maximum. */
  const int n_guards = 271 / 2;
  tt_int_op(smartlist_len(gs->sampled_entry_guards), OP_LE,
            (int)(n_guards * .3));

 done:
  guard_selection_free(gs);
  digestmap_free(node_by_id, NULL);
}

static void
test_entry_guard_expand_sample_small_net(void *arg)
{
  (void)arg;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);

  /* Fun corner case: not enough guards to make up our whole sample size. */
  SMARTLIST_FOREACH(big_fake_net_nodes, node_t *, n, {
    if (n_sl_idx >= 15) {
      test_node_free(n);
      SMARTLIST_DEL_CURRENT(big_fake_net_nodes, n);
    } else {
      n->rs->addr = 0; // make the filter reject this.
    }
  });

  entry_guard_t *guard = entry_guards_expand_sample(gs);
  tt_assert(guard); // the last guard returned -- some guard was added.
  // half the nodes are guards, so we have 8 guards left.  The set
  // is small, so we sampled everything.
  tt_int_op(smartlist_len(gs->sampled_entry_guards), OP_EQ, 8);
  tt_int_op(num_reachable_filtered_guards(gs, NULL), OP_EQ, 0);
 done:
  guard_selection_free(gs);
}

static void
test_entry_guard_update_from_consensus_status(void *arg)
{
  /* Here we're going to have some nodes become un-guardy, and say we got a
   * new consensus. This should cause those nodes to get detected as
   * unreachable. */

  (void)arg;
  int i;
  time_t start = approx_time();
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  networkstatus_t *ns_tmp = NULL;

  /* Don't randomly backdate stuff; it will make correctness harder to check.*/
  MOCK(randomize_time, mock_randomize_time_no_randomization);

  /* First, sample some guards. */
  entry_guards_expand_sample(gs);
  int n_sampled_pre = smartlist_len(gs->sampled_entry_guards);
  int n_filtered_pre = num_reachable_filtered_guards(gs, NULL);
  tt_i64_op(n_sampled_pre, OP_EQ, n_filtered_pre);
  tt_i64_op(n_sampled_pre, OP_GT, 10);

  /* At this point, it should be a no-op to do this: */
  sampled_guards_update_from_consensus(gs);

  /* Now let's make some of our guards become unlisted.  The easiest way to
   * do that would be to take away their guard flag. */
  for (i = 0; i < 5; ++i) {
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, i);
    node_t *n = (node_t*) bfn_mock_node_get_by_id(g->identity);
    tt_assert(n);
    n->is_possible_guard = 0;
  }

  update_approx_time(start + 30);
  {
    /* try this with no live networkstatus. Nothing should happen! */
    ns_tmp = dummy_consensus;
    dummy_consensus = NULL;
    sampled_guards_update_from_consensus(gs);
    tt_i64_op(smartlist_len(gs->sampled_entry_guards), OP_EQ, n_sampled_pre);
    tt_i64_op(num_reachable_filtered_guards(gs, NULL), OP_EQ, n_filtered_pre);
    /* put the networkstatus back. */
    dummy_consensus = ns_tmp;
    ns_tmp = NULL;
  }

  /* Now those guards should become unlisted, and drop off the filter, but
   * stay in the sample. */
  update_approx_time(start + 60);
  sampled_guards_update_from_consensus(gs);

  tt_i64_op(smartlist_len(gs->sampled_entry_guards), OP_EQ, n_sampled_pre);
  tt_i64_op(num_reachable_filtered_guards(gs, NULL), OP_EQ, n_filtered_pre-5);
  for (i = 0; i < 5; ++i) {
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, i);
    tt_assert(! g->currently_listed);
    tt_i64_op(g->unlisted_since_date, OP_EQ, start+60);
  }
  for (i = 5; i < n_sampled_pre; ++i) {
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, i);
    tt_assert(g->currently_listed);
    tt_i64_op(g->unlisted_since_date, OP_EQ, 0);
  }

  /* Now re-list one, and remove one completely. */
  {
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, 0);
    node_t *n = (node_t*) bfn_mock_node_get_by_id(g->identity);
    tt_assert(n);
    n->is_possible_guard = 1;
  }
  {
    /* try removing the node, to make sure we don't crash on an absent node
     */
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, 5);
    node_t *n = (node_t*) bfn_mock_node_get_by_id(g->identity);
    tt_assert(n);
    smartlist_remove(big_fake_net_nodes, n);
    test_node_free(n);
  }
  update_approx_time(start + 300);
  sampled_guards_update_from_consensus(gs);

  /* guards 1..5 are now unlisted; 0,6,7.. are listed. */
  tt_i64_op(smartlist_len(gs->sampled_entry_guards), OP_EQ, n_sampled_pre);
  for (i = 1; i < 6; ++i) {
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, i);
    tt_assert(! g->currently_listed);
    if (i == 5)
      tt_i64_op(g->unlisted_since_date, OP_EQ, start+300);
    else
      tt_i64_op(g->unlisted_since_date, OP_EQ, start+60);
  }
  for (i = 0; i < n_sampled_pre; i = (!i) ? 6 : i+1) { /* 0,6,7,8, ... */
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, i);
    tt_assert(g->currently_listed);
    tt_i64_op(g->unlisted_since_date, OP_EQ, 0);
  }

 done:
  tor_free(ns_tmp); /* in case we couldn't put it back */
  guard_selection_free(gs);
  UNMOCK(randomize_time);
}

static void
test_entry_guard_update_from_consensus_repair(void *arg)
{
  /* Here we'll make sure that our code to repair the unlisted-since
   * times is correct. */

  (void)arg;
  int i;
  time_t start = approx_time();
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);

  /* Don't randomly backdate stuff; it will make correctness harder to check.*/
  MOCK(randomize_time, mock_randomize_time_no_randomization);

  /* First, sample some guards. */
  entry_guards_expand_sample(gs);
  int n_sampled_pre = smartlist_len(gs->sampled_entry_guards);
  int n_filtered_pre = num_reachable_filtered_guards(gs, NULL);
  tt_i64_op(n_sampled_pre, OP_EQ, n_filtered_pre);
  tt_i64_op(n_sampled_pre, OP_GT, 10);

  /* Now corrupt the list a bit.  Call some unlisted-since-never, and some
   * listed-and-unlisted-since-a-time. */
  update_approx_time(start + 300);
  for (i = 0; i < 3; ++i) {
    /* these will get a date. */
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, i);
    node_t *n = (node_t*) bfn_mock_node_get_by_id(g->identity);
    tt_assert(n);
    n->is_possible_guard = 0;
    g->currently_listed = 0;
  }
  for (i = 3; i < 6; ++i) {
    /* these will become listed. */
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, i);
    g->unlisted_since_date = start+100;
  }
  setup_full_capture_of_logs(LOG_WARN);
  sampled_guards_update_from_consensus(gs);
  expect_log_msg_containing(
             "was listed, but with unlisted_since_date set");
  expect_log_msg_containing(
             "was unlisted, but with unlisted_since_date unset");
  teardown_capture_of_logs();

  tt_int_op(smartlist_len(gs->sampled_entry_guards), OP_EQ, n_sampled_pre);
  tt_int_op(num_reachable_filtered_guards(gs, NULL), OP_EQ, n_filtered_pre-3);
  for (i = 3; i < n_sampled_pre; ++i) {
    /* these will become listed. */
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, i);
    if (i < 3) {
      tt_assert(! g->currently_listed);
      tt_i64_op(g->unlisted_since_date, OP_EQ, start+300);
    } else {
      tt_assert(g->currently_listed);
      tt_i64_op(g->unlisted_since_date, OP_EQ, 0);
    }
  }

 done:
  teardown_capture_of_logs();
  guard_selection_free(gs);
  UNMOCK(randomize_time);
}

static void
test_entry_guard_update_from_consensus_remove(void *arg)
{
  /* Now let's check the logic responsible for removing guards from the
   * sample entirely. */

  (void)arg;
  //int i;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  smartlist_t *keep_ids = smartlist_new();
  smartlist_t *remove_ids = smartlist_new();

  /* Don't randomly backdate stuff; it will make correctness harder to check.*/
  MOCK(randomize_time, mock_randomize_time_no_randomization);

  /* First, sample some guards. */
  entry_guards_expand_sample(gs);
  int n_sampled_pre = smartlist_len(gs->sampled_entry_guards);
  int n_filtered_pre = num_reachable_filtered_guards(gs, NULL);
  tt_i64_op(n_sampled_pre, OP_EQ, n_filtered_pre);
  tt_i64_op(n_sampled_pre, OP_GT, 10);

  const time_t one_day_ago = approx_time() - 1*24*60*60;
  const time_t one_year_ago = approx_time() - 365*24*60*60;
  const time_t two_years_ago = approx_time() - 2*365*24*60*60;
  /* 0: unlisted for a day. (keep this) */
  {
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, 0);
    node_t *n = (node_t*) bfn_mock_node_get_by_id(g->identity);
    tt_assert(n);
    n->is_possible_guard = 0;
    g->currently_listed = 0;
    g->unlisted_since_date = one_day_ago;
    smartlist_add(keep_ids, tor_memdup(g->identity, 20));
  }
  /* 1: unlisted for a year. (remove this) */
  {
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, 1);
    node_t *n = (node_t*) bfn_mock_node_get_by_id(g->identity);
    tt_assert(n);
    n->is_possible_guard = 0;
    g->currently_listed = 0;
    g->unlisted_since_date = one_year_ago;
    smartlist_add(remove_ids, tor_memdup(g->identity, 20));
  }
  /* 2: added a day ago, never confirmed. (keep this) */
  {
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, 2);
    g->sampled_on_date = one_day_ago;
    smartlist_add(keep_ids, tor_memdup(g->identity, 20));
  }
  /* 3: added a year ago, never confirmed. (remove this) */
  {
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, 3);
    g->sampled_on_date = one_year_ago;
    smartlist_add(remove_ids, tor_memdup(g->identity, 20));
  }
  /* 4: added two year ago, confirmed yesterday, primary. (keep this.) */
  {
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, 4);
    g->sampled_on_date = one_year_ago;
    g->confirmed_on_date = one_day_ago;
    g->confirmed_idx = 0;
    g->is_primary = 1;
    smartlist_add(gs->confirmed_entry_guards, g);
    smartlist_add(gs->primary_entry_guards, g);
    smartlist_add(keep_ids, tor_memdup(g->identity, 20));
  }
  /* 5: added two years ago, confirmed a year ago, primary. (remove this) */
  {
    entry_guard_t *g = smartlist_get(gs->sampled_entry_guards, 5);
    g->sampled_on_date = two_years_ago;
    g->confirmed_on_date = one_year_ago;
    g->confirmed_idx = 1;
    g->is_primary = 1;
    smartlist_add(gs->confirmed_entry_guards, g);
    smartlist_add(gs->primary_entry_guards, g);
    smartlist_add(remove_ids, tor_memdup(g->identity, 20));
  }

  sampled_guards_update_from_consensus(gs);

  /* Did we remove the right ones? */
  SMARTLIST_FOREACH(keep_ids, uint8_t *, id, {
      tt_assert(get_sampled_guard_with_id(gs, id) != NULL);
  });
  SMARTLIST_FOREACH(remove_ids, uint8_t *, id, {
    tt_want(get_sampled_guard_with_id(gs, id) == NULL);
  });

  /* Did we remove the right number? */
  tt_int_op(smartlist_len(gs->sampled_entry_guards), OP_EQ, n_sampled_pre - 3);

 done:
  guard_selection_free(gs);
  UNMOCK(randomize_time);
  SMARTLIST_FOREACH(keep_ids, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(remove_ids, char *, cp, tor_free(cp));
  smartlist_free(keep_ids);
  smartlist_free(remove_ids);
}

static void
test_entry_guard_confirming_guards(void *arg)
{
  (void)arg;
  /* Now let's check the logic responsible for manipulating the list
   * of confirmed guards */
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  MOCK(randomize_time, mock_randomize_time_no_randomization);

  /* Create the sample. */
  entry_guards_expand_sample(gs);

  /* Confirm a few  guards. */
  time_t start = approx_time();
  entry_guard_t *g1 = smartlist_get(gs->sampled_entry_guards, 0);
  entry_guard_t *g2 = smartlist_get(gs->sampled_entry_guards, 1);
  entry_guard_t *g3 = smartlist_get(gs->sampled_entry_guards, 8);
  make_guard_confirmed(gs, g2);
  update_approx_time(start + 10);
  make_guard_confirmed(gs, g1);
  make_guard_confirmed(gs, g3);

  /* Were the correct dates and indices fed in? */
  tt_int_op(g1->confirmed_idx, OP_EQ, 1);
  tt_int_op(g2->confirmed_idx, OP_EQ, 0);
  tt_int_op(g3->confirmed_idx, OP_EQ, 2);
  tt_i64_op(g1->confirmed_on_date, OP_EQ, start+10);
  tt_i64_op(g2->confirmed_on_date, OP_EQ, start);
  tt_i64_op(g3->confirmed_on_date, OP_EQ, start+10);
  tt_ptr_op(smartlist_get(gs->confirmed_entry_guards, 0), OP_EQ, g2);
  tt_ptr_op(smartlist_get(gs->confirmed_entry_guards, 1), OP_EQ, g1);
  tt_ptr_op(smartlist_get(gs->confirmed_entry_guards, 2), OP_EQ, g3);

  /* Now make sure we can regenerate the confirmed_entry_guards list. */
  smartlist_clear(gs->confirmed_entry_guards);
  g2->confirmed_idx = 0;
  g1->confirmed_idx = 10;
  g3->confirmed_idx = 100;
  entry_guards_update_confirmed(gs);
  tt_int_op(g1->confirmed_idx, OP_EQ, 1);
  tt_int_op(g2->confirmed_idx, OP_EQ, 0);
  tt_int_op(g3->confirmed_idx, OP_EQ, 2);
  tt_ptr_op(smartlist_get(gs->confirmed_entry_guards, 0), OP_EQ, g2);
  tt_ptr_op(smartlist_get(gs->confirmed_entry_guards, 1), OP_EQ, g1);
  tt_ptr_op(smartlist_get(gs->confirmed_entry_guards, 2), OP_EQ, g3);

  /* Now make sure we can regenerate the confirmed_entry_guards list if
   * the indices are messed up. */
  g1->confirmed_idx = g2->confirmed_idx = g3->confirmed_idx = 999;
  smartlist_clear(gs->confirmed_entry_guards);
  entry_guards_update_confirmed(gs);
  tt_int_op(g1->confirmed_idx, OP_GE, 0);
  tt_int_op(g2->confirmed_idx, OP_GE, 0);
  tt_int_op(g3->confirmed_idx, OP_GE, 0);
  tt_int_op(g1->confirmed_idx, OP_LE, 2);
  tt_int_op(g2->confirmed_idx, OP_LE, 2);
  tt_int_op(g3->confirmed_idx, OP_LE, 2);
  g1 = smartlist_get(gs->confirmed_entry_guards, 0);
  g2 = smartlist_get(gs->confirmed_entry_guards, 1);
  g3 = smartlist_get(gs->confirmed_entry_guards, 2);
  tt_int_op(g1->confirmed_idx, OP_EQ, 0);
  tt_int_op(g2->confirmed_idx, OP_EQ, 1);
  tt_int_op(g3->confirmed_idx, OP_EQ, 2);
  tt_assert(g1 != g2);
  tt_assert(g1 != g3);
  tt_assert(g2 != g3);

 done:
  UNMOCK(randomize_time);
  guard_selection_free(gs);
}

static void
test_entry_guard_sample_reachable_filtered(void *arg)
{
  (void)arg;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  entry_guards_expand_sample(gs);
  const int N = 10000;
  bitarray_t *selected = NULL;
  int i, j;

  /* We've got a sampled list now; let's make one non-usable-filtered; some
   * confirmed, some primary, some pending.
   */
  int n_guards = smartlist_len(gs->sampled_entry_guards);
  tt_int_op(n_guards, OP_GT, 10);
  entry_guard_t *g;
  g = smartlist_get(gs->sampled_entry_guards, 0);
  g->is_pending = 1;
  g = smartlist_get(gs->sampled_entry_guards, 1);
  make_guard_confirmed(gs, g);
  g = smartlist_get(gs->sampled_entry_guards, 2);
  g->is_primary = 1;
  g = smartlist_get(gs->sampled_entry_guards, 3);
  g->pb.path_bias_disabled = 1;

  entry_guards_update_filtered_sets(gs);
  gs->primary_guards_up_to_date = 1;
  tt_int_op(num_reachable_filtered_guards(gs, NULL), OP_EQ, n_guards - 1);
  tt_int_op(smartlist_len(gs->sampled_entry_guards), OP_EQ, n_guards);

  // +1 since the one we made disabled will make  another one get added.
  ++n_guards;

  /* Try a bunch of selections. */
  const struct {
    int flag; int idx;
  } tests[] = {
    { 0, -1 },
    { SAMPLE_EXCLUDE_CONFIRMED, 1 },
    { SAMPLE_EXCLUDE_PRIMARY|SAMPLE_NO_UPDATE_PRIMARY, 2 },
    { SAMPLE_EXCLUDE_PENDING, 0 },
    { -1, -1},
  };

  for (j = 0; tests[j].flag >= 0; ++j) {
    selected = bitarray_init_zero(n_guards);
    const int excluded_flags = tests[j].flag;
    const int excluded_idx = tests[j].idx;
    for (i = 0; i < N; ++i) {
      g = sample_reachable_filtered_entry_guards(gs, NULL, excluded_flags);
      tor_assert(g);
      int pos = smartlist_pos(gs->sampled_entry_guards, g);
      tt_int_op(smartlist_len(gs->sampled_entry_guards), OP_EQ, n_guards);
      tt_int_op(pos, OP_GE, 0);
      tt_int_op(pos, OP_LT, n_guards);
      bitarray_set(selected, pos);
    }
    for (i = 0; i < n_guards; ++i) {
      const int should_be_set = (i != excluded_idx &&
                                 i != 3); // filtered out.
      tt_int_op(!!bitarray_is_set(selected, i), OP_EQ, should_be_set);
    }
    bitarray_free(selected);
    selected = NULL;
  }

 done:
  guard_selection_free(gs);
  bitarray_free(selected);
}

static void
test_entry_guard_sample_reachable_filtered_empty(void *arg)
{
  (void)arg;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  /* What if we try to sample from a set of 0? */
  SMARTLIST_FOREACH(big_fake_net_nodes, node_t *, n,
                    n->is_possible_guard = 0);

  entry_guard_t *g = sample_reachable_filtered_entry_guards(gs, NULL, 0);
  tt_ptr_op(g, OP_EQ, NULL);

 done:
  guard_selection_free(gs);
}

static void
test_entry_guard_retry_unreachable(void *arg)
{
  (void)arg;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);

  entry_guards_expand_sample(gs);
  /* Let's say that we have two guards, and they're down.
   */
  time_t start = approx_time();
  entry_guard_t *g1 = smartlist_get(gs->sampled_entry_guards, 0);
  entry_guard_t *g2 = smartlist_get(gs->sampled_entry_guards, 1);
  entry_guard_t *g3 = smartlist_get(gs->sampled_entry_guards, 2);
  g1->is_reachable = GUARD_REACHABLE_NO;
  g2->is_reachable = GUARD_REACHABLE_NO;
  g1->is_primary = 1;
  g1->failing_since = g2->failing_since = start;
  g1->last_tried_to_connect = g2->last_tried_to_connect = start;

  /* Wait 5 minutes.  Nothing will get retried. */
  update_approx_time(start + 5 * 60);
  entry_guard_consider_retry(g1);
  entry_guard_consider_retry(g2);
  entry_guard_consider_retry(g3); // just to make sure this doesn't crash.
  tt_int_op(g1->is_reachable, OP_EQ, GUARD_REACHABLE_NO);
  tt_int_op(g2->is_reachable, OP_EQ, GUARD_REACHABLE_NO);
  tt_int_op(g3->is_reachable, OP_EQ, GUARD_REACHABLE_MAYBE);

  /* After 30 min, the primary one gets retried */
  update_approx_time(start + 35 * 60);
  entry_guard_consider_retry(g1);
  entry_guard_consider_retry(g2);
  tt_int_op(g1->is_reachable, OP_EQ, GUARD_REACHABLE_MAYBE);
  tt_int_op(g2->is_reachable, OP_EQ, GUARD_REACHABLE_NO);

  g1->is_reachable = GUARD_REACHABLE_NO;
  g1->last_tried_to_connect = start + 55*60;

  /* After 1 hour, we'll retry the nonprimary one. */
  update_approx_time(start + 61 * 60);
  entry_guard_consider_retry(g1);
  entry_guard_consider_retry(g2);
  tt_int_op(g1->is_reachable, OP_EQ, GUARD_REACHABLE_NO);
  tt_int_op(g2->is_reachable, OP_EQ, GUARD_REACHABLE_MAYBE);

  g2->is_reachable = GUARD_REACHABLE_NO;
  g2->last_tried_to_connect = start + 61*60;

  /* And then the primary one again. */
  update_approx_time(start + 66 * 60);
  entry_guard_consider_retry(g1);
  entry_guard_consider_retry(g2);
  tt_int_op(g1->is_reachable, OP_EQ, GUARD_REACHABLE_MAYBE);
  tt_int_op(g2->is_reachable, OP_EQ, GUARD_REACHABLE_NO);

 done:
  guard_selection_free(gs);
}

static void
test_entry_guard_manage_primary(void *arg)
{
  (void)arg;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  smartlist_t *prev_guards = smartlist_new();

  /* If no guards are confirmed, we should pick a few reachable guards and
   * call them all primary. But not confirmed.*/
  entry_guards_update_primary(gs);
  int n_primary = smartlist_len(gs->primary_entry_guards);
  tt_int_op(n_primary, OP_GE, 1);
  SMARTLIST_FOREACH(gs->primary_entry_guards, entry_guard_t *, g, {
    tt_assert(g->is_primary);
    tt_assert(g->confirmed_idx == -1);
  });

  /* Calling it a second time should leave the guards unchanged. */
  smartlist_add_all(prev_guards, gs->primary_entry_guards);
  entry_guards_update_primary(gs);
  tt_int_op(smartlist_len(gs->primary_entry_guards), OP_EQ, n_primary);
  SMARTLIST_FOREACH(gs->primary_entry_guards, entry_guard_t *, g, {
    tt_ptr_op(g, OP_EQ, smartlist_get(prev_guards, g_sl_idx));
  });

  /* If we have one confirmed guard, that guards becomes the first primary
   * guard, and the other primary guards get kept. */

  /* find a non-primary guard... */
  entry_guard_t *confirmed = NULL;
  SMARTLIST_FOREACH(gs->sampled_entry_guards, entry_guard_t *, g, {
    if (! g->is_primary) {
      confirmed = g;
      break;
    }
  });
  tt_assert(confirmed);
  /* make it confirmed. */
  make_guard_confirmed(gs, confirmed);
  /* update the list... */
  smartlist_clear(prev_guards);
  smartlist_add_all(prev_guards, gs->primary_entry_guards);
  entry_guards_update_primary(gs);

  /*  and see what's primary now! */
  tt_int_op(smartlist_len(gs->primary_entry_guards), OP_EQ, n_primary);
  tt_ptr_op(smartlist_get(gs->primary_entry_guards, 0), OP_EQ, confirmed);
  SMARTLIST_FOREACH(gs->primary_entry_guards, entry_guard_t *, g, {
    tt_assert(g->is_primary);
    if (g_sl_idx == 0)
      continue;
    tt_ptr_op(g, OP_EQ, smartlist_get(prev_guards, g_sl_idx - 1));
  });
  {
    entry_guard_t *prev_last_guard = smartlist_get(prev_guards, n_primary-1);
    tt_assert(! prev_last_guard->is_primary);
  }

  /* Calling it a fourth time should leave the guards unchanged. */
  smartlist_clear(prev_guards);
  smartlist_add_all(prev_guards, gs->primary_entry_guards);
  entry_guards_update_primary(gs);
  tt_int_op(smartlist_len(gs->primary_entry_guards), OP_EQ, n_primary);
  SMARTLIST_FOREACH(gs->primary_entry_guards, entry_guard_t *, g, {
    tt_ptr_op(g, OP_EQ, smartlist_get(prev_guards, g_sl_idx));
  });

  /* Do some dirinfo checks */
  {
    /* Check that we have all required dirinfo for the primaries (that's done
     * in big_fake_network_setup()) */
    char *dir_info_str =
      guard_selection_get_err_str_if_dir_info_missing(gs, 0, 0, 0);
    tt_assert(!dir_info_str);

    /* Now artificially remove the first primary's descriptor and re-check */
    entry_guard_t *first_primary;
    first_primary = smartlist_get(gs->primary_entry_guards, 0);
    /* Change the first primary's identity digest so that the mocked functions
     * can't find its descriptor */
    memset(first_primary->identity, 9, sizeof(first_primary->identity));
    dir_info_str =guard_selection_get_err_str_if_dir_info_missing(gs, 1, 2, 3);
    tt_str_op(dir_info_str, OP_EQ,
              "We're missing descriptors for 1/2 of our primary entry guards "
              "(total microdescriptors: 2/3).");
    tor_free(dir_info_str);
  }

 done:
  guard_selection_free(gs);
  smartlist_free(prev_guards);
}

static void
test_entry_guard_guard_preferred(void *arg)
{
  (void) arg;
  entry_guard_t *g1 = tor_malloc_zero(sizeof(entry_guard_t));
  entry_guard_t *g2 = tor_malloc_zero(sizeof(entry_guard_t));

  g1->confirmed_idx = g2->confirmed_idx = -1;
  g1->last_tried_to_connect = approx_time();
  g2->last_tried_to_connect = approx_time();

  tt_int_op(0, OP_EQ, entry_guard_has_higher_priority(g1, g1));

  /* Neither is pending; priorities equal. */
  tt_int_op(0, OP_EQ, entry_guard_has_higher_priority(g2, g1));
  tt_int_op(0, OP_EQ, entry_guard_has_higher_priority(g1, g2));

  /* If one is pending, the pending one has higher priority */
  g1->is_pending = 1;
  tt_int_op(1, OP_EQ, entry_guard_has_higher_priority(g1, g2));
  tt_int_op(0, OP_EQ, entry_guard_has_higher_priority(g2, g1));

  /* If both are pending, and last_tried_to_connect is equal:
     priorities equal */
  g2->is_pending = 1;
  tt_int_op(0, OP_EQ, entry_guard_has_higher_priority(g2, g1));
  tt_int_op(0, OP_EQ, entry_guard_has_higher_priority(g1, g2));

  /* One had a connection that startied earlier: it has higher priority. */
  g2->last_tried_to_connect -= 10;
  tt_int_op(1, OP_EQ, entry_guard_has_higher_priority(g2, g1));
  tt_int_op(0, OP_EQ, entry_guard_has_higher_priority(g1, g2));

  /* Now, say that g1 is confirmed. It will get higher priority. */
  g1->confirmed_idx = 5;
  tt_int_op(0, OP_EQ, entry_guard_has_higher_priority(g2, g1));
  tt_int_op(1, OP_EQ, entry_guard_has_higher_priority(g1, g2));

  /* But if g2 was confirmed first, it will get priority */
  g2->confirmed_idx = 2;
  tt_int_op(1, OP_EQ, entry_guard_has_higher_priority(g2, g1));
  tt_int_op(0, OP_EQ, entry_guard_has_higher_priority(g1, g2));

 done:
  tor_free(g1);
  tor_free(g2);
}

static void
test_entry_guard_select_for_circuit_no_confirmed(void *arg)
{
  /* Simpler cases: no gaurds are confirmed yet. */
  (void)arg;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  entry_guard_restriction_t *rst = NULL;

  /* simple starting configuration */
  entry_guards_update_primary(gs);
  unsigned state = 9999;

  entry_guard_t *g = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC,
                                                    NULL, &state);

  tt_assert(g);
  tt_assert(g->is_primary);
  tt_int_op(g->confirmed_idx, OP_EQ, -1);
  tt_uint_op(g->is_pending, OP_EQ, 0); // primary implies non-pending.
  tt_uint_op(state, OP_EQ, GUARD_CIRC_STATE_USABLE_ON_COMPLETION);
  tt_i64_op(g->last_tried_to_connect, OP_EQ, approx_time());

  // If we do that again, we should get the same guard.
  entry_guard_t *g2 = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC,
                                                     NULL, &state);
  tt_ptr_op(g2, OP_EQ, g);

  // if we mark that guard down, we should get a different primary guard.
  // auto-retry it.
  g->is_reachable = GUARD_REACHABLE_NO;
  g->failing_since = approx_time() - 10;
  g->last_tried_to_connect = approx_time() - 10;
  state = 9999;
  g2 = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL, &state);
  tt_ptr_op(g2, OP_NE, g);
  tt_assert(g2);
  tt_assert(g2->is_primary);
  tt_int_op(g2->confirmed_idx, OP_EQ, -1);
  tt_uint_op(g2->is_pending, OP_EQ, 0); // primary implies non-pending.
  tt_uint_op(state, OP_EQ, GUARD_CIRC_STATE_USABLE_ON_COMPLETION);
  tt_i64_op(g2->last_tried_to_connect, OP_EQ, approx_time());

  // If we say that the first primary guard was last tried a long time ago, we
  // should get an automatic retry on it.
  g->failing_since = approx_time() - 72*60*60;
  g->last_tried_to_connect = approx_time() - 72*60*60;
  state = 9999;
  g2 = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL, &state);
  tt_ptr_op(g2, OP_EQ, g);
  tt_assert(g2);
  tt_uint_op(state, OP_EQ, GUARD_CIRC_STATE_USABLE_ON_COMPLETION);
  tt_i64_op(g2->last_tried_to_connect, OP_EQ, approx_time());
  tt_int_op(g2->is_reachable, OP_EQ, GUARD_REACHABLE_MAYBE);

  // And if we mark ALL the primary guards down, we should get another guard
  // at random.
  SMARTLIST_FOREACH(gs->primary_entry_guards, entry_guard_t *, guard, {
    guard->is_reachable = GUARD_REACHABLE_NO;
    guard->last_tried_to_connect = approx_time() - 5;
    guard->failing_since = approx_time() - 30;
  });
  state = 9999;
  g2 = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL, &state);
  tt_assert(g2);
  tt_assert(!g2->is_primary);
  tt_int_op(g2->confirmed_idx, OP_EQ, -1);
  tt_uint_op(g2->is_pending, OP_EQ, 1);
  tt_uint_op(state, OP_EQ, GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD);
  tt_i64_op(g2->last_tried_to_connect, OP_EQ, approx_time());
  tt_int_op(g2->is_reachable, OP_EQ, GUARD_REACHABLE_MAYBE);

  // As a bonus, maybe we should be retrying the primary guards. Let's say so.
  mark_primary_guards_maybe_reachable(gs);
  SMARTLIST_FOREACH(gs->primary_entry_guards, entry_guard_t *, guard, {
    tt_int_op(guard->is_reachable, OP_EQ, GUARD_REACHABLE_MAYBE);
    tt_assert(guard->is_usable_filtered_guard == 1);
    // no change to these fields.
    tt_i64_op(guard->last_tried_to_connect, OP_EQ, approx_time() - 5);
    tt_i64_op(guard->failing_since, OP_EQ, approx_time() - 30);
  });

  /* Let's try again and we should get the first primary guard again */
  g = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL, &state);
  tt_ptr_op(g, OP_EQ, smartlist_get(gs->primary_entry_guards, 0));
  g2 = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL, &state);
  tt_ptr_op(g2, OP_EQ, g);

  /* But if we impose a restriction, we don't get the same guard */
  rst = guard_create_exit_restriction((uint8_t*)g->identity);
  g2 = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC, rst, &state);
  tt_ptr_op(g2, OP_NE, g);

 done:
  guard_selection_free(gs);
  entry_guard_restriction_free(rst);
}

static void
test_entry_guard_select_for_circuit_confirmed(void *arg)
{
  /* Case 2: if all the primary guards are down, and there are more confirmed
     guards, we use a confirmed guard. */
  (void)arg;
  int i;
  entry_guard_restriction_t *rst = NULL;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  const int N_CONFIRMED = 10;

  /* slightly more complicated simple starting configuration */
  entry_guards_update_primary(gs);
  for (i = 0; i < N_CONFIRMED; ++i) {
    entry_guard_t *guard = smartlist_get(gs->sampled_entry_guards, i);
    make_guard_confirmed(gs, guard);
  }
  entry_guards_update_primary(gs); // rebuild the primary list.

  unsigned state = 9999;

  // As above, this gives us a primary guard.
  entry_guard_t *g = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC,
                                                    NULL, &state);
  tt_assert(g);
  tt_assert(g->is_primary);
  tt_int_op(g->confirmed_idx, OP_EQ, 0);
  tt_uint_op(g->is_pending, OP_EQ, 0); // primary implies non-pending.
  tt_uint_op(state, OP_EQ, GUARD_CIRC_STATE_USABLE_ON_COMPLETION);
  tt_i64_op(g->last_tried_to_connect, OP_EQ, approx_time());
  tt_ptr_op(g, OP_EQ, smartlist_get(gs->primary_entry_guards, 0));

  // But if we mark all the primary guards down...
  SMARTLIST_FOREACH(gs->primary_entry_guards, entry_guard_t *, guard, {
    guard->last_tried_to_connect = approx_time();
    entry_guards_note_guard_failure(gs, guard);
  });

  // ... we should get a confirmed guard.
  state = 9999;
  g = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL, &state);
  tt_assert(g);
  tt_assert(! g->is_primary);
  tt_int_op(g->confirmed_idx, OP_EQ, smartlist_len(gs->primary_entry_guards));
  tt_assert(g->is_pending);
  tt_uint_op(state, OP_EQ, GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD);
  tt_i64_op(g->last_tried_to_connect, OP_EQ, approx_time());

  // And if we try again, we should get a different confirmed guard, since
  // that one is pending.
  state = 9999;
  entry_guard_t *g2 = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC,
                                                     NULL, &state);
  tt_assert(g2);
  tt_assert(! g2->is_primary);
  tt_ptr_op(g2, OP_NE, g);
  tt_int_op(g2->confirmed_idx, OP_EQ,
            smartlist_len(gs->primary_entry_guards)+1);
  tt_assert(g2->is_pending);
  tt_uint_op(state, OP_EQ, GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD);
  tt_i64_op(g2->last_tried_to_connect, OP_EQ, approx_time());

  // If we say that the next confirmed guard in order is excluded, and
  // we disable EnforceDistinctSubnets, we get the guard AFTER the
  // one we excluded.
  get_options_mutable()->EnforceDistinctSubnets = 0;
  g = smartlist_get(gs->confirmed_entry_guards,
                     smartlist_len(gs->primary_entry_guards)+2);
  rst = guard_create_exit_restriction((uint8_t*)g->identity);
  g2 = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC, rst, &state);
  tt_ptr_op(g2, OP_NE, NULL);
  tt_ptr_op(g2, OP_NE, g);
  tt_int_op(g2->confirmed_idx, OP_EQ,
            smartlist_len(gs->primary_entry_guards)+3);

  // If we make every confirmed guard become pending then we start poking
  // other guards.
  const int n_remaining_confirmed =
    N_CONFIRMED - 3 - smartlist_len(gs->primary_entry_guards);
  for (i = 0; i < n_remaining_confirmed; ++i) {
    g = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL, &state);
    tt_int_op(g->confirmed_idx, OP_GE, 0);
    tt_assert(g);
  }
  state = 9999;
  g = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL, &state);
  tt_assert(g);
  tt_assert(g->is_pending);
  tt_int_op(g->confirmed_idx, OP_EQ, -1);

  // If we EnforceDistinctSubnets and apply a restriction, we get
  // nothing, since we put all of the nodes in the same /16.
  // Regression test for bug 22753/TROVE-2017-006.
  get_options_mutable()->EnforceDistinctSubnets = 1;
  g = smartlist_get(gs->confirmed_entry_guards, 0);
  memcpy(rst->exclude_id, g->identity, DIGEST_LEN);
  g2 = select_entry_guard_for_circuit(gs, GUARD_USAGE_TRAFFIC, rst, &state);
  tt_ptr_op(g2, OP_EQ, NULL);

 done:
  guard_selection_free(gs);
  entry_guard_restriction_free(rst);
}

static void
test_entry_guard_select_for_circuit_highlevel_primary(void *arg)
{
  /* Play around with selecting primary guards for circuits and markign
   * them up and down */
  (void)arg;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);

  time_t start = approx_time();

  const node_t *node = NULL;
  circuit_guard_state_t *guard = NULL;
  entry_guard_t *g;
  guard_usable_t u;
  /*
   * Make sure that the pick-for-circuit API basically works.  We'll get
   * a primary guard, so it'll be usable on completion.
   */
  int r = entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                                       &node, &guard);

  tt_int_op(r, OP_EQ, 0);
  tt_assert(node);
  tt_assert(guard);
  tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_USABLE_ON_COMPLETION);
  g = entry_guard_handle_get(guard->guard);
  tt_assert(g);
  tt_mem_op(g->identity, OP_EQ, node->identity, DIGEST_LEN);
  tt_int_op(g->is_primary, OP_EQ, 1);
  tt_i64_op(g->last_tried_to_connect, OP_EQ, start);
  tt_int_op(g->confirmed_idx, OP_EQ, -1);

  /* Call that circuit successful. */
  update_approx_time(start+15);
  u = entry_guard_succeeded(&guard);
  tt_int_op(u, OP_EQ, GUARD_USABLE_NOW); /* We can use it now. */
  tt_assert(guard);
  tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_COMPLETE);
  g = entry_guard_handle_get(guard->guard);
  tt_assert(g);
  tt_int_op(g->is_reachable, OP_EQ, GUARD_REACHABLE_YES);
  tt_int_op(g->confirmed_idx, OP_EQ, 0);

  circuit_guard_state_free(guard);
  guard = NULL;
  node = NULL;
  g = NULL;

  /* Try again. We'll also get a primary guard this time. (The same one,
     in fact.)  But this time, we'll say the connection has failed. */
  update_approx_time(start+35);
  r = entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                                   &node, &guard);
  tt_int_op(r, OP_EQ, 0);
  tt_assert(node);
  tt_assert(guard);
  tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_USABLE_ON_COMPLETION);
  tt_i64_op(guard->state_set_at, OP_EQ, start+35);
  g = entry_guard_handle_get(guard->guard);
  tt_assert(g);
  tt_mem_op(g->identity, OP_EQ, node->identity, DIGEST_LEN);
  tt_int_op(g->is_primary, OP_EQ, 1);
  tt_i64_op(g->last_tried_to_connect, OP_EQ, start+35);
  tt_int_op(g->confirmed_idx, OP_EQ, 0); // same one.

  /* It's failed!  What will happen to our poor guard? */
  update_approx_time(start+45);
  entry_guard_failed(&guard);
  tt_assert(guard);
  tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_DEAD);
  tt_i64_op(guard->state_set_at, OP_EQ, start+45);
  g = entry_guard_handle_get(guard->guard);
  tt_assert(g);
  tt_int_op(g->is_reachable, OP_EQ, GUARD_REACHABLE_NO);
  tt_i64_op(g->failing_since, OP_EQ, start+45);
  tt_int_op(g->confirmed_idx, OP_EQ, 0); // still confirmed.

  circuit_guard_state_free(guard);
  guard = NULL;
  node = NULL;
  entry_guard_t *g_prev = g;
  g = NULL;

  /* Now try a third time. Since the other one is down, we'll get a different
   * (still primary) guard.
   */
  update_approx_time(start+60);
  r = entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                                   &node, &guard);
  tt_int_op(r, OP_EQ, 0);
  tt_assert(node);
  tt_assert(guard);
  tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_USABLE_ON_COMPLETION);
  g = entry_guard_handle_get(guard->guard);
  tt_assert(g);
  tt_ptr_op(g, OP_NE, g_prev);
  tt_mem_op(g->identity, OP_EQ, node->identity, DIGEST_LEN);
  tt_mem_op(g->identity, OP_NE, g_prev->identity, DIGEST_LEN);
  tt_int_op(g->is_primary, OP_EQ, 1);
  tt_i64_op(g->last_tried_to_connect, OP_EQ, start+60);
  tt_int_op(g->confirmed_idx, OP_EQ, -1); // not confirmed now.

  /* Call this one up; watch it get confirmed. */
  update_approx_time(start+90);
  u = entry_guard_succeeded(&guard);
  tt_int_op(u, OP_EQ, GUARD_USABLE_NOW);
  tt_assert(guard);
  tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_COMPLETE);
  g = entry_guard_handle_get(guard->guard);
  tt_assert(g);
  tt_int_op(g->is_reachable, OP_EQ, GUARD_REACHABLE_YES);
  tt_int_op(g->confirmed_idx, OP_EQ, 1);

 done:
  guard_selection_free(gs);
  circuit_guard_state_free(guard);
}

static void
test_entry_guard_select_for_circuit_highlevel_confirm_other(void *arg)
{
  (void) arg;
  const int N_PRIMARY = DFLT_N_PRIMARY_GUARDS;

  /* At the start, we have no confirmed guards.  We'll mark the primary guards
   * down, then confirm something else.  As soon as we do, it should become
   * primary, and we should get it next time. */

  time_t start = approx_time();
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  circuit_guard_state_t *guard = NULL;
  int i, r;
  const node_t *node = NULL;
  guard_usable_t u;

  /* Declare that we're on the internet. */
  entry_guards_note_internet_connectivity(gs);

  /* Primary guards are down! */
  for (i = 0; i < N_PRIMARY; ++i) {
    r = entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                                     &node, &guard);
    tt_assert(node);
    tt_assert(guard);
    tt_int_op(r, OP_EQ, 0);
    tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_USABLE_ON_COMPLETION);
    entry_guard_failed(&guard);
    circuit_guard_state_free(guard);
    guard = NULL;
    node = NULL;
  }

  /* Next guard should be non-primary. */
  node = NULL;
  r = entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                                   &node, &guard);
  tt_assert(node);
  tt_assert(guard);
  tt_int_op(r, OP_EQ, 0);
  entry_guard_t *g = entry_guard_handle_get(guard->guard);
  tt_assert(g);
  tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD);
  tt_int_op(g->confirmed_idx, OP_EQ, -1);
  tt_int_op(g->is_primary, OP_EQ, 0);
  tt_int_op(g->is_pending, OP_EQ, 1);
  (void)start;

  u = entry_guard_succeeded(&guard);
  /* We're on the internet (by fiat), so this guard will get called "confirmed"
   * and should immediately become primary.
   */
  tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_COMPLETE);
  tt_assert(u == GUARD_USABLE_NOW);
  tt_int_op(g->confirmed_idx, OP_EQ, 0);
  tt_int_op(g->is_primary, OP_EQ, 1);
  tt_int_op(g->is_pending, OP_EQ, 0);

 done:
  guard_selection_free(gs);
  circuit_guard_state_free(guard);
}

static void
test_entry_guard_select_for_circuit_highlevel_primary_retry(void *arg)
{
  (void) arg;
  const int N_PRIMARY = DFLT_N_PRIMARY_GUARDS;

  /* At the start, we have no confirmed guards.  We'll mark the primary guards
   * down, then confirm something else.  As soon as we do, it should become
   * primary, and we should get it next time. */

  time_t start = approx_time();
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  circuit_guard_state_t *guard = NULL, *guard2 = NULL;
  int i, r;
  const node_t *node = NULL;
  entry_guard_t *g;
  guard_usable_t u;

  /* Declare that we're on the internet. */
  entry_guards_note_internet_connectivity(gs);

  /* Make primary guards confirmed (so they won't be superseded by a later
   * guard), then mark them down. */
  for (i = 0; i < N_PRIMARY; ++i) {
    r = entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                                     &node, &guard);
    tt_assert(node);
    tt_assert(guard);
    tt_int_op(r, OP_EQ, 0);
    tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_USABLE_ON_COMPLETION);
    g = entry_guard_handle_get(guard->guard);
    make_guard_confirmed(gs, g);
    tt_int_op(g->is_primary, OP_EQ, 1);
    entry_guard_failed(&guard);
    circuit_guard_state_free(guard);
    tt_int_op(g->is_reachable, OP_EQ, GUARD_REACHABLE_NO);
    guard = NULL;
    node = NULL;
  }

  /* Get another guard that we might try. */
  r = entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                                   &node, &guard);
  tt_assert(node);
  tt_assert(guard);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD);
  g = entry_guard_handle_get(guard->guard);
  tt_int_op(g->is_primary, OP_EQ, 0);

  tt_assert(entry_guards_all_primary_guards_are_down(gs));

  /* And an hour has passed ... */
  update_approx_time(start + 3600);

  /* Say that guard has succeeded! */
  u = entry_guard_succeeded(&guard);
  tt_int_op(u, OP_EQ, GUARD_MAYBE_USABLE_LATER);
  tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_WAITING_FOR_BETTER_GUARD);
  g = entry_guard_handle_get(guard->guard);

  /* The primary guards should have been marked up! */
  SMARTLIST_FOREACH(gs->primary_entry_guards, entry_guard_t *, pg, {
    tt_int_op(pg->is_primary, OP_EQ, 1);
    tt_ptr_op(g, OP_NE, pg);
    tt_int_op(pg->is_reachable, OP_EQ, GUARD_REACHABLE_MAYBE);
  });

  /* Have a circuit to a primary guard succeed. */
  r = entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                                   &node, &guard2);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(guard2->state, OP_EQ, GUARD_CIRC_STATE_USABLE_ON_COMPLETION);
  u = entry_guard_succeeded(&guard2);
  tt_assert(u == GUARD_USABLE_NOW);
  tt_int_op(guard2->state, OP_EQ, GUARD_CIRC_STATE_COMPLETE);

  tt_assert(! entry_guards_all_primary_guards_are_down(gs));

 done:
  guard_selection_free(gs);
  circuit_guard_state_free(guard);
  circuit_guard_state_free(guard2);
}

static void
test_entry_guard_select_and_cancel(void *arg)
{
  (void) arg;
  const int N_PRIMARY = DFLT_N_PRIMARY_GUARDS;
  int i,r;
  const node_t *node = NULL;
  circuit_guard_state_t *guard;
  guard_selection_t *gs = guard_selection_new("default", GS_TYPE_NORMAL);
  entry_guard_t *g;

  /* Once more, we mark all the primary guards down. */
  entry_guards_note_internet_connectivity(gs);
  for (i = 0; i < N_PRIMARY; ++i) {
    r = entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                                     &node, &guard);
    tt_int_op(r, OP_EQ, 0);
    tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_USABLE_ON_COMPLETION);
    g = entry_guard_handle_get(guard->guard);
    tt_int_op(g->is_primary, OP_EQ, 1);
    tt_int_op(g->is_pending, OP_EQ, 0);
    make_guard_confirmed(gs, g);
    entry_guard_failed(&guard);
    circuit_guard_state_free(guard);
    guard = NULL;
    node = NULL;
  }

  tt_assert(entry_guards_all_primary_guards_are_down(gs));

  /* Now get another guard we could try... */
  r = entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                                   &node, &guard);
  tt_assert(node);
  tt_assert(guard);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(guard->state, OP_EQ, GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD);
  g = entry_guard_handle_get(guard->guard);
  tt_int_op(g->is_primary, OP_EQ, 0);
  tt_int_op(g->is_pending, OP_EQ, 1);

  /* Whoops! We should never have asked for this guard. Cancel the request! */
  entry_guard_cancel(&guard);
  tt_ptr_op(guard, OP_EQ, NULL);
  tt_int_op(g->is_primary, OP_EQ, 0);
  tt_int_op(g->is_pending, OP_EQ, 0);

 done:
  guard_selection_free(gs);
  circuit_guard_state_free(guard);
}

static void
test_entry_guard_drop_guards(void *arg)
{
  (void) arg;
  int r;
  const node_t *node = NULL;
  circuit_guard_state_t *guard;
  guard_selection_t *gs = get_guard_selection_info();

  // Pick a guard, to get things set up.
  r = entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                                   &node, &guard);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(gs->sampled_entry_guards), OP_GE,
            DFLT_MIN_FILTERED_SAMPLE_SIZE);
  tt_ptr_op(gs, OP_EQ, get_guard_selection_info());

  // Drop all the guards!  (This is a bad idea....)
  remove_all_entry_guards_for_guard_selection(gs);
  gs = get_guard_selection_info();
  tt_int_op(smartlist_len(gs->sampled_entry_guards), OP_EQ, 0);
  tt_int_op(smartlist_len(gs->primary_entry_guards), OP_EQ, 0);
  tt_int_op(smartlist_len(gs->confirmed_entry_guards), OP_EQ, 0);

 done:
  circuit_guard_state_free(guard);
  guard_selection_free(gs);
}

/* Unit test setup function: Create a fake network, and set everything up
 * for testing the upgrade-a-waiting-circuit code. */
typedef struct {
  guard_selection_t *gs;
  time_t start;
  circuit_guard_state_t *guard1_state;
  circuit_guard_state_t *guard2_state;
  entry_guard_t *guard1;
  entry_guard_t *guard2;
  origin_circuit_t *circ1;
  origin_circuit_t *circ2;
  smartlist_t *all_origin_circuits;
} upgrade_circuits_data_t;
static void *
upgrade_circuits_setup(const struct testcase_t *testcase)
{
  upgrade_circuits_data_t *data = tor_malloc_zero(sizeof(*data));
  guard_selection_t *gs = data->gs =
    guard_selection_new("default", GS_TYPE_NORMAL);
  circuit_guard_state_t *guard;
  const node_t *node;
  entry_guard_t *g;
  int i;
  const int N_PRIMARY = DFLT_N_PRIMARY_GUARDS;
  const char *argument = testcase->setup_data;
  const int make_circ1_succeed = strstr(argument, "c1-done") != NULL;
  const int make_circ2_succeed = strstr(argument, "c2-done") != NULL;

  big_fake_network_setup(testcase);

  /* We're going to set things up in a state where a circuit will be ready to
   * be upgraded.  Each test can make a single change (or not) that should
   * block the upgrade.
   */

  /* First, make all the primary guards confirmed, and down. */
  data->start = approx_time();
  entry_guards_note_internet_connectivity(gs);
  for (i = 0; i < N_PRIMARY; ++i) {
    entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL, &node, &guard);
    g = entry_guard_handle_get(guard->guard);
    make_guard_confirmed(gs, g);
    entry_guard_failed(&guard);
    circuit_guard_state_free(guard);
  }

  /* Grab another couple of guards */
  data->all_origin_circuits = smartlist_new();

  update_approx_time(data->start + 27);
  entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                               &node, &data->guard1_state);
  origin_circuit_t *circ;
  data->circ1 = circ = origin_circuit_new();
  circ->base_.purpose = CIRCUIT_PURPOSE_C_GENERAL;
  circ->guard_state = data->guard1_state;
  smartlist_add(data->all_origin_circuits, circ);

  update_approx_time(data->start + 30);
  entry_guard_pick_for_circuit(gs, GUARD_USAGE_TRAFFIC, NULL,
                               &node, &data->guard2_state);
  data->circ2 = circ = origin_circuit_new();
  circ->base_.purpose = CIRCUIT_PURPOSE_C_GENERAL;
  circ->guard_state = data->guard2_state;
  smartlist_add(data->all_origin_circuits, circ);

  data->guard1 = entry_guard_handle_get(data->guard1_state->guard);
  data->guard2 = entry_guard_handle_get(data->guard2_state->guard);
  tor_assert(data->guard1 != data->guard2);
  tor_assert(data->guard1_state->state ==
             GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD);
  tor_assert(data->guard2_state->state ==
             GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD);

  guard_usable_t r;
  update_approx_time(data->start + 32);
  if (make_circ1_succeed) {
    r = entry_guard_succeeded(&data->guard1_state);
    tor_assert(r == GUARD_MAYBE_USABLE_LATER);
    tor_assert(data->guard1_state->state ==
               GUARD_CIRC_STATE_WAITING_FOR_BETTER_GUARD);
  }
  update_approx_time(data->start + 33);
  if (make_circ2_succeed) {
    r = entry_guard_succeeded(&data->guard2_state);
    tor_assert(r == GUARD_MAYBE_USABLE_LATER);
    tor_assert(data->guard2_state->state ==
               GUARD_CIRC_STATE_WAITING_FOR_BETTER_GUARD);
  }

  return data;
}
static int
upgrade_circuits_cleanup(const struct testcase_t *testcase, void *ptr)
{
  upgrade_circuits_data_t *data = ptr;
  // circuit_guard_state_free(data->guard1_state); // held in circ1
  // circuit_guard_state_free(data->guard2_state); // held in circ2
  guard_selection_free(data->gs);
  smartlist_free(data->all_origin_circuits);
  circuit_free_(TO_CIRCUIT(data->circ1));
  circuit_free_(TO_CIRCUIT(data->circ2));
  tor_free(data);
  return big_fake_network_cleanup(testcase, NULL);
}

static void
test_entry_guard_upgrade_a_circuit(void *arg)
{
  upgrade_circuits_data_t *data = arg;

  /* This is the easy case: we have no COMPLETED circuits, all the
   * primary guards are down, we have two WAITING circuits: one will
   * get upgraded to COMPLETED!  (The one that started first.)
   */

  smartlist_t *result = smartlist_new();
  int r;
  r = entry_guards_upgrade_waiting_circuits(data->gs,
                                            data->all_origin_circuits,
                                            result);
  tt_int_op(r, OP_EQ, 1);
  tt_int_op(smartlist_len(result), OP_EQ, 1);
  origin_circuit_t *oc = smartlist_get(result, 0);

  /* circ1 was started first, so we'll get told to ugrade it... */
  tt_ptr_op(oc, OP_EQ, data->circ1);

  /* And the guard state should be complete */
  tt_ptr_op(data->guard1_state, OP_NE, NULL);
  tt_int_op(data->guard1_state->state, OP_EQ, GUARD_CIRC_STATE_COMPLETE);

 done:
  smartlist_free(result);
}

static void
test_entry_guard_upgrade_blocked_by_live_primary_guards(void *arg)
{
  upgrade_circuits_data_t *data = arg;

  /* If any primary guards might be up, we can't upgrade any waiting
   * circuits.
   */
  mark_primary_guards_maybe_reachable(data->gs);

  smartlist_t *result = smartlist_new();
  int r;
  setup_capture_of_logs(LOG_DEBUG);
  r = entry_guards_upgrade_waiting_circuits(data->gs,
                                            data->all_origin_circuits,
                                            result);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(result), OP_EQ, 0);
  expect_log_msg_containing("not all primary guards were definitely down.");

 done:
  teardown_capture_of_logs();
  smartlist_free(result);
}

static void
test_entry_guard_upgrade_blocked_by_lack_of_waiting_circuits(void *arg)
{
  upgrade_circuits_data_t *data = arg;

  /* If no circuits are waiting, we can't upgrade anything.  (The test
   * setup in this case was told not to make any of the circuits "waiting".)
   */
  smartlist_t *result = smartlist_new();
  int r;
  setup_capture_of_logs(LOG_DEBUG);
  r = entry_guards_upgrade_waiting_circuits(data->gs,
                                            data->all_origin_circuits,
                                            result);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(result), OP_EQ, 0);
  expect_log_msg_containing("Considered upgrading guard-stalled circuits, "
                            "but didn't find any.");

 done:
  teardown_capture_of_logs();
  smartlist_free(result);
}

static void
test_entry_guard_upgrade_blocked_by_better_circ_complete(void *arg)
{
  upgrade_circuits_data_t *data = arg;

  /* We'll run through the logic of upgrade_a_circuit below...
   * and then try again to make sure that circ2 isn't also upgraded.
   */

  smartlist_t *result = smartlist_new();
  int r;
  r = entry_guards_upgrade_waiting_circuits(data->gs,
                                            data->all_origin_circuits,
                                            result);
  tt_int_op(r, OP_EQ, 1);
  tt_int_op(smartlist_len(result), OP_EQ, 1);
  origin_circuit_t *oc = smartlist_get(result, 0);
  tt_ptr_op(oc, OP_EQ, data->circ1);
  tt_ptr_op(data->guard1_state, OP_NE, NULL);
  tt_int_op(data->guard1_state->state, OP_EQ, GUARD_CIRC_STATE_COMPLETE);

  /* Now, try again. Make sure that circ2 isn't upgraded. */
  smartlist_clear(result);
  setup_capture_of_logs(LOG_DEBUG);
  r = entry_guards_upgrade_waiting_circuits(data->gs,
                                            data->all_origin_circuits,
                                            result);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(result), OP_EQ, 0);
  expect_log_msg_containing("At least one complete circuit had higher "
                            "priority, so not upgrading.");

 done:
  teardown_capture_of_logs();
  smartlist_free(result);
}

static void
test_entry_guard_upgrade_not_blocked_by_restricted_circ_complete(void *arg)
{
  upgrade_circuits_data_t *data = arg;

  /* Once more, let circ1 become complete. But this time, we'll claim
   * that circ2 was restricted to not use the same guard as circ1. */
  data->guard2_state->restrictions =
    guard_create_exit_restriction((uint8_t*)data->guard1->identity);

  smartlist_t *result = smartlist_new();
  int r;
  r = entry_guards_upgrade_waiting_circuits(data->gs,
                                            data->all_origin_circuits,
                                            result);
  tt_int_op(r, OP_EQ, 1);
  tt_int_op(smartlist_len(result), OP_EQ, 1);
  origin_circuit_t *oc = smartlist_get(result, 0);
  tt_ptr_op(oc, OP_EQ, data->circ1);
  tt_ptr_op(data->guard1_state, OP_NE, NULL);
  tt_int_op(data->guard1_state->state, OP_EQ, GUARD_CIRC_STATE_COMPLETE);

  /* Now, we try again. Since circ2 has a restriction that circ1 doesn't obey,
   * circ2 _is_ eligible for upgrade. */
  smartlist_clear(result);
  r = entry_guards_upgrade_waiting_circuits(data->gs,
                                            data->all_origin_circuits,
                                            result);
  tt_int_op(r, OP_EQ, 1);
  tt_int_op(smartlist_len(result), OP_EQ, 1);
  origin_circuit_t *oc2 = smartlist_get(result, 0);
  tt_ptr_op(oc2, OP_EQ, data->circ2);

 done:
  smartlist_free(result);
}

static void
test_entry_guard_upgrade_not_blocked_by_worse_circ_complete(void *arg)
{
  upgrade_circuits_data_t *data = arg;
  smartlist_t *result = smartlist_new();
  /* here we manually make circ2 COMPLETE, and make sure that circ1
   * gets made complete anyway, since guard1 has higher priority
   */
  update_approx_time(data->start + 300);
  data->guard2_state->state = GUARD_CIRC_STATE_COMPLETE;
  data->guard2_state->state_set_at = approx_time();
  update_approx_time(data->start + 301);

  /* Now, try again. Make sure that circ1 is approved. */
  int r;
  r = entry_guards_upgrade_waiting_circuits(data->gs,
                                            data->all_origin_circuits,
                                            result);
  tt_int_op(r, OP_EQ, 1);
  tt_int_op(smartlist_len(result), OP_EQ, 1);
  origin_circuit_t *oc = smartlist_get(result, 0);
  tt_ptr_op(oc, OP_EQ, data->circ1);

 done:
  smartlist_free(result);
}

static void
test_entry_guard_upgrade_blocked_by_better_circ_pending(void *arg)
{
  upgrade_circuits_data_t *data = arg;

  /* circ2 is done, but circ1 is still pending. Since circ1 is better,
   * we won't upgrade circ2. */

  /* XXXX Prop271 -- this is a kludge.  I'm making sure circ1 _is_ better,
   * by messing with the guards' confirmed_idx */
  make_guard_confirmed(data->gs, data->guard1);
  {
    int tmp;
    tmp = data->guard1->confirmed_idx;
    data->guard1->confirmed_idx = data->guard2->confirmed_idx;
    data->guard2->confirmed_idx = tmp;
  }

  smartlist_t *result = smartlist_new();
  setup_capture_of_logs(LOG_DEBUG);
  int r;
  r = entry_guards_upgrade_waiting_circuits(data->gs,
                                            data->all_origin_circuits,
                                            result);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(result), OP_EQ, 0);
  expect_log_msg_containing("but 1 pending circuit(s) had higher guard "
                            "priority, so not upgrading.");

 done:
  teardown_capture_of_logs();
  smartlist_free(result);
}

static void
test_entry_guard_upgrade_not_blocked_by_restricted_circ_pending(void *arg)
{
  upgrade_circuits_data_t *data = arg;
  /* circ2 is done, but circ1 is still pending. But when there is a
     restriction on circ2 that circ1 can't satisfy, circ1 can't block
     circ2. */

  /* XXXX Prop271 -- this is a kludge.  I'm making sure circ1 _is_ better,
   * by messing with the guards' confirmed_idx */
  make_guard_confirmed(data->gs, data->guard1);
  {
    int tmp;
    tmp = data->guard1->confirmed_idx;
    data->guard1->confirmed_idx = data->guard2->confirmed_idx;
    data->guard2->confirmed_idx = tmp;
  }

  data->guard2_state->restrictions =
    guard_create_exit_restriction((uint8_t*)data->guard1->identity);

  smartlist_t *result = smartlist_new();
  int r;
  r = entry_guards_upgrade_waiting_circuits(data->gs,
                                            data->all_origin_circuits,
                                            result);
  tt_int_op(r, OP_EQ, 1);
  tt_int_op(smartlist_len(result), OP_EQ, 1);
  origin_circuit_t *oc = smartlist_get(result, 0);
  tt_ptr_op(oc, OP_EQ, data->circ2);

 done:
  smartlist_free(result);
}

static void
test_entry_guard_upgrade_not_blocked_by_worse_circ_pending(void *arg)
{
  upgrade_circuits_data_t *data = arg;

  /* circ1 is done, but circ2 is still pending. Since circ1 is better,
   * we will upgrade it. */
  smartlist_t *result = smartlist_new();
  int r;
  r = entry_guards_upgrade_waiting_circuits(data->gs,
                                            data->all_origin_circuits,
                                            result);
  tt_int_op(r, OP_EQ, 1);
  tt_int_op(smartlist_len(result), OP_EQ, 1);
  origin_circuit_t *oc = smartlist_get(result, 0);
  tt_ptr_op(oc, OP_EQ, data->circ1);

 done:
  smartlist_free(result);
}

static void
test_entry_guard_should_expire_waiting(void *arg)
{
  (void)arg;
  circuit_guard_state_t *fake_state = tor_malloc_zero(sizeof(*fake_state));
  /* We'll leave "guard" unset -- it won't matter here. */

  /* No state? Can't expire. */
  tt_assert(! entry_guard_state_should_expire(NULL));

  /* Let's try one that expires. */
  fake_state->state = GUARD_CIRC_STATE_WAITING_FOR_BETTER_GUARD;
  fake_state->state_set_at =
    approx_time() - DFLT_NONPRIMARY_GUARD_IDLE_TIMEOUT - 1;

  tt_assert(entry_guard_state_should_expire(fake_state));

  /* But it wouldn't expire if we changed the state. */
  fake_state->state = GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD;
  tt_assert(! entry_guard_state_should_expire(fake_state));

  /* And it wouldn't have expired a few seconds ago. */
  fake_state->state = GUARD_CIRC_STATE_WAITING_FOR_BETTER_GUARD;
  fake_state->state_set_at =
    approx_time() - DFLT_NONPRIMARY_GUARD_IDLE_TIMEOUT + 5;
  tt_assert(! entry_guard_state_should_expire(fake_state));

 done:
  tor_free(fake_state);
}

/** Test that the number of primary guards can be controlled using torrc */
static void
test_entry_guard_number_of_primaries(void *arg)
{
  (void) arg;

  /* Get default value */
  tt_int_op(get_n_primary_guards(), OP_EQ, DFLT_N_PRIMARY_GUARDS);

  /* Set number of primaries using torrc */
  get_options_mutable()->NumPrimaryGuards = 42;
  tt_int_op(get_n_primary_guards(), OP_EQ, 42);

 done:
  ;
}

static void
mock_directory_initiate_request(directory_request_t *req)
{
  if (req->guard_state) {
    circuit_guard_state_free(req->guard_state);
  }
}

static networkstatus_t *mock_ns_val = NULL;
static networkstatus_t *
mock_ns_get_by_flavor(consensus_flavor_t f)
{
  (void)f;
  return mock_ns_val;
}

/** Test that when we fetch microdescriptors we skip guards that have
 *  previously failed to serve us needed microdescriptors. */
static void
test_entry_guard_outdated_dirserver_exclusion(void *arg)
{
  int retval;
  response_handler_args_t *args = NULL;
  dir_connection_t *conn = NULL;
  (void) arg;

  /* Test prep: Make a new guard selection */
  guard_selection_t *gs = get_guard_selection_by_name("default",
                                                      GS_TYPE_NORMAL, 1);

  /* ... we want to use entry guards */
  or_options_t *options = get_options_mutable();
  options->UseEntryGuards = 1;
  options->UseBridges = 0;

  /* ... prepare some md digests we want to download in the future */
  smartlist_t *digests = smartlist_new();
  const char *prose = "unhurried and wise, we perceive.";
  for (int i = 0; i < 20; i++) {
    smartlist_add(digests, (char*)prose);
  }

  tt_int_op(smartlist_len(digests), OP_EQ, 20);

  /* ... now mock some functions */
  mock_ns_val = tor_malloc_zero(sizeof(networkstatus_t));
  MOCK(networkstatus_get_latest_consensus_by_flavor, mock_ns_get_by_flavor);
  MOCK(directory_initiate_request, mock_directory_initiate_request);

  /* Test logic:
   *  0. Create a proper guard set and primary guard list.
   *  1. Pretend to fail microdescriptor fetches from all the primary guards.
   *  2. Order another microdescriptor fetch and make sure that primary guards
   *     get skipped since they failed previous fetches.
   */

  { /* Setup primary guard list */
    int i;
    entry_guards_update_primary(gs);
    for (i = 0; i < DFLT_N_PRIMARY_GUARDS; ++i) {
      entry_guard_t *guard = smartlist_get(gs->sampled_entry_guards, i);
      make_guard_confirmed(gs, guard);
    }
    entry_guards_update_primary(gs);
  }

  {
    /* Fail microdesc fetches with all the primary guards */
    args = tor_malloc_zero(sizeof(response_handler_args_t));
    args->status_code = 404;
    args->reason = NULL;
    args->body = NULL;
    args->body_len = 0;

    conn = tor_malloc_zero(sizeof(dir_connection_t));
    conn->requested_resource = tor_strdup("d/jlinblackorigami");
    conn->base_.purpose = DIR_PURPOSE_FETCH_MICRODESC;

    /* Pretend to fail fetches with all primary guards */
    SMARTLIST_FOREACH_BEGIN(gs->primary_entry_guards,const entry_guard_t *,g) {
      memcpy(conn->identity_digest, g->identity, DIGEST_LEN);

      retval = handle_response_fetch_microdesc(conn, args);
      tt_int_op(retval, OP_EQ, 0);
    } SMARTLIST_FOREACH_END(g);
  }

  {
    /* Now order the final md download */
    setup_full_capture_of_logs(LOG_INFO);
    initiate_descriptor_downloads(NULL, DIR_PURPOSE_FETCH_MICRODESC,
                                  digests, 3, 7, 0);

    /* ... and check that because we failed to fetch microdescs from all our
     * primaries, we didn't end up selecting a primary for fetching dir info */
    expect_log_msg_containing("No primary or confirmed guards available.");
    teardown_capture_of_logs();
  }

 done:
  UNMOCK(networkstatus_get_latest_consensus_by_flavor);
  UNMOCK(directory_initiate_request);
  smartlist_free(digests);
  tor_free(mock_ns_val);
  tor_free(args);
  if (conn) {
    tor_free(conn->requested_resource);
    tor_free(conn);
  }
}

/** Test helper to extend the <b>oc</b> circuit path <b>n</b> times and then
 *  ensure that the circuit is now complete. */
static void
helper_extend_circuit_path_n_times(origin_circuit_t *oc, int n)
{
  int retval;
  int i;

  /* Extend path n times */
  for (i = 0 ; i < n ; i++) {
    retval = onion_extend_cpath(oc);
    tt_int_op(retval, OP_EQ, 0);
    tt_int_op(circuit_get_cpath_len(oc), OP_EQ, i+1);
  }

  /* Now do it one last time and see that circ is complete */
  retval = onion_extend_cpath(oc);
  tt_int_op(retval, OP_EQ, 1);

 done:
  ;
}

/** Test for basic Tor path selection. Makes sure we build 3-hop circuits. */
static void
test_entry_guard_basic_path_selection(void *arg)
{
  (void) arg;

  int retval;

  /* Enable entry guards */
  or_options_t *options = get_options_mutable();
  options->UseEntryGuards = 1;

  /* disables /16 check since all nodes have the same addr... */
  options->EnforceDistinctSubnets = 0;

  /* Create our circuit */
  circuit_t *circ = dummy_origin_circuit_new(30);
  origin_circuit_t *oc = TO_ORIGIN_CIRCUIT(circ);
  oc->build_state = tor_malloc_zero(sizeof(cpath_build_state_t));

  /* First pick the exit and pin it on the build_state */
  retval = onion_pick_cpath_exit(oc, NULL, 0);
  tt_int_op(retval, OP_EQ, 0);

  /* Extend path 3 times. First we pick guard, then middle, then exit. */
  helper_extend_circuit_path_n_times(oc, 3);

 done:
  circuit_free_(circ);
}

/** Test helper to build an L2 and L3 vanguard list. The vanguard lists
 *  produced should be completely disjoint. */
static void
helper_setup_vanguard_list(or_options_t *options)
{
  int i = 0;

  /* Add some nodes to the vanguard L2 list */
  options->HSLayer2Nodes = routerset_new();
  for (i = 0; i < 10 ; i += 2) {
    node_t *vanguard_node = smartlist_get(big_fake_net_nodes, i);
    tt_assert(vanguard_node->is_possible_guard);
    routerset_parse(options->HSLayer2Nodes, vanguard_node->rs->nickname, "l2");
  }
  /* also add some nodes to vanguard L3 list
   * (L2 list and L3 list should be disjoint for this test to work) */
  options->HSLayer3Nodes = routerset_new();
  for (i = 10; i < 20 ; i += 2) {
    node_t *vanguard_node = smartlist_get(big_fake_net_nodes, i);
    tt_assert(vanguard_node->is_possible_guard);
    routerset_parse(options->HSLayer3Nodes, vanguard_node->rs->nickname, "l3");
  }

 done:
  ;
}

/** Test to ensure that vanguard path selection works properly.  Ensures that
 *  default vanguard circuits are 4 hops, and that path selection works
 *  correctly given the vanguard settings. */
static void
test_entry_guard_vanguard_path_selection(void *arg)
{
  (void) arg;

  int retval;

  /* Enable entry guards */
  or_options_t *options = get_options_mutable();
  options->UseEntryGuards = 1;

  /* XXX disables /16 check */
  options->EnforceDistinctSubnets = 0;

  /* Setup our vanguard list */
  helper_setup_vanguard_list(options);

  /* Create our circuit */
  circuit_t *circ = dummy_origin_circuit_new(30);
  origin_circuit_t *oc = TO_ORIGIN_CIRCUIT(circ);
  oc->build_state = tor_malloc_zero(sizeof(cpath_build_state_t));
  oc->build_state->is_internal = 1;

  /* Switch circuit purpose to vanguards */
  circ->purpose = CIRCUIT_PURPOSE_HS_VANGUARDS;

  /* First pick the exit and pin it on the build_state */
  tt_int_op(oc->build_state->desired_path_len, OP_EQ, 0);
  retval = onion_pick_cpath_exit(oc, NULL, 0);
  tt_int_op(retval, OP_EQ, 0);

  /* Ensure that vanguards make 4-hop circuits by default */
  tt_int_op(oc->build_state->desired_path_len, OP_EQ, 4);

  /* Extend path as many times as needed to have complete circ. */
  helper_extend_circuit_path_n_times(oc, oc->build_state->desired_path_len);

  /* Test that the cpath linked list is set correctly. */
  crypt_path_t *l1_node = oc->cpath;
  crypt_path_t *l2_node = l1_node->next;
  crypt_path_t *l3_node = l2_node->next;
  crypt_path_t *l4_node = l3_node->next;
  crypt_path_t *l1_node_again = l4_node->next;
  tt_ptr_op(l1_node, OP_EQ, l1_node_again);

  /* Test that L2 is indeed HSLayer2Node */
  retval = routerset_contains_extendinfo(options->HSLayer2Nodes,
                                         l2_node->extend_info);
  tt_int_op(retval, OP_EQ, 4);
  /* test that L3 node is _not_ contained in HSLayer2Node */
  retval = routerset_contains_extendinfo(options->HSLayer2Nodes,
                                         l3_node->extend_info);
  tt_int_op(retval, OP_LT, 4);

  /* Test that L3 is indeed HSLayer3Node */
  retval = routerset_contains_extendinfo(options->HSLayer3Nodes,
                                         l3_node->extend_info);
  tt_int_op(retval, OP_EQ, 4);
  /* test that L2 node is _not_ contained in HSLayer3Node */
  retval = routerset_contains_extendinfo(options->HSLayer3Nodes,
                                         l2_node->extend_info);
  tt_int_op(retval, OP_LT, 4);

  /* TODO: Test that L1 can be the same as exit. To test this we need start
     enforcing EnforceDistinctSubnets again, which means that we need to give
     each test node a different address which currently breaks some tests. */

 done:
  circuit_free_(circ);
}

static const struct testcase_setup_t big_fake_network = {
  big_fake_network_setup, big_fake_network_cleanup
};

static const struct testcase_setup_t upgrade_circuits = {
  upgrade_circuits_setup, upgrade_circuits_cleanup
};

#define NO_PREFIX_TEST(name) \
  { #name, test_ ## name, 0, NULL, NULL }

#define EN_TEST_BASE(name, fork, setup, arg) \
  { #name, test_entry_guard_ ## name, fork, setup, (void*)(arg) }

#define EN_TEST(name)      EN_TEST_BASE(name, 0,       NULL, NULL)
#define EN_TEST_FORK(name) EN_TEST_BASE(name, TT_FORK, NULL, NULL)

#define BFN_TEST(name) \
  EN_TEST_BASE(name, TT_FORK, &big_fake_network, NULL), \
  { #name "_reasonably_live", test_entry_guard_ ## name, TT_FORK, \
    &big_fake_network, (void*)("reasonably-live") }

#define UPGRADE_TEST(name, arg) \
  EN_TEST_BASE(name, TT_FORK, &upgrade_circuits, arg), \
  { #name "_reasonably_live", test_entry_guard_ ## name, TT_FORK, \
    &upgrade_circuits, (void*)(arg " reasonably-live") }

struct testcase_t entrynodes_tests[] = {
  NO_PREFIX_TEST(node_preferred_orport),
  NO_PREFIX_TEST(entry_guard_describe),

  EN_TEST(randomize_time),
  EN_TEST(encode_for_state_minimal),
  EN_TEST(encode_for_state_maximal),
  EN_TEST(parse_from_state_minimal),
  EN_TEST(parse_from_state_maximal),
  EN_TEST(parse_from_state_failure),
  EN_TEST(parse_from_state_partial_failure),

  EN_TEST_FORK(parse_from_state_full),
  EN_TEST_FORK(parse_from_state_broken),
  EN_TEST_FORK(get_guard_selection_by_name),
  EN_TEST_FORK(number_of_primaries),

  BFN_TEST(choose_selection_initial),
  BFN_TEST(add_single_guard),
  BFN_TEST(node_filter),
  BFN_TEST(expand_sample),
  BFN_TEST(expand_sample_small_net),
  BFN_TEST(update_from_consensus_status),
  BFN_TEST(update_from_consensus_repair),
  BFN_TEST(update_from_consensus_remove),
  BFN_TEST(confirming_guards),
  BFN_TEST(sample_reachable_filtered),
  BFN_TEST(sample_reachable_filtered_empty),
  BFN_TEST(retry_unreachable),
  BFN_TEST(manage_primary),

  EN_TEST_FORK(guard_preferred),

  BFN_TEST(select_for_circuit_no_confirmed),
  BFN_TEST(select_for_circuit_confirmed),
  BFN_TEST(select_for_circuit_highlevel_primary),
  BFN_TEST(select_for_circuit_highlevel_confirm_other),
  BFN_TEST(select_for_circuit_highlevel_primary_retry),
  BFN_TEST(select_and_cancel),
  BFN_TEST(drop_guards),
  BFN_TEST(outdated_dirserver_exclusion),
  BFN_TEST(basic_path_selection),
  BFN_TEST(vanguard_path_selection),

  UPGRADE_TEST(upgrade_a_circuit, "c1-done c2-done"),
  UPGRADE_TEST(upgrade_blocked_by_live_primary_guards, "c1-done c2-done"),
  UPGRADE_TEST(upgrade_blocked_by_lack_of_waiting_circuits, ""),
  UPGRADE_TEST(upgrade_blocked_by_better_circ_complete, "c1-done c2-done"),
  UPGRADE_TEST(upgrade_not_blocked_by_restricted_circ_complete,
               "c1-done c2-done"),
  UPGRADE_TEST(upgrade_not_blocked_by_worse_circ_complete, "c1-done c2-done"),
  UPGRADE_TEST(upgrade_blocked_by_better_circ_pending, "c2-done"),
  UPGRADE_TEST(upgrade_not_blocked_by_restricted_circ_pending,
               "c2-done"),
  UPGRADE_TEST(upgrade_not_blocked_by_worse_circ_pending, "c1-done"),

  EN_TEST_FORK(should_expire_waiting),

  END_OF_TESTCASES
};
