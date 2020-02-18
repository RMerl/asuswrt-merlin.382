/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define CIRCUITBUILD_PRIVATE
#define CIRCUITLIST_PRIVATE
#define ENTRYNODES_PRIVATE

#include "core/or/or.h"
#include "test/test.h"
#include "test/test_helpers.h"
#include "test/log_test_helpers.h"
#include "app/config/config.h"
#include "core/or/circuitbuild.h"
#include "core/or/circuitlist.h"

#include "core/or/cpath_build_state_st.h"
#include "core/or/extend_info_st.h"
#include "core/or/origin_circuit_st.h"

#include "feature/client/entrynodes.h"

/* Dummy nodes smartlist for testing */
static smartlist_t dummy_nodes;
/* Dummy exit extend_info for testing */
static extend_info_t dummy_ei;

static int
mock_count_acceptable_nodes(smartlist_t *nodes)
{
  (void)nodes;

  return DEFAULT_ROUTE_LEN + 1;
}

/* Test route lengths when the caller of new_route_len() doesn't
 * specify exit_ei. */
static void
test_new_route_len_noexit(void *arg)
{
  int r;

  (void)arg;
  MOCK(count_acceptable_nodes, mock_count_acceptable_nodes);

  r = new_route_len(CIRCUIT_PURPOSE_C_GENERAL, NULL, &dummy_nodes);
  tt_int_op(DEFAULT_ROUTE_LEN, OP_EQ, r);

  r = new_route_len(CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT, NULL, &dummy_nodes);
  tt_int_op(DEFAULT_ROUTE_LEN, OP_EQ, r);

  r = new_route_len(CIRCUIT_PURPOSE_S_CONNECT_REND, NULL, &dummy_nodes);
  tt_int_op(DEFAULT_ROUTE_LEN, OP_EQ, r);

 done:
  UNMOCK(count_acceptable_nodes);
}

/* Test route lengths where someone else chose the "exit" node, which
 * require an extra hop for safety. */
static void
test_new_route_len_unsafe_exit(void *arg)
{
  int r;

  (void)arg;
  MOCK(count_acceptable_nodes, mock_count_acceptable_nodes);

  /* connecting to hidden service directory */
  r = new_route_len(CIRCUIT_PURPOSE_C_GENERAL, &dummy_ei, &dummy_nodes);
  tt_int_op(DEFAULT_ROUTE_LEN + 1, OP_EQ, r);

  /* client connecting to introduction point */
  r = new_route_len(CIRCUIT_PURPOSE_C_INTRODUCING, &dummy_ei, &dummy_nodes);
  tt_int_op(DEFAULT_ROUTE_LEN + 1, OP_EQ, r);

  /* hidden service connecting to rendezvous point */
  r = new_route_len(CIRCUIT_PURPOSE_S_CONNECT_REND, &dummy_ei, &dummy_nodes);
  tt_int_op(DEFAULT_ROUTE_LEN + 1, OP_EQ, r);

 done:
  UNMOCK(count_acceptable_nodes);
}

/* Test route lengths where we chose the "exit" node, which don't
 * require an extra hop for safety. */
static void
test_new_route_len_safe_exit(void *arg)
{
  int r;

  (void)arg;
  MOCK(count_acceptable_nodes, mock_count_acceptable_nodes);

  /* hidden service connecting to introduction point */
  r = new_route_len(CIRCUIT_PURPOSE_S_ESTABLISH_INTRO, &dummy_ei,
                    &dummy_nodes);
  tt_int_op(DEFAULT_ROUTE_LEN, OP_EQ, r);

  /* router testing its own reachability */
  r = new_route_len(CIRCUIT_PURPOSE_TESTING, &dummy_ei, &dummy_nodes);
  tt_int_op(DEFAULT_ROUTE_LEN, OP_EQ, r);

 done:
  UNMOCK(count_acceptable_nodes);
}

/* Make sure a non-fatal assertion fails when new_route_len() gets an
 * unexpected circuit purpose. */
static void
test_new_route_len_unhandled_exit(void *arg)
{
  int r;

  (void)arg;
  MOCK(count_acceptable_nodes, mock_count_acceptable_nodes);

  tor_capture_bugs_(1);
  setup_full_capture_of_logs(LOG_WARN);
  r = new_route_len(CIRCUIT_PURPOSE_CONTROLLER, &dummy_ei, &dummy_nodes);
  tt_int_op(DEFAULT_ROUTE_LEN + 1, OP_EQ, r);
  tt_int_op(smartlist_len(tor_get_captured_bug_log_()), OP_EQ, 1);
  tt_str_op(smartlist_get(tor_get_captured_bug_log_(), 0), OP_EQ,
            "!(exit_ei && !known_purpose)");
  expect_single_log_msg_containing("Unhandled purpose");
  expect_single_log_msg_containing("with a chosen exit; assuming routelen");
  teardown_capture_of_logs();
  tor_end_capture_bugs_();

 done:
  UNMOCK(count_acceptable_nodes);
}

static void
test_upgrade_from_guard_wait(void *arg)
{
  circuit_t *circ = NULL;
  origin_circuit_t *orig_circ = NULL;
  entry_guard_t *guard = NULL;
  smartlist_t *list = NULL;

  (void) arg;

  circ = dummy_origin_circuit_new(0);
  orig_circ = TO_ORIGIN_CIRCUIT(circ);
  tt_assert(orig_circ);

  orig_circ->build_state = tor_malloc_zero(sizeof(cpath_build_state_t));

  circuit_set_state(circ, CIRCUIT_STATE_GUARD_WAIT);

  /* Put it in guard wait state. */
  guard = tor_malloc_zero(sizeof(*guard));
  guard->in_selection = get_guard_selection_info();

  orig_circ->guard_state =
    circuit_guard_state_new(guard, GUARD_CIRC_STATE_WAITING_FOR_BETTER_GUARD,
                            NULL);

  /* Mark the circuit for close. */
  circuit_mark_for_close(circ, END_CIRC_REASON_TORPROTOCOL);
  tt_int_op(circ->marked_for_close, OP_NE, 0);

  /* We shouldn't pick the mark for close circuit. */
  list = circuit_find_circuits_to_upgrade_from_guard_wait();
  tt_assert(!list);

 done:
  circuit_free(circ);
  entry_guard_free_(guard);
}

struct testcase_t circuitbuild_tests[] = {
  { "noexit", test_new_route_len_noexit, 0, NULL, NULL },
  { "safe_exit", test_new_route_len_safe_exit, 0, NULL, NULL },
  { "unsafe_exit", test_new_route_len_unsafe_exit, 0, NULL, NULL },
  { "unhandled_exit", test_new_route_len_unhandled_exit, 0, NULL, NULL },
  { "upgrade_from_guard_wait", test_upgrade_from_guard_wait, TT_FORK,
    NULL, NULL },
  END_OF_TESTCASES
};
