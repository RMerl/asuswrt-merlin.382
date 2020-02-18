/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file entrynodes.c
 * \brief Code to manage our fixed first nodes for various functions.
 *
 * Entry nodes can be guards (for general use) or bridges (for censorship
 * circumvention).
 *
 * In general, we use entry guards to prevent traffic-sampling attacks:
 * if we chose every circuit independently, an adversary controlling
 * some fraction of paths on the network would observe a sample of every
 * user's traffic. Using guards gives users a chance of not being
 * profiled.
 *
 * The current entry guard selection code is designed to try to avoid
 * _ever_ trying every guard on the network, to try to stick to guards
 * that we've used before, to handle hostile/broken networks, and
 * to behave sanely when the network goes up and down.
 *
 * Our algorithm works as follows: First, we maintain a SAMPLE of guards
 * we've seen in the networkstatus consensus.  We maintain this sample
 * over time, and store it persistently; it is chosen without reference
 * to our configuration or firewall rules.  Guards remain in the sample
 * as they enter and leave the consensus.  We expand this sample as
 * needed, up to a maximum size.
 *
 * As a subset of the sample, we maintain a FILTERED SET of the guards
 * that we would be willing to use if we could connect to them.  The
 * filter removes all the guards that we're excluding because they're
 * bridges (or not bridges), because we have restrictive firewall rules,
 * because of ExcludeNodes, because we of path bias restrictions,
 * because they're absent from the network at present, and so on.
 *
 * As a subset of the filtered set, we keep a REACHABLE FILTERED SET
 * (also called a "usable filtered set") of those guards that we call
 * "reachable" or "maybe reachable".  A guard is reachable if we've
 * connected to it more recently than we've failed.  A guard is "maybe
 * reachable" if we have never tried to connect to it, or if we
 * failed to connect to it so long ago that we no longer think our
 * failure means it's down.
 *
 * As a persistent ordered list whose elements are taken from the
 * sampled set, we track a CONFIRMED GUARDS LIST.  A guard becomes
 * confirmed when we successfully build a circuit through it, and decide
 * to use that circuit.  We order the guards on this list by the order
 * in which they became confirmed.
 *
 * And as a final group, we have an ordered list of PRIMARY GUARDS,
 * whose elements are taken from the filtered set. We prefer
 * confirmed guards to non-confirmed guards for this list, and place
 * other restrictions on it.  The primary guards are the ones that we
 * connect to "when nothing is wrong" -- circuits through them can be used
 * immediately.
 *
 * To build circuits, we take a primary guard if possible -- or a
 * reachable filtered confirmed guard if no primary guard is possible --
 * or a random reachable filtered guard otherwise.  If the guard is
 * primary, we can use the circuit immediately on success.  Otherwise,
 * the guard is now "pending" -- we won't use its circuit unless all
 * of the circuits we're trying to build through better guards have
 * definitely failed.
 *
 * While we're building circuits, we track a little "guard state" for
 * each circuit. We use this to keep track of whether the circuit is
 * one that we can use as soon as it's done, or whether it's one that
 * we should keep around to see if we can do better.  In the latter case,
 * a periodic call to entry_guards_upgrade_waiting_circuits() will
 * eventually upgrade it.
 **/
/* DOCDOC -- expand this.
 *
 * Information invariants:
 *
 * [x] whenever a guard becomes unreachable, clear its usable_filtered flag.
 *
 * [x] Whenever a guard becomes reachable or maybe-reachable, if its filtered
 * flag is set, set its usable_filtered flag.
 *
 * [x] Whenever we get a new consensus, call update_from_consensus(). (LATER.)
 *
 * [x] Whenever the configuration changes in a relevant way, update the
 * filtered/usable flags. (LATER.)
 *
 * [x] Whenever we add a guard to the sample, make sure its filtered/usable
 * flags are set as possible.
 *
 * [x] Whenever we remove a guard from the sample, remove it from the primary
 * and confirmed lists.
 *
 * [x] When we make a guard confirmed, update the primary list.
 *
 * [x] When we make a guard filtered or unfiltered, update the primary list.
 *
 * [x] When we are about to pick a guard, make sure that the primary list is
 * full.
 *
 * [x] Before calling sample_reachable_filtered_entry_guards(), make sure
 * that the filtered, primary, and confirmed flags are up-to-date.
 *
 * [x] Call entry_guard_consider_retry every time we are about to check
 * is_usable_filtered or is_reachable, and every time we set
 * is_filtered to 1.
 *
 * [x] Call entry_guards_changed_for_guard_selection() whenever we update
 * a persistent field.
 */

#define ENTRYNODES_PRIVATE

#include "core/or/or.h"
#include "app/config/config.h"
#include "app/config/confparse.h"
#include "app/config/statefile.h"
#include "core/mainloop/connection.h"
#include "core/mainloop/mainloop.h"
#include "core/or/channel.h"
#include "core/or/circuitbuild.h"
#include "core/or/circuitlist.h"
#include "core/or/circuitstats.h"
#include "core/or/circuituse.h"
#include "core/or/policies.h"
#include "feature/client/bridges.h"
#include "feature/client/circpathbias.h"
#include "feature/client/entrynodes.h"
#include "feature/client/transports.h"
#include "feature/control/control.h"
#include "feature/dircommon/directory.h"
#include "feature/nodelist/describe.h"
#include "feature/nodelist/microdesc.h"
#include "feature/nodelist/networkstatus.h"
#include "feature/nodelist/nickname.h"
#include "feature/nodelist/nodelist.h"
#include "feature/nodelist/node_select.h"
#include "feature/nodelist/routerset.h"
#include "feature/relay/router.h"
#include "lib/crypt_ops/crypto_rand.h"
#include "lib/crypt_ops/digestset.h"
#include "lib/encoding/confline.h"
#include "lib/math/fp.h"

#include "feature/nodelist/node_st.h"
#include "core/or/origin_circuit_st.h"
#include "app/config/or_state_st.h"

/** A list of existing guard selection contexts. */
static smartlist_t *guard_contexts = NULL;
/** The currently enabled guard selection context. */
static guard_selection_t *curr_guard_context = NULL;

/** A value of 1 means that at least one context has changed,
 * and those changes need to be flushed to disk. */
static int entry_guards_dirty = 0;

static void entry_guard_set_filtered_flags(const or_options_t *options,
                                           guard_selection_t *gs,
                                           entry_guard_t *guard);
static void pathbias_check_use_success_count(entry_guard_t *guard);
static void pathbias_check_close_success_count(entry_guard_t *guard);
static int node_is_possible_guard(const node_t *node);
static int node_passes_guard_filter(const or_options_t *options,
                                    const node_t *node);
static entry_guard_t *entry_guard_add_to_sample_impl(guard_selection_t *gs,
                               const uint8_t *rsa_id_digest,
                               const char *nickname,
                               const tor_addr_port_t *bridge_addrport);
static entry_guard_t *get_sampled_guard_by_bridge_addr(guard_selection_t *gs,
                                              const tor_addr_port_t *addrport);
static int entry_guard_obeys_restriction(const entry_guard_t *guard,
                                         const entry_guard_restriction_t *rst);

/** Return 0 if we should apply guardfraction information found in the
 *  consensus. A specific consensus can be specified with the
 *  <b>ns</b> argument, if NULL the most recent one will be picked.*/
int
should_apply_guardfraction(const networkstatus_t *ns)
{
  /* We need to check the corresponding torrc option and the consensus
   * parameter if we need to. */
  const or_options_t *options = get_options();

  /* If UseGuardFraction is 'auto' then check the same-named consensus
   * parameter. If the consensus parameter is not present, default to
   * "off". */
  if (options->UseGuardFraction == -1) {
    return networkstatus_get_param(ns, "UseGuardFraction",
                                   0, /* default to "off" */
                                   0, 1);
  }

  return options->UseGuardFraction;
}

/** Return true iff we know a preferred descriptor for <b>guard</b> */
static int
guard_has_descriptor(const entry_guard_t *guard)
{
  const node_t *node = node_get_by_id(guard->identity);
  if (!node)
    return 0;
  return node_has_preferred_descriptor(node, 1);
}

/**
 * Try to determine the correct type for a selection named "name",
 * if <b>type</b> is GS_TYPE_INFER.
 */
STATIC guard_selection_type_t
guard_selection_infer_type(guard_selection_type_t type,
                           const char *name)
{
  if (type == GS_TYPE_INFER) {
    if (!strcmp(name, "bridges"))
      type = GS_TYPE_BRIDGE;
    else if (!strcmp(name, "restricted"))
      type = GS_TYPE_RESTRICTED;
    else
      type = GS_TYPE_NORMAL;
  }
  return type;
}

/**
 * Allocate and return a new guard_selection_t, with the name <b>name</b>.
 */
STATIC guard_selection_t *
guard_selection_new(const char *name,
                    guard_selection_type_t type)
{
  guard_selection_t *gs;

  type = guard_selection_infer_type(type, name);

  gs = tor_malloc_zero(sizeof(*gs));
  gs->name = tor_strdup(name);
  gs->type = type;
  gs->sampled_entry_guards = smartlist_new();
  gs->confirmed_entry_guards = smartlist_new();
  gs->primary_entry_guards = smartlist_new();

  return gs;
}

/**
 * Return the guard selection called <b>name</b>. If there is none, and
 * <b>create_if_absent</b> is true, then create and return it.  If there
 * is none, and <b>create_if_absent</b> is false, then return NULL.
 */
STATIC guard_selection_t *
get_guard_selection_by_name(const char *name,
                            guard_selection_type_t type,
                            int create_if_absent)
{
  if (!guard_contexts) {
    guard_contexts = smartlist_new();
  }
  SMARTLIST_FOREACH_BEGIN(guard_contexts, guard_selection_t *, gs) {
    if (!strcmp(gs->name, name))
      return gs;
  } SMARTLIST_FOREACH_END(gs);

  if (! create_if_absent)
    return NULL;

  log_debug(LD_GUARD, "Creating a guard selection called %s", name);
  guard_selection_t *new_selection = guard_selection_new(name, type);
  smartlist_add(guard_contexts, new_selection);

  return new_selection;
}

/**
 * Allocate the first guard context that we're planning to use,
 * and make it the current context.
 */
static void
create_initial_guard_context(void)
{
  tor_assert(! curr_guard_context);
  if (!guard_contexts) {
    guard_contexts = smartlist_new();
  }
  guard_selection_type_t type = GS_TYPE_INFER;
  const char *name = choose_guard_selection(
                             get_options(),
                             networkstatus_get_reasonably_live_consensus(
                                                    approx_time(),
                                                    usable_consensus_flavor()),
                             NULL,
                             &type);
  tor_assert(name); // "name" can only be NULL if we had an old name.
  tor_assert(type != GS_TYPE_INFER);
  log_notice(LD_GUARD, "Starting with guard context \"%s\"", name);
  curr_guard_context = get_guard_selection_by_name(name, type, 1);
}

/** Get current default guard_selection_t, creating it if necessary */
guard_selection_t *
get_guard_selection_info(void)
{
  if (!curr_guard_context) {
    create_initial_guard_context();
  }

  return curr_guard_context;
}

/** Return a statically allocated human-readable description of <b>guard</b>
 */
const char *
entry_guard_describe(const entry_guard_t *guard)
{
  static char buf[256];
  tor_snprintf(buf, sizeof(buf),
               "%s ($%s)",
               strlen(guard->nickname) ? guard->nickname : "[bridge]",
               hex_str(guard->identity, DIGEST_LEN));
  return buf;
}

/** Return <b>guard</b>'s 20-byte RSA identity digest */
const char *
entry_guard_get_rsa_id_digest(const entry_guard_t *guard)
{
  return guard->identity;
}

/** Return the pathbias state associated with <b>guard</b>. */
guard_pathbias_t *
entry_guard_get_pathbias_state(entry_guard_t *guard)
{
  return &guard->pb;
}

HANDLE_IMPL(entry_guard, entry_guard_t, ATTR_UNUSED STATIC)

/** Return an interval betweeen 'now' and 'max_backdate' seconds in the past,
 * chosen uniformly at random.  We use this before recording persistent
 * dates, so that we aren't leaking exactly when we recorded it.
 */
MOCK_IMPL(STATIC time_t,
randomize_time,(time_t now, time_t max_backdate))
{
  tor_assert(max_backdate > 0);

  time_t earliest = now - max_backdate;
  time_t latest = now;
  if (earliest <= 0)
    earliest = 1;
  if (latest <= earliest)
    latest = earliest + 1;

  return crypto_rand_time_range(earliest, latest);
}

/**
 * @name parameters for networkstatus algorithm
 *
 * These parameters are taken from the consensus; some are overrideable in
 * the torrc.
 */
/**@{*/
/**
 * We never let our sampled guard set grow larger than this fraction
 * of the guards on the network.
 */
STATIC double
get_max_sample_threshold(void)
{
  int32_t pct =
    networkstatus_get_param(NULL, "guard-max-sample-threshold-percent",
                            DFLT_MAX_SAMPLE_THRESHOLD_PERCENT,
                            1, 100);
  return pct / 100.0;
}
/**
 * We never let our sampled guard set grow larger than this number.
 */
STATIC int
get_max_sample_size_absolute(void)
{
  return (int) networkstatus_get_param(NULL, "guard-max-sample-size",
                                       DFLT_MAX_SAMPLE_SIZE,
                                       1, INT32_MAX);
}
/**
 * We always try to make our sample contain at least this many guards.
 */
STATIC int
get_min_filtered_sample_size(void)
{
  return networkstatus_get_param(NULL, "guard-min-filtered-sample-size",
                                 DFLT_MIN_FILTERED_SAMPLE_SIZE,
                                 1, INT32_MAX);
}
/**
 * If a guard is unlisted for this many days in a row, we remove it.
 */
STATIC int
get_remove_unlisted_guards_after_days(void)
{
  return networkstatus_get_param(NULL,
                                 "guard-remove-unlisted-guards-after-days",
                                 DFLT_REMOVE_UNLISTED_GUARDS_AFTER_DAYS,
                                 1, 365*10);
}

/**
 * Return number of seconds that will make a guard no longer eligible
 * for selection if unlisted for this long.
 */
static time_t
get_remove_unlisted_guards_after_seconds(void)
{
  return get_remove_unlisted_guards_after_days() * 24 * 60 * 60;
}

/**
 * We remove unconfirmed guards from the sample after this many days,
 * regardless of whether they are listed or unlisted.
 */
STATIC int
get_guard_lifetime(void)
{
  if (get_options()->GuardLifetime >= 86400)
    return get_options()->GuardLifetime;
  int32_t days;
  days = networkstatus_get_param(NULL,
                                 "guard-lifetime-days",
                                 DFLT_GUARD_LIFETIME_DAYS, 1, 365*10);
  return days * 86400;
}
/**
 * We remove confirmed guards from the sample if they were sampled
 * GUARD_LIFETIME_DAYS ago and confirmed this many days ago.
 */
STATIC int
get_guard_confirmed_min_lifetime(void)
{
  if (get_options()->GuardLifetime >= 86400)
    return get_options()->GuardLifetime;
  int32_t days;
  days = networkstatus_get_param(NULL, "guard-confirmed-min-lifetime-days",
                                 DFLT_GUARD_CONFIRMED_MIN_LIFETIME_DAYS,
                                 1, 365*10);
  return days * 86400;
}
/**
 * How many guards do we try to keep on our primary guard list?
 */
STATIC int
get_n_primary_guards(void)
{
  /* If the user has explicitly configured the number of primary guards, do
   * what the user wishes to do */
  const int configured_primaries = get_options()->NumPrimaryGuards;
  if (configured_primaries) {
    return configured_primaries;
  }

  /* otherwise check for consensus parameter and if that's not set either, just
   * use the default value. */
  return networkstatus_get_param(NULL,
                                 "guard-n-primary-guards",
                                 DFLT_N_PRIMARY_GUARDS, 1, INT32_MAX);
}
/**
 * Return the number of the live primary guards we should look at when
 * making a circuit.
 */
STATIC int
get_n_primary_guards_to_use(guard_usage_t usage)
{
  int configured;
  const char *param_name;
  int param_default;

  /* If the user has explicitly configured the amount of guards, use
     that. Otherwise, fall back to the default value. */
  if (usage == GUARD_USAGE_DIRGUARD) {
    configured = get_options()->NumDirectoryGuards;
    param_name = "guard-n-primary-dir-guards-to-use";
    param_default = DFLT_N_PRIMARY_DIR_GUARDS_TO_USE;
  } else {
    configured = get_options()->NumEntryGuards;
    param_name = "guard-n-primary-guards-to-use";
    param_default = DFLT_N_PRIMARY_GUARDS_TO_USE;
  }
  if (configured >= 1) {
    return configured;
  }
  return networkstatus_get_param(NULL,
                                 param_name, param_default, 1, INT32_MAX);
}
/**
 * If we haven't successfully built or used a circuit in this long, then
 * consider that the internet is probably down.
 */
STATIC int
get_internet_likely_down_interval(void)
{
  return networkstatus_get_param(NULL, "guard-internet-likely-down-interval",
                                 DFLT_INTERNET_LIKELY_DOWN_INTERVAL,
                                 1, INT32_MAX);
}
/**
 * If we're trying to connect to a nonprimary guard for at least this
 * many seconds, and we haven't gotten the connection to work, we will treat
 * lower-priority guards as usable.
 */
STATIC int
get_nonprimary_guard_connect_timeout(void)
{
  return networkstatus_get_param(NULL,
                                 "guard-nonprimary-guard-connect-timeout",
                                 DFLT_NONPRIMARY_GUARD_CONNECT_TIMEOUT,
                                 1, INT32_MAX);
}
/**
 * If a circuit has been sitting around in 'waiting for better guard' state
 * for at least this long, we'll expire it.
 */
STATIC int
get_nonprimary_guard_idle_timeout(void)
{
  return networkstatus_get_param(NULL,
                                 "guard-nonprimary-guard-idle-timeout",
                                 DFLT_NONPRIMARY_GUARD_IDLE_TIMEOUT,
                                 1, INT32_MAX);
}
/**
 * If our configuration retains fewer than this fraction of guards from the
 * torrc, we are in a restricted setting.
 */
STATIC double
get_meaningful_restriction_threshold(void)
{
  int32_t pct = networkstatus_get_param(NULL,
                                        "guard-meaningful-restriction-percent",
                                        DFLT_MEANINGFUL_RESTRICTION_PERCENT,
                                        1, INT32_MAX);
  return pct / 100.0;
}
/**
 * If our configuration retains fewer than this fraction of guards from the
 * torrc, we are in an extremely restricted setting, and should warn.
 */
STATIC double
get_extreme_restriction_threshold(void)
{
  int32_t pct = networkstatus_get_param(NULL,
                                        "guard-extreme-restriction-percent",
                                        DFLT_EXTREME_RESTRICTION_PERCENT,
                                        1, INT32_MAX);
  return pct / 100.0;
}

/* Mark <b>guard</b> as maybe reachable again. */
static void
mark_guard_maybe_reachable(entry_guard_t *guard)
{
  if (guard->is_reachable != GUARD_REACHABLE_NO) {
    return;
  }

  /* Note that we do not clear failing_since: this guard is now only
   * _maybe-reachable_. */
  guard->is_reachable = GUARD_REACHABLE_MAYBE;
  if (guard->is_filtered_guard)
    guard->is_usable_filtered_guard = 1;
}

/**
 * Called when the network comes up after having seemed to be down for
 * a while: Mark the primary guards as maybe-reachable so that we'll
 * try them again.
 */
STATIC void
mark_primary_guards_maybe_reachable(guard_selection_t *gs)
{
  tor_assert(gs);

  if (!gs->primary_guards_up_to_date)
    entry_guards_update_primary(gs);

  SMARTLIST_FOREACH_BEGIN(gs->primary_entry_guards, entry_guard_t *, guard) {
    mark_guard_maybe_reachable(guard);
  } SMARTLIST_FOREACH_END(guard);
}

/* Called when we exhaust all guards in our sampled set: Marks all guards as
   maybe-reachable so that we 'll try them again. */
static void
mark_all_guards_maybe_reachable(guard_selection_t *gs)
{
  tor_assert(gs);

  SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, guard) {
    mark_guard_maybe_reachable(guard);
  } SMARTLIST_FOREACH_END(guard);
}

/**@}*/

/**
 * Given our options and our list of nodes, return the name of the
 * guard selection that we should use.  Return NULL for "use the
 * same selection you were using before.
 */
STATIC const char *
choose_guard_selection(const or_options_t *options,
                       const networkstatus_t *live_ns,
                       const guard_selection_t *old_selection,
                       guard_selection_type_t *type_out)
{
  tor_assert(options);
  tor_assert(type_out);

  if (options->UseBridges) {
    *type_out = GS_TYPE_BRIDGE;
    return "bridges";
  }

  if (! live_ns) {
    /* without a networkstatus, we can't tell any more than that. */
    *type_out = GS_TYPE_NORMAL;
    return "default";
  }

  const smartlist_t *nodes = nodelist_get_list();
  int n_guards = 0, n_passing_filter = 0;
  SMARTLIST_FOREACH_BEGIN(nodes, const node_t *, node) {
    if (node_is_possible_guard(node)) {
      ++n_guards;
      if (node_passes_guard_filter(options, node)) {
        ++n_passing_filter;
      }
    }
  } SMARTLIST_FOREACH_END(node);

  /* We use separate 'high' and 'low' thresholds here to prevent flapping
   * back and forth */
  const int meaningful_threshold_high =
    (int)(n_guards * get_meaningful_restriction_threshold() * 1.05);
  const int meaningful_threshold_mid =
    (int)(n_guards * get_meaningful_restriction_threshold());
  const int meaningful_threshold_low =
    (int)(n_guards * get_meaningful_restriction_threshold() * .95);
  const int extreme_threshold =
    (int)(n_guards * get_extreme_restriction_threshold());

  /*
    If we have no previous selection, then we're "restricted" iff we are
    below the meaningful restriction threshold.  That's easy enough.

    But if we _do_ have a previous selection, we make it a little
    "sticky": we only move from "restricted" to "default" when we find
    that we're above the threshold plus 5%, and we only move from
    "default" to "restricted" when we're below the threshold minus 5%.
    That should prevent us from flapping back and forth if we happen to
    be hovering very close to the default.

    The extreme threshold is for warning only.
  */

  static int have_warned_extreme_threshold = 0;
  if (n_guards &&
      n_passing_filter < extreme_threshold &&
      ! have_warned_extreme_threshold) {
    have_warned_extreme_threshold = 1;
    const double exclude_frac =
      (n_guards - n_passing_filter) / (double)n_guards;
    log_warn(LD_GUARD, "Your configuration excludes %d%% of all possible "
             "guards. That's likely to make you stand out from the "
             "rest of the world.", (int)(exclude_frac * 100));
  }

  /* Easy case: no previous selection. Just check if we are in restricted or
     normal guard selection. */
  if (old_selection == NULL) {
    if (n_passing_filter >= meaningful_threshold_mid) {
      *type_out = GS_TYPE_NORMAL;
      return "default";
    } else {
      *type_out = GS_TYPE_RESTRICTED;
      return "restricted";
    }
  }

  /* Trickier case: we do have a previous guard selection context. */
  tor_assert(old_selection);

  /* Use high and low thresholds to decide guard selection, and if we fall in
     the middle then keep the current guard selection context. */
  if (n_passing_filter >= meaningful_threshold_high) {
    *type_out = GS_TYPE_NORMAL;
    return "default";
  } else if (n_passing_filter < meaningful_threshold_low) {
    *type_out = GS_TYPE_RESTRICTED;
    return "restricted";
  } else {
    /* we are in the middle: maintain previous guard selection */
    *type_out = old_selection->type;
    return old_selection->name;
  }
}

/**
 * Check whether we should switch from our current guard selection to a
 * different one.  If so, switch and return 1.  Return 0 otherwise.
 *
 * On a 1 return, the caller should mark all currently live circuits unusable
 * for new streams, by calling circuit_mark_all_unused_circs() and
 * circuit_mark_all_dirty_circs_as_unusable().
 */
int
update_guard_selection_choice(const or_options_t *options)
{
  if (!curr_guard_context) {
    create_initial_guard_context();
    return 1;
  }

  guard_selection_type_t type = GS_TYPE_INFER;
  const char *new_name = choose_guard_selection(
                             options,
                             networkstatus_get_reasonably_live_consensus(
                                                    approx_time(),
                                                    usable_consensus_flavor()),
                             curr_guard_context,
                             &type);
  tor_assert(new_name);
  tor_assert(type != GS_TYPE_INFER);

  const char *cur_name = curr_guard_context->name;
  if (! strcmp(cur_name, new_name)) {
    log_debug(LD_GUARD,
              "Staying with guard context \"%s\" (no change)", new_name);
    return 0; // No change
  }

  log_notice(LD_GUARD, "Switching to guard context \"%s\" (was using \"%s\")",
             new_name, cur_name);
  guard_selection_t *new_guard_context;
  new_guard_context = get_guard_selection_by_name(new_name, type, 1);
  tor_assert(new_guard_context);
  tor_assert(new_guard_context != curr_guard_context);
  curr_guard_context = new_guard_context;

  return 1;
}

/**
 * Return true iff <b>node</b> has all the flags needed for us to consider it
 * a possible guard when sampling guards.
 */
static int
node_is_possible_guard(const node_t *node)
{
  /* The "GUARDS" set is all nodes in the nodelist for which this predicate
   * holds. */

  tor_assert(node);
  return (node->is_possible_guard &&
          node->is_stable &&
          node->is_fast &&
          node->is_valid &&
          node_is_dir(node) &&
          !router_digest_is_me(node->identity));
}

/**
 * Return the sampled guard with the RSA identity digest <b>rsa_id</b>, or
 * NULL if we don't have one. */
STATIC entry_guard_t *
get_sampled_guard_with_id(guard_selection_t *gs,
                          const uint8_t *rsa_id)
{
  tor_assert(gs);
  tor_assert(rsa_id);
  SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, guard) {
    if (tor_memeq(guard->identity, rsa_id, DIGEST_LEN))
      return guard;
  } SMARTLIST_FOREACH_END(guard);
  return NULL;
}

/** If <b>gs</b> contains a sampled entry guard matching <b>bridge</b>,
 * return that guard. Otherwise return NULL. */
static entry_guard_t *
get_sampled_guard_for_bridge(guard_selection_t *gs,
                             const bridge_info_t *bridge)
{
  const uint8_t *id = bridge_get_rsa_id_digest(bridge);
  const tor_addr_port_t *addrport = bridge_get_addr_port(bridge);
  entry_guard_t *guard;
  if (BUG(!addrport))
    return NULL; // LCOV_EXCL_LINE
  guard = get_sampled_guard_by_bridge_addr(gs, addrport);
  if (! guard || (id && tor_memneq(id, guard->identity, DIGEST_LEN)))
    return NULL;
  else
    return guard;
}

/** If we know a bridge_info_t matching <b>guard</b>, return that
 * bridge.  Otherwise return NULL. */
static bridge_info_t *
get_bridge_info_for_guard(const entry_guard_t *guard)
{
  const uint8_t *identity = NULL;
  if (! tor_digest_is_zero(guard->identity)) {
    identity = (const uint8_t *)guard->identity;
  }
  if (BUG(guard->bridge_addr == NULL))
    return NULL;

  return get_configured_bridge_by_exact_addr_port_digest(
                                                 &guard->bridge_addr->addr,
                                                 guard->bridge_addr->port,
                                                 (const char*)identity);
}

/**
 * Return true iff we have a sampled guard with the RSA identity digest
 * <b>rsa_id</b>. */
static inline int
have_sampled_guard_with_id(guard_selection_t *gs, const uint8_t *rsa_id)
{
  return get_sampled_guard_with_id(gs, rsa_id) != NULL;
}

/**
 * Allocate a new entry_guard_t object for <b>node</b>, add it to the
 * sampled entry guards in <b>gs</b>, and return it. <b>node</b> must
 * not currently be a sampled guard in <b>gs</b>.
 */
STATIC entry_guard_t *
entry_guard_add_to_sample(guard_selection_t *gs,
                          const node_t *node)
{
  log_info(LD_GUARD, "Adding %s to the entry guard sample set.",
           node_describe(node));

  /* make sure that the guard is not already sampled. */
  if (BUG(have_sampled_guard_with_id(gs, (const uint8_t*)node->identity)))
    return NULL; // LCOV_EXCL_LINE

  return entry_guard_add_to_sample_impl(gs,
                                        (const uint8_t*)node->identity,
                                        node_get_nickname(node),
                                        NULL);
}

/**
 * Backend: adds a new sampled guard to <b>gs</b>, with given identity,
 * nickname, and ORPort.  rsa_id_digest and bridge_addrport are optional, but
 * we need one of them. nickname is optional. The caller is responsible for
 * maintaining the size limit of the SAMPLED_GUARDS set.
 */
static entry_guard_t *
entry_guard_add_to_sample_impl(guard_selection_t *gs,
                               const uint8_t *rsa_id_digest,
                               const char *nickname,
                               const tor_addr_port_t *bridge_addrport)
{
  const int GUARD_LIFETIME = get_guard_lifetime();
  tor_assert(gs);

  // XXXX #20827 take ed25519 identity here too.

  /* Make sure we can actually identify the guard. */
  if (BUG(!rsa_id_digest && !bridge_addrport))
    return NULL; // LCOV_EXCL_LINE

  entry_guard_t *guard = tor_malloc_zero(sizeof(entry_guard_t));

  /* persistent fields */
  guard->is_persistent = (rsa_id_digest != NULL);
  guard->selection_name = tor_strdup(gs->name);
  if (rsa_id_digest)
    memcpy(guard->identity, rsa_id_digest, DIGEST_LEN);
  if (nickname)
    strlcpy(guard->nickname, nickname, sizeof(guard->nickname));
  guard->sampled_on_date = randomize_time(approx_time(), GUARD_LIFETIME/10);
  tor_free(guard->sampled_by_version);
  guard->sampled_by_version = tor_strdup(VERSION);
  guard->currently_listed = 1;
  guard->confirmed_idx = -1;

  /* non-persistent fields */
  guard->is_reachable = GUARD_REACHABLE_MAYBE;
  if (bridge_addrport)
    guard->bridge_addr = tor_memdup(bridge_addrport, sizeof(*bridge_addrport));

  smartlist_add(gs->sampled_entry_guards, guard);
  guard->in_selection = gs;
  entry_guard_set_filtered_flags(get_options(), gs, guard);
  entry_guards_changed_for_guard_selection(gs);
  return guard;
}

/**
 * Add an entry guard to the "bridges" guard selection sample, with
 * information taken from <b>bridge</b>. Return that entry guard.
 */
static entry_guard_t *
entry_guard_add_bridge_to_sample(guard_selection_t *gs,
                                 const bridge_info_t *bridge)
{
  const uint8_t *id_digest = bridge_get_rsa_id_digest(bridge);
  const tor_addr_port_t *addrport = bridge_get_addr_port(bridge);

  tor_assert(addrport);

  /* make sure that the guard is not already sampled. */
  if (BUG(get_sampled_guard_for_bridge(gs, bridge)))
    return NULL; // LCOV_EXCL_LINE

  return entry_guard_add_to_sample_impl(gs, id_digest, NULL, addrport);
}

/**
 * Return the entry_guard_t in <b>gs</b> whose address is <b>addrport</b>,
 * or NULL if none exists.
*/
static entry_guard_t *
get_sampled_guard_by_bridge_addr(guard_selection_t *gs,
                                 const tor_addr_port_t *addrport)
{
  if (! gs)
    return NULL;
  if (BUG(!addrport))
    return NULL;
  SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, g) {
    if (g->bridge_addr && tor_addr_port_eq(addrport, g->bridge_addr))
      return g;
  } SMARTLIST_FOREACH_END(g);
  return NULL;
}

/** Update the guard subsystem's knowledge of the identity of the bridge
 * at <b>addrport</b>.  Idempotent.
 */
void
entry_guard_learned_bridge_identity(const tor_addr_port_t *addrport,
                                    const uint8_t *rsa_id_digest)
{
  guard_selection_t *gs = get_guard_selection_by_name("bridges",
                                                      GS_TYPE_BRIDGE,
                                                      0);
  if (!gs)
    return;

  entry_guard_t *g = get_sampled_guard_by_bridge_addr(gs, addrport);
  if (!g)
    return;

  int make_persistent = 0;

  if (tor_digest_is_zero(g->identity)) {
    memcpy(g->identity, rsa_id_digest, DIGEST_LEN);
    make_persistent = 1;
  } else if (tor_memeq(g->identity, rsa_id_digest, DIGEST_LEN)) {
    /* Nothing to see here; we learned something we already knew. */
    if (BUG(! g->is_persistent))
      make_persistent = 1;
  } else {
    char old_id[HEX_DIGEST_LEN+1];
    base16_encode(old_id, sizeof(old_id), g->identity, sizeof(g->identity));
    log_warn(LD_BUG, "We 'learned' an identity %s for a bridge at %s:%d, but "
             "we already knew a different one (%s). Ignoring the new info as "
             "possibly bogus.",
             hex_str((const char *)rsa_id_digest, DIGEST_LEN),
             fmt_and_decorate_addr(&addrport->addr), addrport->port,
             old_id);
    return; // redundant, but let's be clear: we're not making this persistent.
  }

  if (make_persistent) {
    g->is_persistent = 1;
    entry_guards_changed_for_guard_selection(gs);
  }
}

/**
 * Return the number of sampled guards in <b>gs</b> that are "filtered"
 * (that is, we're willing to connect to them) and that are "usable"
 * (that is, either "reachable" or "maybe reachable").
 *
 * If a restriction is provided in <b>rst</b>, do not count any guards that
 * violate it.
 */
STATIC int
num_reachable_filtered_guards(const guard_selection_t *gs,
                              const entry_guard_restriction_t *rst)
{
  int n_reachable_filtered_guards = 0;
  SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, guard) {
    entry_guard_consider_retry(guard);
    if (! entry_guard_obeys_restriction(guard, rst))
      continue;
    if (guard->is_usable_filtered_guard)
      ++n_reachable_filtered_guards;
  } SMARTLIST_FOREACH_END(guard);
  return n_reachable_filtered_guards;
}

/** Return the actual maximum size for the sample in <b>gs</b>,
 * given that we know about <b>n_guards</b> total. */
static int
get_max_sample_size(guard_selection_t *gs,
                    int n_guards)
{
  const int using_bridges = (gs->type == GS_TYPE_BRIDGE);
  const int min_sample = get_min_filtered_sample_size();

  /* If we are in bridge mode, expand our sample set as needed without worrying
   * about max size. We should respect the user's wishes to use many bridges if
   * that's what they have specified in their configuration file. */
  if (using_bridges)
    return INT_MAX;

  const int max_sample_by_pct = (int)(n_guards * get_max_sample_threshold());
  const int max_sample_absolute = get_max_sample_size_absolute();
  const int max_sample = MIN(max_sample_by_pct, max_sample_absolute);
  if (max_sample < min_sample)
    return min_sample;
  else
    return max_sample;
}

/**
 * Return a smartlist of the all the guards that are not currently
 * members of the sample (GUARDS - SAMPLED_GUARDS).  The elements of
 * this list are node_t pointers in the non-bridge case, and
 * bridge_info_t pointers in the bridge case.  Set *<b>n_guards_out/b>
 * to the number of guards that we found in GUARDS, including those
 * that were already sampled.
 */
static smartlist_t *
get_eligible_guards(const or_options_t *options,
                    guard_selection_t *gs,
                    int *n_guards_out)
{
  /* Construct eligible_guards as GUARDS - SAMPLED_GUARDS */
  smartlist_t *eligible_guards = smartlist_new();
  int n_guards = 0; // total size of "GUARDS"

  if (gs->type == GS_TYPE_BRIDGE) {
    const smartlist_t *bridges = bridge_list_get();
    SMARTLIST_FOREACH_BEGIN(bridges, bridge_info_t *, bridge) {
      ++n_guards;
      if (NULL != get_sampled_guard_for_bridge(gs, bridge)) {
        continue;
      }
      smartlist_add(eligible_guards, bridge);
    } SMARTLIST_FOREACH_END(bridge);
  } else {
    const smartlist_t *nodes = nodelist_get_list();
    const int n_sampled = smartlist_len(gs->sampled_entry_guards);

    /* Build a bloom filter of our current guards: let's keep this O(N). */
    digestset_t *sampled_guard_ids = digestset_new(n_sampled);
    SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, const entry_guard_t *,
                            guard) {
      digestset_add(sampled_guard_ids, guard->identity);
    } SMARTLIST_FOREACH_END(guard);

    SMARTLIST_FOREACH_BEGIN(nodes, const node_t *, node) {
      if (! node_is_possible_guard(node))
        continue;
      if (gs->type == GS_TYPE_RESTRICTED) {
        /* In restricted mode, we apply the filter BEFORE sampling, so
         * that we are sampling from the nodes that we might actually
         * select. If we sampled first, we might wind up with a sample
         * that didn't include any EntryNodes at all. */
        if (! node_passes_guard_filter(options, node))
          continue;
      }
      ++n_guards;
      if (digestset_probably_contains(sampled_guard_ids, node->identity))
        continue;
      smartlist_add(eligible_guards, (node_t*)node);
    } SMARTLIST_FOREACH_END(node);

    /* Now we can free that bloom filter. */
    digestset_free(sampled_guard_ids);
  }

  *n_guards_out = n_guards;
  return eligible_guards;
}

/** Helper: given a smartlist of either bridge_info_t (if gs->type is
 * GS_TYPE_BRIDGE) or node_t (otherwise), pick one that can be a guard,
 * add it as a guard, remove it from the list, and return a new
 * entry_guard_t.  Return NULL on failure. */
static entry_guard_t *
select_and_add_guard_item_for_sample(guard_selection_t *gs,
                                     smartlist_t *eligible_guards)
{
  entry_guard_t *added_guard;
  if (gs->type == GS_TYPE_BRIDGE) {
    const bridge_info_t *bridge = smartlist_choose(eligible_guards);
    if (BUG(!bridge))
      return NULL; // LCOV_EXCL_LINE
    smartlist_remove(eligible_guards, bridge);
    added_guard = entry_guard_add_bridge_to_sample(gs, bridge);
  } else {
    const node_t *node =
      node_sl_choose_by_bandwidth(eligible_guards, WEIGHT_FOR_GUARD);
    if (BUG(!node))
      return NULL; // LCOV_EXCL_LINE
    smartlist_remove(eligible_guards, node);
    added_guard = entry_guard_add_to_sample(gs, node);
  }

  return added_guard;
}

/**
 * Return true iff we need a consensus to update our guards, but we don't
 * have one. (We can return 0 here either if the consensus is _not_ missing,
 * or if we don't need a consensus because we're using bridges.)
 */
static int
reasonably_live_consensus_is_missing(const guard_selection_t *gs)
{
  tor_assert(gs);
  if (gs->type == GS_TYPE_BRIDGE) {
    /* We don't update bridges from the consensus; they aren't there. */
    return 0;
  }
  return networkstatus_get_reasonably_live_consensus(
                                            approx_time(),
                                            usable_consensus_flavor()) == NULL;
}

/**
 * Add new guards to the sampled guards in <b>gs</b> until there are
 * enough usable filtered guards, but never grow the sample beyond its
 * maximum size.  Return the last guard added, or NULL if none were
 * added.
 */
STATIC entry_guard_t *
entry_guards_expand_sample(guard_selection_t *gs)
{
  tor_assert(gs);
  const or_options_t *options = get_options();

  if (reasonably_live_consensus_is_missing(gs)) {
    log_info(LD_GUARD, "Not expanding the sample guard set; we have "
             "no reasonably live consensus.");
    return NULL;
  }

  int n_sampled = smartlist_len(gs->sampled_entry_guards);
  entry_guard_t *added_guard = NULL;
  int n_usable_filtered_guards = num_reachable_filtered_guards(gs, NULL);
  int n_guards = 0;
  smartlist_t *eligible_guards = get_eligible_guards(options, gs, &n_guards);

  const int max_sample = get_max_sample_size(gs, n_guards);
  const int min_filtered_sample = get_min_filtered_sample_size();

  log_info(LD_GUARD, "Expanding the sample guard set. We have %d guards "
           "in the sample, and %d eligible guards to extend it with.",
           n_sampled, smartlist_len(eligible_guards));

  while (n_usable_filtered_guards < min_filtered_sample) {
    /* Has our sample grown too large to expand? */
    if (n_sampled >= max_sample) {
      log_info(LD_GUARD, "Not expanding the guard sample any further; "
               "just hit the maximum sample threshold of %d",
               max_sample);
      goto done;
    }

    /* Did we run out of guards? */
    if (smartlist_len(eligible_guards) == 0) {
      /* LCOV_EXCL_START
         As long as MAX_SAMPLE_THRESHOLD makes can't be adjusted to
         allow all guards to be sampled, this can't be reached.
       */
      log_info(LD_GUARD, "Not expanding the guard sample any further; "
               "just ran out of eligible guards");
      goto done;
      /* LCOV_EXCL_STOP */
    }

    /* Otherwise we can add at least one new guard. */
    added_guard = select_and_add_guard_item_for_sample(gs, eligible_guards);
    if (!added_guard)
      goto done; // LCOV_EXCL_LINE -- only fails on BUG.

    ++n_sampled;

    if (added_guard->is_usable_filtered_guard)
      ++n_usable_filtered_guards;
  }

 done:
  smartlist_free(eligible_guards);
  return added_guard;
}

/**
 * Helper: <b>guard</b> has just been removed from the sampled guards:
 * also remove it from primary and confirmed. */
static void
remove_guard_from_confirmed_and_primary_lists(guard_selection_t *gs,
                                              entry_guard_t *guard)
{
  if (guard->is_primary) {
    guard->is_primary = 0;
    smartlist_remove_keeporder(gs->primary_entry_guards, guard);
  } else {
    if (BUG(smartlist_contains(gs->primary_entry_guards, guard))) {
      smartlist_remove_keeporder(gs->primary_entry_guards, guard);
    }
  }

  if (guard->confirmed_idx >= 0) {
    smartlist_remove_keeporder(gs->confirmed_entry_guards, guard);
    guard->confirmed_idx = -1;
    guard->confirmed_on_date = 0;
  } else {
    if (BUG(smartlist_contains(gs->confirmed_entry_guards, guard))) {
      // LCOV_EXCL_START
      smartlist_remove_keeporder(gs->confirmed_entry_guards, guard);
      // LCOV_EXCL_STOP
    }
  }
}

/** Return true iff <b>guard</b> is currently "listed" -- that is, it
 * appears in the consensus, or as a configured bridge (as
 * appropriate) */
MOCK_IMPL(STATIC int,
entry_guard_is_listed,(guard_selection_t *gs, const entry_guard_t *guard))
{
  if (gs->type == GS_TYPE_BRIDGE) {
    return NULL != get_bridge_info_for_guard(guard);
  } else {
    const node_t *node = node_get_by_id(guard->identity);

    return node && node_is_possible_guard(node);
  }
}

/**
 * Enumerate <b>sampled_entry_guards</b> smartlist in <b>gs</b>.
 * For each <b>entry_guard_t</b> object in smartlist, do the following:
 *  * Update <b>currently_listed</b> field to reflect if guard is listed
 *    in guard selection <b>gs</b>.
 *  * Set <b>unlisted_since_date</b> to approximate UNIX time of
 *    unlisting if guard is unlisted (randomize within 20% of
 *    get_remove_unlisted_guards_after_seconds()). Otherwise,
 *    set it to 0.
 *
 * Require <b>gs</b> to be non-null pointer.
 * Return a number of entries updated.
 */
static size_t
sampled_guards_update_consensus_presence(guard_selection_t *gs)
{
  size_t n_changes = 0;

  tor_assert(gs);

  const time_t unlisted_since_slop =
    get_remove_unlisted_guards_after_seconds() /  5;

  SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, guard) {
    /* XXXX #20827 check ed ID too */
    const int is_listed = entry_guard_is_listed(gs, guard);

    if (is_listed && ! guard->currently_listed) {
      ++n_changes;
      guard->currently_listed = 1;
      guard->unlisted_since_date = 0;
      log_info(LD_GUARD, "Sampled guard %s is now listed again.",
               entry_guard_describe(guard));
    } else if (!is_listed && guard->currently_listed) {
      ++n_changes;
      guard->currently_listed = 0;
      guard->unlisted_since_date = randomize_time(approx_time(),
                                                  unlisted_since_slop);
      log_info(LD_GUARD, "Sampled guard %s is now unlisted.",
               entry_guard_describe(guard));
    } else if (is_listed && guard->currently_listed) {
      log_debug(LD_GUARD, "Sampled guard %s is still listed.",
               entry_guard_describe(guard));
    } else {
      tor_assert(! is_listed && ! guard->currently_listed);
      log_debug(LD_GUARD, "Sampled guard %s is still unlisted.",
                entry_guard_describe(guard));
    }

    /* Clean up unlisted_since_date, just in case. */
    if (guard->currently_listed && guard->unlisted_since_date) {
      ++n_changes;
      guard->unlisted_since_date = 0;
      log_warn(LD_BUG, "Sampled guard %s was listed, but with "
               "unlisted_since_date set. Fixing.",
               entry_guard_describe(guard));
    } else if (!guard->currently_listed && ! guard->unlisted_since_date) {
      ++n_changes;
      guard->unlisted_since_date = randomize_time(approx_time(),
                                                  unlisted_since_slop);
      log_warn(LD_BUG, "Sampled guard %s was unlisted, but with "
               "unlisted_since_date unset. Fixing.",
               entry_guard_describe(guard));
    }
  } SMARTLIST_FOREACH_END(guard);

  return n_changes;
}

/**
 * Enumerate <b>sampled_entry_guards</b> smartlist in <b>gs</b>.
 * For each <b>entry_guard_t</b> object in smartlist, do the following:
 * * If <b>currently_listed</b> is false and <b>unlisted_since_date</b>
 *   is earlier than <b>remove_if_unlisted_since</b> - remove it.
 * * Otherwise, check if <b>sampled_on_date</b> is earlier than
 *   <b>maybe_remove_if_sampled_before</b>.
 *   * When above condition is correct, remove the guard if:
 *     * It was never confirmed.
 *     * It was confirmed before <b>remove_if_confirmed_before</b>.
 *
 * Require <b>gs</b> to be non-null pointer.
 * Return number of entries deleted.
 */
static size_t
sampled_guards_prune_obsolete_entries(guard_selection_t *gs,
                                  const time_t remove_if_unlisted_since,
                                  const time_t maybe_remove_if_sampled_before,
                                  const time_t remove_if_confirmed_before)
{
  size_t n_changes = 0;

  tor_assert(gs);

  SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, guard) {
    int rmv = 0;

    if (guard->currently_listed == 0 &&
        guard->unlisted_since_date < remove_if_unlisted_since) {
      /*
        "We have a live consensus, and {IS_LISTED} is false, and
         {FIRST_UNLISTED_AT} is over get_remove_unlisted_guards_after_days()
         days in the past."
      */
      log_info(LD_GUARD, "Removing sampled guard %s: it has been unlisted "
               "for over %d days", entry_guard_describe(guard),
               get_remove_unlisted_guards_after_days());
      rmv = 1;
    } else if (guard->sampled_on_date < maybe_remove_if_sampled_before) {
      /* We have a live consensus, and {ADDED_ON_DATE} is over
        {GUARD_LIFETIME} ago, *and* {CONFIRMED_ON_DATE} is either
        "never", or over {GUARD_CONFIRMED_MIN_LIFETIME} ago.
      */
      if (guard->confirmed_on_date == 0) {
        rmv = 1;
        log_info(LD_GUARD, "Removing sampled guard %s: it was sampled "
                 "over %d days ago, but never confirmed.",
                 entry_guard_describe(guard),
                 get_guard_lifetime() / 86400);
      } else if (guard->confirmed_on_date < remove_if_confirmed_before) {
        rmv = 1;
        log_info(LD_GUARD, "Removing sampled guard %s: it was sampled "
                 "over %d days ago, and confirmed over %d days ago.",
                 entry_guard_describe(guard),
                 get_guard_lifetime() / 86400,
                 get_guard_confirmed_min_lifetime() / 86400);
      }
    }

    if (rmv) {
      ++n_changes;
      SMARTLIST_DEL_CURRENT(gs->sampled_entry_guards, guard);
      remove_guard_from_confirmed_and_primary_lists(gs, guard);
      entry_guard_free(guard);
    }
  } SMARTLIST_FOREACH_END(guard);

  return n_changes;
}

/**
 * Update the status of all sampled guards based on the arrival of a
 * new consensus networkstatus document.  This will include marking
 * some guards as listed or unlisted, and removing expired guards. */
STATIC void
sampled_guards_update_from_consensus(guard_selection_t *gs)
{
  tor_assert(gs);

  // It's important to use a reasonably live consensus here; we want clients
  // to bootstrap even if their clock is skewed by more than 2-3 hours.
  // But we don't want to make changes based on anything that's really old.
  if (reasonably_live_consensus_is_missing(gs)) {
    log_info(LD_GUARD, "Not updating the sample guard set; we have "
             "no reasonably live consensus.");
    return;
  }
  log_info(LD_GUARD, "Updating sampled guard status based on received "
           "consensus.");

  /* First: Update listed/unlisted. */
  size_t n_changes = sampled_guards_update_consensus_presence(gs);

  const time_t remove_if_unlisted_since =
    approx_time() - get_remove_unlisted_guards_after_seconds();
  const time_t maybe_remove_if_sampled_before =
    approx_time() - get_guard_lifetime();
  const time_t remove_if_confirmed_before =
    approx_time() - get_guard_confirmed_min_lifetime();

  /* Then: remove the ones that have been junk for too long */
  n_changes +=
    sampled_guards_prune_obsolete_entries(gs,
                                          remove_if_unlisted_since,
                                          maybe_remove_if_sampled_before,
                                          remove_if_confirmed_before);

  if (n_changes) {
    gs->primary_guards_up_to_date = 0;
    entry_guards_update_filtered_sets(gs);
    /* We don't need to rebuild the confirmed list right here -- we may have
     * removed confirmed guards above, but we can't have added any new
     * confirmed guards.
     */
    entry_guards_changed_for_guard_selection(gs);
  }
}

/**
 * Return true iff <b>node</b> is a Tor relay that we are configured to
 * be able to connect to. */
static int
node_passes_guard_filter(const or_options_t *options,
                         const node_t *node)
{
  /* NOTE: Make sure that this function stays in sync with
   * options_transition_affects_entry_guards */
  if (routerset_contains_node(options->ExcludeNodes, node))
    return 0;

  if (options->EntryNodes &&
      !routerset_contains_node(options->EntryNodes, node))
    return 0;

  if (!fascist_firewall_allows_node(node, FIREWALL_OR_CONNECTION, 0))
    return 0;

  if (node_is_a_configured_bridge(node))
    return 0;

  return 1;
}

/** Helper: Return true iff <b>bridge</b> passes our configuration
 * filter-- if it is a relay that we are configured to be able to
 * connect to. */
static int
bridge_passes_guard_filter(const or_options_t *options,
                           const bridge_info_t *bridge)
{
  tor_assert(bridge);
  if (!bridge)
    return 0;

  if (routerset_contains_bridge(options->ExcludeNodes, bridge))
    return 0;

  /* Ignore entrynodes */
  const tor_addr_port_t *addrport = bridge_get_addr_port(bridge);

  if (!fascist_firewall_allows_address_addr(&addrport->addr,
                                            addrport->port,
                                            FIREWALL_OR_CONNECTION,
                                            0, 0))
    return 0;

  return 1;
}

/**
 * Return true iff <b>guard</b> is a Tor relay that we are configured to
 * be able to connect to, and we haven't disabled it for omission from
 * the consensus or path bias issues. */
static int
entry_guard_passes_filter(const or_options_t *options, guard_selection_t *gs,
                          entry_guard_t *guard)
{
  if (guard->currently_listed == 0)
    return 0;
  if (guard->pb.path_bias_disabled)
    return 0;

  if (gs->type == GS_TYPE_BRIDGE) {
    const bridge_info_t *bridge = get_bridge_info_for_guard(guard);
    if (bridge == NULL)
      return 0;
    return bridge_passes_guard_filter(options, bridge);
  } else {
    const node_t *node = node_get_by_id(guard->identity);
    if (node == NULL) {
      // This can happen when currently_listed is true, and we're not updating
      // it because we don't have a live consensus.
      return 0;
    }

    return node_passes_guard_filter(options, node);
  }
}

/** Return true iff <b>guard</b> is in the same family as <b>node</b>.
 */
static int
guard_in_node_family(const entry_guard_t *guard, const node_t *node)
{
  const node_t *guard_node = node_get_by_id(guard->identity);
  if (guard_node) {
    return nodes_in_same_family(guard_node, node);
  } else {
    /* If we don't have a node_t for the guard node, we might have
     * a bridge_info_t for it. So let's check to see whether the bridge
     * address matches has any family issues.
     *
     * (Strictly speaking, I believe this check is unnecessary, since we only
     * use it to avoid the exit's family when building circuits, and we don't
     * build multihop circuits until we have a routerinfo_t for the
     * bridge... at which point, we'll also have a node_t for the
     * bridge. Nonetheless, it seems wise to include it, in case our
     * assumptions change down the road.  -nickm.)
     */
    if (get_options()->EnforceDistinctSubnets && guard->bridge_addr) {
      tor_addr_t node_addr;
      node_get_addr(node, &node_addr);
      if (addrs_in_same_network_family(&node_addr,
                                       &guard->bridge_addr->addr)) {
        return 1;
      }
    }
    return 0;
  }
}

/* Allocate and return a new exit guard restriction (where <b>exit_id</b> is of
 * size DIGEST_LEN) */
STATIC entry_guard_restriction_t *
guard_create_exit_restriction(const uint8_t *exit_id)
{
  entry_guard_restriction_t *rst = NULL;
  rst = tor_malloc_zero(sizeof(entry_guard_restriction_t));
  rst->type = RST_EXIT_NODE;
  memcpy(rst->exclude_id, exit_id, DIGEST_LEN);
  return rst;
}

/** If we have fewer than this many possible usable guards, don't set
 * MD-availability-based restrictions: we might blacklist all of them. */
#define MIN_GUARDS_FOR_MD_RESTRICTION 10

/** Return true if we should set md dirserver restrictions. We might not want
 *  to set those if our guard options are too restricted, since we don't want
 *  to blacklist all of them. */
static int
should_set_md_dirserver_restriction(void)
{
  const guard_selection_t *gs = get_guard_selection_info();
  int num_usable_guards = num_reachable_filtered_guards(gs, NULL);

  /* Don't set restriction if too few reachable filtered guards. */
  if (num_usable_guards < MIN_GUARDS_FOR_MD_RESTRICTION) {
    log_info(LD_GUARD, "Not setting md restriction: only %d"
             " usable guards.", num_usable_guards);
    return 0;
  }

  /* We have enough usable guards: set MD restriction */
  return 1;
}

/** Allocate and return an outdated md guard restriction. Return NULL if no
 *  such restriction is needed. */
STATIC entry_guard_restriction_t *
guard_create_dirserver_md_restriction(void)
{
  entry_guard_restriction_t *rst = NULL;

  if (!should_set_md_dirserver_restriction()) {
    log_debug(LD_GUARD, "Not setting md restriction: too few "
              "filtered guards.");
    return NULL;
  }

  rst = tor_malloc_zero(sizeof(entry_guard_restriction_t));
  rst->type = RST_OUTDATED_MD_DIRSERVER;

  return rst;
}

/* Return True if <b>guard</b> obeys the exit restriction <b>rst</b>. */
static int
guard_obeys_exit_restriction(const entry_guard_t *guard,
                             const entry_guard_restriction_t *rst)
{
  tor_assert(rst->type == RST_EXIT_NODE);

  // Exclude the exit ID and all of its family.
  const node_t *node = node_get_by_id((const char*)rst->exclude_id);
  if (node && guard_in_node_family(guard, node))
    return 0;

  return tor_memneq(guard->identity, rst->exclude_id, DIGEST_LEN);
}

/** Return True if <b>guard</b> should be used as a dirserver for fetching
 *  microdescriptors. */
static int
guard_obeys_md_dirserver_restriction(const entry_guard_t *guard)
{
  /* If this guard is an outdated dirserver, don't use it. */
  if (microdesc_relay_is_outdated_dirserver(guard->identity)) {
    log_info(LD_GENERAL, "Skipping %s dirserver: outdated",
             hex_str(guard->identity, DIGEST_LEN));
    return 0;
  }

  log_debug(LD_GENERAL, "%s dirserver obeys md restrictions",
            hex_str(guard->identity, DIGEST_LEN));

  return 1;
}

/**
 * Return true iff <b>guard</b> obeys the restrictions defined in <b>rst</b>.
 * (If <b>rst</b> is NULL, there are no restrictions.)
 */
static int
entry_guard_obeys_restriction(const entry_guard_t *guard,
                              const entry_guard_restriction_t *rst)
{
  tor_assert(guard);
  if (! rst)
    return 1; // No restriction?  No problem.

  if (rst->type == RST_EXIT_NODE) {
    return guard_obeys_exit_restriction(guard, rst);
  } else if (rst->type == RST_OUTDATED_MD_DIRSERVER) {
    return guard_obeys_md_dirserver_restriction(guard);
  }

  tor_assert_nonfatal_unreached();
  return 0;
}

/**
 * Update the <b>is_filtered_guard</b> and <b>is_usable_filtered_guard</b>
 * flags on <b>guard</b>. */
void
entry_guard_set_filtered_flags(const or_options_t *options,
                               guard_selection_t *gs,
                               entry_guard_t *guard)
{
  unsigned was_filtered = guard->is_filtered_guard;
  guard->is_filtered_guard = 0;
  guard->is_usable_filtered_guard = 0;

  if (entry_guard_passes_filter(options, gs, guard)) {
    guard->is_filtered_guard = 1;

    if (guard->is_reachable != GUARD_REACHABLE_NO)
      guard->is_usable_filtered_guard = 1;

    entry_guard_consider_retry(guard);
  }
  log_debug(LD_GUARD, "Updated sampled guard %s: filtered=%d; "
            "reachable_filtered=%d.", entry_guard_describe(guard),
            guard->is_filtered_guard, guard->is_usable_filtered_guard);

  if (!bool_eq(was_filtered, guard->is_filtered_guard)) {
    /* This guard might now be primary or nonprimary. */
    gs->primary_guards_up_to_date = 0;
  }
}

/**
 * Update the <b>is_filtered_guard</b> and <b>is_usable_filtered_guard</b>
 * flag on every guard in <b>gs</b>. */
STATIC void
entry_guards_update_filtered_sets(guard_selection_t *gs)
{
  const or_options_t *options = get_options();

  SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, guard) {
    entry_guard_set_filtered_flags(options, gs, guard);
  } SMARTLIST_FOREACH_END(guard);
}

/**
 * Return a random guard from the reachable filtered sample guards
 * in <b>gs</b>, subject to the exclusion rules listed in <b>flags</b>.
 * Return NULL if no such guard can be found.
 *
 * Make sure that the sample is big enough, and that all the filter flags
 * are set correctly, before calling this function.
 *
 * If a restriction is provided in <b>rst</b>, do not return any guards that
 * violate it.
 **/
STATIC entry_guard_t *
sample_reachable_filtered_entry_guards(guard_selection_t *gs,
                                       const entry_guard_restriction_t *rst,
                                       unsigned flags)
{
  tor_assert(gs);
  entry_guard_t *result = NULL;
  const unsigned exclude_confirmed = flags & SAMPLE_EXCLUDE_CONFIRMED;
  const unsigned exclude_primary = flags & SAMPLE_EXCLUDE_PRIMARY;
  const unsigned exclude_pending = flags & SAMPLE_EXCLUDE_PENDING;
  const unsigned no_update_primary = flags & SAMPLE_NO_UPDATE_PRIMARY;
  const unsigned need_descriptor = flags & SAMPLE_EXCLUDE_NO_DESCRIPTOR;

  SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, guard) {
    entry_guard_consider_retry(guard);
  } SMARTLIST_FOREACH_END(guard);

  const int n_reachable_filtered = num_reachable_filtered_guards(gs, rst);

  log_info(LD_GUARD, "Trying to sample a reachable guard: We know of %d "
           "in the USABLE_FILTERED set.", n_reachable_filtered);

  const int min_filtered_sample = get_min_filtered_sample_size();
  if (n_reachable_filtered < min_filtered_sample) {
    log_info(LD_GUARD, "  (That isn't enough. Trying to expand the sample.)");
    entry_guards_expand_sample(gs);
  }

  if (exclude_primary && !gs->primary_guards_up_to_date && !no_update_primary)
    entry_guards_update_primary(gs);

  /* Build the set of reachable filtered guards. */
  smartlist_t *reachable_filtered_sample = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, guard) {
    entry_guard_consider_retry(guard);// redundant, but cheap.
    if (! entry_guard_obeys_restriction(guard, rst))
      continue;
    if (! guard->is_usable_filtered_guard)
      continue;
    if (exclude_confirmed && guard->confirmed_idx >= 0)
      continue;
    if (exclude_primary && guard->is_primary)
      continue;
    if (exclude_pending && guard->is_pending)
      continue;
    if (need_descriptor && !guard_has_descriptor(guard))
      continue;
    smartlist_add(reachable_filtered_sample, guard);
  } SMARTLIST_FOREACH_END(guard);

  log_info(LD_GUARD, "  (After filters [%x], we have %d guards to consider.)",
           flags, smartlist_len(reachable_filtered_sample));

  if (smartlist_len(reachable_filtered_sample)) {
    result = smartlist_choose(reachable_filtered_sample);
    log_info(LD_GUARD, "  (Selected %s.)",
             result ? entry_guard_describe(result) : "<null>");
  }
  smartlist_free(reachable_filtered_sample);

  return result;
}

/**
 * Helper: compare two entry_guard_t by their confirmed_idx values.
 * Used to sort the confirmed list.
 */
static int
compare_guards_by_confirmed_idx(const void **a_, const void **b_)
{
  const entry_guard_t *a = *a_, *b = *b_;
  if (a->confirmed_idx < b->confirmed_idx)
    return -1;
  else if (a->confirmed_idx > b->confirmed_idx)
    return 1;
  else
    return 0;
}

/**
 * Find the confirmed guards from among the sampled guards in <b>gs</b>,
 * and put them in confirmed_entry_guards in the correct
 * order. Recalculate their indices.
 */
STATIC void
entry_guards_update_confirmed(guard_selection_t *gs)
{
  smartlist_clear(gs->confirmed_entry_guards);
  SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, guard) {
    if (guard->confirmed_idx >= 0)
      smartlist_add(gs->confirmed_entry_guards, guard);
  } SMARTLIST_FOREACH_END(guard);

  smartlist_sort(gs->confirmed_entry_guards, compare_guards_by_confirmed_idx);

  int any_changed = 0;
  SMARTLIST_FOREACH_BEGIN(gs->confirmed_entry_guards, entry_guard_t *, guard) {
    if (guard->confirmed_idx != guard_sl_idx) {
      any_changed = 1;
      guard->confirmed_idx = guard_sl_idx;
    }
  } SMARTLIST_FOREACH_END(guard);

  gs->next_confirmed_idx = smartlist_len(gs->confirmed_entry_guards);

  if (any_changed) {
    entry_guards_changed_for_guard_selection(gs);
  }
}

/**
 * Mark <b>guard</b> as a confirmed guard -- that is, one that we have
 * connected to, and intend to use again.
 */
STATIC void
make_guard_confirmed(guard_selection_t *gs, entry_guard_t *guard)
{
  if (BUG(guard->confirmed_on_date && guard->confirmed_idx >= 0))
    return; // LCOV_EXCL_LINE

  if (BUG(smartlist_contains(gs->confirmed_entry_guards, guard)))
    return; // LCOV_EXCL_LINE

  const int GUARD_LIFETIME = get_guard_lifetime();
  guard->confirmed_on_date = randomize_time(approx_time(), GUARD_LIFETIME/10);

  log_info(LD_GUARD, "Marking %s as a confirmed guard (index %d)",
           entry_guard_describe(guard),
           gs->next_confirmed_idx);

  guard->confirmed_idx = gs->next_confirmed_idx++;
  smartlist_add(gs->confirmed_entry_guards, guard);

  // This confirmed guard might kick something else out of the primary
  // guards.
  gs->primary_guards_up_to_date = 0;

  entry_guards_changed_for_guard_selection(gs);
}

/**
 * Recalculate the list of primary guards (the ones we'd prefer to use) from
 * the filtered sample and the confirmed list.
 */
STATIC void
entry_guards_update_primary(guard_selection_t *gs)
{
  tor_assert(gs);

  // prevent recursion. Recursion is potentially very bad here.
  static int running = 0;
  tor_assert(!running);
  running = 1;

  const int N_PRIMARY_GUARDS = get_n_primary_guards();

  smartlist_t *new_primary_guards = smartlist_new();
  smartlist_t *old_primary_guards = smartlist_new();
  smartlist_add_all(old_primary_guards, gs->primary_entry_guards);

  /* Set this flag now, to prevent the calls below from recursing. */
  gs->primary_guards_up_to_date = 1;

  /* First, can we fill it up with confirmed guards? */
  SMARTLIST_FOREACH_BEGIN(gs->confirmed_entry_guards, entry_guard_t *, guard) {
    if (smartlist_len(new_primary_guards) >= N_PRIMARY_GUARDS)
      break;
    if (! guard->is_filtered_guard)
      continue;
    guard->is_primary = 1;
    smartlist_add(new_primary_guards, guard);
  } SMARTLIST_FOREACH_END(guard);

  SMARTLIST_FOREACH_BEGIN(old_primary_guards, entry_guard_t *, guard) {
    /* Can we keep any older primary guards? First remove all the ones
     * that we already kept. */
    if (smartlist_contains(new_primary_guards, guard)) {
      SMARTLIST_DEL_CURRENT_KEEPORDER(old_primary_guards, guard);
      continue;
    }

    /* Now add any that are still good. */
    if (smartlist_len(new_primary_guards) < N_PRIMARY_GUARDS &&
        guard->is_filtered_guard) {
      guard->is_primary = 1;
      smartlist_add(new_primary_guards, guard);
      SMARTLIST_DEL_CURRENT_KEEPORDER(old_primary_guards, guard);
    } else {
      /* Mark the remaining previous primary guards as non-primary */
      guard->is_primary = 0;
    }
  } SMARTLIST_FOREACH_END(guard);

  /* Finally, fill out the list with sampled guards. */
  while (smartlist_len(new_primary_guards) < N_PRIMARY_GUARDS) {
    entry_guard_t *guard = sample_reachable_filtered_entry_guards(gs, NULL,
                                            SAMPLE_EXCLUDE_CONFIRMED|
                                            SAMPLE_EXCLUDE_PRIMARY|
                                            SAMPLE_NO_UPDATE_PRIMARY);
    if (!guard)
      break;
    guard->is_primary = 1;
    smartlist_add(new_primary_guards, guard);
  }

#if 1
  /* Debugging. */
  SMARTLIST_FOREACH(gs->sampled_entry_guards, entry_guard_t *, guard, {
    tor_assert_nonfatal(
                   bool_eq(guard->is_primary,
                           smartlist_contains(new_primary_guards, guard)));
  });
#endif /* 1 */

  const int any_change = !smartlist_ptrs_eq(gs->primary_entry_guards,
                                            new_primary_guards);
  if (any_change) {
    log_info(LD_GUARD, "Primary entry guards have changed. "
             "New primary guard list is: ");
    int n = smartlist_len(new_primary_guards);
    SMARTLIST_FOREACH_BEGIN(new_primary_guards, entry_guard_t *, g) {
      log_info(LD_GUARD, "  %d/%d: %s%s%s",
               g_sl_idx+1, n, entry_guard_describe(g),
               g->confirmed_idx >= 0 ? " (confirmed)" : "",
               g->is_filtered_guard ? "" : " (excluded by filter)");
    } SMARTLIST_FOREACH_END(g);
  }

  smartlist_free(old_primary_guards);
  smartlist_free(gs->primary_entry_guards);
  gs->primary_entry_guards = new_primary_guards;
  gs->primary_guards_up_to_date = 1;
  running = 0;
}

/**
 * Return the number of seconds after the last attempt at which we should
 * retry a guard that has been failing since <b>failing_since</b>.
 */
static int
get_retry_schedule(time_t failing_since, time_t now,
                   int is_primary)
{
  const unsigned SIX_HOURS = 6 * 3600;
  const unsigned FOUR_DAYS = 4 * 86400;
  const unsigned SEVEN_DAYS = 7 * 86400;

  time_t tdiff;
  if (now > failing_since) {
    tdiff = now - failing_since;
  } else {
    tdiff = 0;
  }

  const struct {
    time_t maximum; int primary_delay; int nonprimary_delay;
  } delays[] = {
    { SIX_HOURS,    10*60,  1*60*60 },
    { FOUR_DAYS,    90*60,  4*60*60 },
    { SEVEN_DAYS, 4*60*60, 18*60*60 },
    { TIME_MAX,   9*60*60, 36*60*60 }
  };

  unsigned i;
  for (i = 0; i < ARRAY_LENGTH(delays); ++i) {
    if (tdiff <= delays[i].maximum) {
      return is_primary ? delays[i].primary_delay : delays[i].nonprimary_delay;
    }
  }
  /* LCOV_EXCL_START -- can't reach, since delays ends with TIME_MAX. */
  tor_assert_nonfatal_unreached();
  return 36*60*60;
  /* LCOV_EXCL_STOP */
}

/**
 * If <b>guard</b> is unreachable, consider whether enough time has passed
 * to consider it maybe-reachable again.
 */
STATIC void
entry_guard_consider_retry(entry_guard_t *guard)
{
  if (guard->is_reachable != GUARD_REACHABLE_NO)
    return; /* No retry needed. */

  const time_t now = approx_time();
  const int delay =
    get_retry_schedule(guard->failing_since, now, guard->is_primary);
  const time_t last_attempt = guard->last_tried_to_connect;

  if (BUG(last_attempt == 0) ||
      now >= last_attempt + delay) {
    /* We should mark this retriable. */
    char tbuf[ISO_TIME_LEN+1];
    format_local_iso_time(tbuf, last_attempt);
    log_info(LD_GUARD, "Marked %s%sguard %s for possible retry, since we "
             "haven't tried to use it since %s.",
             guard->is_primary?"primary ":"",
             guard->confirmed_idx>=0?"confirmed ":"",
             entry_guard_describe(guard),
             tbuf);

    guard->is_reachable = GUARD_REACHABLE_MAYBE;
    if (guard->is_filtered_guard)
      guard->is_usable_filtered_guard = 1;
  }
}

/** Tell the entry guards subsystem that we have confirmed that as of
 * just now, we're on the internet. */
void
entry_guards_note_internet_connectivity(guard_selection_t *gs)
{
  gs->last_time_on_internet = approx_time();
}

/**
 * Pick a primary guard for use with a circuit, if available. Update the
 * <b>last_tried_to_connect</b> time and the <b>is_pending</b> fields of the
 * guard as appropriate.  Set <b>state_out</b> to the new guard-state
 * of the circuit.
 */
static entry_guard_t *
select_primary_guard_for_circuit(guard_selection_t *gs,
                                 guard_usage_t usage,
                                 const entry_guard_restriction_t *rst,
                                 unsigned *state_out)
{
  const int need_descriptor = (usage == GUARD_USAGE_TRAFFIC);
  entry_guard_t *chosen_guard = NULL;

  int num_entry_guards = get_n_primary_guards_to_use(usage);
  smartlist_t *usable_primary_guards = smartlist_new();

  SMARTLIST_FOREACH_BEGIN(gs->primary_entry_guards, entry_guard_t *, guard) {
    entry_guard_consider_retry(guard);
    if (! entry_guard_obeys_restriction(guard, rst))
      continue;
    if (guard->is_reachable != GUARD_REACHABLE_NO) {
      if (need_descriptor && !guard_has_descriptor(guard)) {
        continue;
      }
      *state_out = GUARD_CIRC_STATE_USABLE_ON_COMPLETION;
      guard->last_tried_to_connect = approx_time();
      smartlist_add(usable_primary_guards, guard);
      if (smartlist_len(usable_primary_guards) >= num_entry_guards)
        break;
    }
  } SMARTLIST_FOREACH_END(guard);

  if (smartlist_len(usable_primary_guards)) {
    chosen_guard = smartlist_choose(usable_primary_guards);
    smartlist_free(usable_primary_guards);
    log_info(LD_GUARD, "Selected primary guard %s for circuit.",
             entry_guard_describe(chosen_guard));
  }

  smartlist_free(usable_primary_guards);
  return chosen_guard;
}

/**
 * For use with a circuit, pick a non-pending running filtered confirmed guard,
 * if one is available. Update the <b>last_tried_to_connect</b> time and the
 * <b>is_pending</b> fields of the guard as appropriate. Set <b>state_out</b>
 * to the new guard-state of the circuit.
 */
static entry_guard_t *
select_confirmed_guard_for_circuit(guard_selection_t *gs,
                                  guard_usage_t usage,
                                  const entry_guard_restriction_t *rst,
                                  unsigned *state_out)
{
  const int need_descriptor = (usage == GUARD_USAGE_TRAFFIC);

  SMARTLIST_FOREACH_BEGIN(gs->confirmed_entry_guards, entry_guard_t *, guard) {
    if (guard->is_primary)
      continue; /* we already considered this one. */
    if (! entry_guard_obeys_restriction(guard, rst))
      continue;
    entry_guard_consider_retry(guard);
    if (guard->is_usable_filtered_guard && ! guard->is_pending) {
      if (need_descriptor && !guard_has_descriptor(guard))
        continue; /* not a bug */
      guard->is_pending = 1;
      guard->last_tried_to_connect = approx_time();
      *state_out = GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD;
      log_info(LD_GUARD, "No primary guards available. Selected confirmed "
               "guard %s for circuit. Will try other guards before using "
               "this circuit.",
               entry_guard_describe(guard));
      return guard;
    }
  } SMARTLIST_FOREACH_END(guard);

  return NULL;
}

/**
 * For use with a circuit, pick a confirmed usable filtered guard
 * at random. Update the <b>last_tried_to_connect</b> time and the
 * <b>is_pending</b> fields of the guard as appropriate. Set <b>state_out</b>
 * to the new guard-state of the circuit.
 */
static entry_guard_t *
select_filtered_guard_for_circuit(guard_selection_t *gs,
                                  guard_usage_t usage,
                                  const entry_guard_restriction_t *rst,
                                  unsigned *state_out)
{
  const int need_descriptor = (usage == GUARD_USAGE_TRAFFIC);
  entry_guard_t *chosen_guard = NULL;
  unsigned flags = 0;
  if (need_descriptor)
    flags |= SAMPLE_EXCLUDE_NO_DESCRIPTOR;
  chosen_guard = sample_reachable_filtered_entry_guards(gs,
                                                 rst,
                                                 SAMPLE_EXCLUDE_CONFIRMED |
                                                 SAMPLE_EXCLUDE_PRIMARY |
                                                 SAMPLE_EXCLUDE_PENDING |
                                                 flags);
  if (!chosen_guard) {
    return NULL;
  }

  chosen_guard->is_pending = 1;
  chosen_guard->last_tried_to_connect = approx_time();
  *state_out = GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD;
  log_info(LD_GUARD, "No primary or confirmed guards available. Selected "
           "random guard %s for circuit. Will try other guards before "
           "using this circuit.",
           entry_guard_describe(chosen_guard));
  return chosen_guard;
}

/**
 * Get a guard for use with a circuit.  Prefer to pick a running primary
 * guard; then a non-pending running filtered confirmed guard; then a
 * non-pending runnable filtered guard.  Update the
 * <b>last_tried_to_connect</b> time and the <b>is_pending</b> fields of the
 * guard as appropriate.  Set <b>state_out</b> to the new guard-state
 * of the circuit.
 */
STATIC entry_guard_t *
select_entry_guard_for_circuit(guard_selection_t *gs,
                               guard_usage_t usage,
                               const entry_guard_restriction_t *rst,
                               unsigned *state_out)
{
  entry_guard_t *chosen_guard = NULL;
  tor_assert(gs);
  tor_assert(state_out);

  if (!gs->primary_guards_up_to_date)
    entry_guards_update_primary(gs);

  /* "If any entry in PRIMARY_GUARDS has {is_reachable} status of
      <maybe> or <yes>, return the first such guard." */
  chosen_guard = select_primary_guard_for_circuit(gs, usage, rst, state_out);
  if (chosen_guard)
    return chosen_guard;

  /* "Otherwise, if the ordered intersection of {CONFIRMED_GUARDS}
      and {USABLE_FILTERED_GUARDS} is nonempty, return the first
      entry in that intersection that has {is_pending} set to
      false." */
  chosen_guard = select_confirmed_guard_for_circuit(gs, usage, rst, state_out);
  if (chosen_guard)
    return chosen_guard;

  /* "Otherwise, if there is no such entry, select a member at
      random from {USABLE_FILTERED_GUARDS}." */
  chosen_guard = select_filtered_guard_for_circuit(gs, usage, rst, state_out);

  if (chosen_guard == NULL) {
    log_info(LD_GUARD, "Absolutely no sampled guards were available. "
             "Marking all guards for retry and starting from top again.");
    mark_all_guards_maybe_reachable(gs);
    return NULL;
  }

  return chosen_guard;
}

/**
 * Note that we failed to connect to or build circuits through <b>guard</b>.
 * Use with a guard returned by select_entry_guard_for_circuit().
 */
STATIC void
entry_guards_note_guard_failure(guard_selection_t *gs,
                                entry_guard_t *guard)
{
  tor_assert(gs);

  guard->is_reachable = GUARD_REACHABLE_NO;
  guard->is_usable_filtered_guard = 0;

  guard->is_pending = 0;
  if (guard->failing_since == 0)
    guard->failing_since = approx_time();

  log_info(LD_GUARD, "Recorded failure for %s%sguard %s",
           guard->is_primary?"primary ":"",
           guard->confirmed_idx>=0?"confirmed ":"",
           entry_guard_describe(guard));
}

/**
 * Note that we successfully connected to, and built a circuit through
 * <b>guard</b>. Given the old guard-state of the circuit in <b>old_state</b>,
 * return the new guard-state of the circuit.
 *
 * Be aware: the circuit is only usable when its guard-state becomes
 * GUARD_CIRC_STATE_COMPLETE.
 **/
STATIC unsigned
entry_guards_note_guard_success(guard_selection_t *gs,
                                entry_guard_t *guard,
                                unsigned old_state)
{
  tor_assert(gs);

  /* Save this, since we're about to overwrite it. */
  const time_t last_time_on_internet = gs->last_time_on_internet;
  gs->last_time_on_internet = approx_time();

  guard->is_reachable = GUARD_REACHABLE_YES;
  guard->failing_since = 0;
  guard->is_pending = 0;
  if (guard->is_filtered_guard)
    guard->is_usable_filtered_guard = 1;

  if (guard->confirmed_idx < 0) {
    make_guard_confirmed(gs, guard);
    if (!gs->primary_guards_up_to_date)
      entry_guards_update_primary(gs);
  }

  unsigned new_state;
  switch (old_state) {
    case GUARD_CIRC_STATE_COMPLETE:
    case GUARD_CIRC_STATE_USABLE_ON_COMPLETION:
      new_state = GUARD_CIRC_STATE_COMPLETE;
      break;
    default:
      tor_assert_nonfatal_unreached();
      /* Fall through. */
    case GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD:
      if (guard->is_primary) {
        /* XXXX #20832 -- I don't actually like this logic. It seems to make
         * us a little more susceptible to evil-ISP attacks.  The mitigations
         * I'm thinking of, however, aren't local to this point, so I'll leave
         * it alone. */
        /* This guard may have become primary by virtue of being confirmed.
         * If so, the circuit for it is now complete.
         */
        new_state = GUARD_CIRC_STATE_COMPLETE;
      } else {
        new_state = GUARD_CIRC_STATE_WAITING_FOR_BETTER_GUARD;
      }
      break;
  }

  if (! guard->is_primary) {
    if (last_time_on_internet + get_internet_likely_down_interval()
        < approx_time()) {
      mark_primary_guards_maybe_reachable(gs);
    }
  }

  log_info(LD_GUARD, "Recorded success for %s%sguard %s",
           guard->is_primary?"primary ":"",
           guard->confirmed_idx>=0?"confirmed ":"",
           entry_guard_describe(guard));

  return new_state;
}

/**
 * Helper: Return true iff <b>a</b> has higher priority than <b>b</b>.
 */
STATIC int
entry_guard_has_higher_priority(entry_guard_t *a, entry_guard_t *b)
{
  tor_assert(a && b);
  if (a == b)
    return 0;

  /* Confirmed is always better than unconfirmed; lower index better
     than higher */
  if (a->confirmed_idx < 0) {
    if (b->confirmed_idx >= 0)
      return 0;
  } else {
    if (b->confirmed_idx < 0)
      return 1;

    /* Lower confirmed_idx is better than higher. */
    return (a->confirmed_idx < b->confirmed_idx);
  }

  /* If we reach this point, both are unconfirmed. If one is pending, it
   * has higher priority. */
  if (a->is_pending) {
    if (! b->is_pending)
      return 1;

    /* Both are pending: earlier last_tried_connect wins. */
    return a->last_tried_to_connect < b->last_tried_to_connect;
  } else {
    if (b->is_pending)
      return 0;

    /* Neither is pending: priorities are equal. */
    return 0;
  }
}

/** Release all storage held in <b>restriction</b> */
STATIC void
entry_guard_restriction_free_(entry_guard_restriction_t *rst)
{
  tor_free(rst);
}

/**
 * Release all storage held in <b>state</b>.
 */
void
circuit_guard_state_free_(circuit_guard_state_t *state)
{
  if (!state)
    return;
  entry_guard_restriction_free(state->restrictions);
  entry_guard_handle_free(state->guard);
  tor_free(state);
}

/** Allocate and return a new circuit_guard_state_t to track the result
 * of using <b>guard</b> for a given operation. */
MOCK_IMPL(STATIC circuit_guard_state_t *,
circuit_guard_state_new,(entry_guard_t *guard, unsigned state,
                         entry_guard_restriction_t *rst))
{
  circuit_guard_state_t *result;

  result = tor_malloc_zero(sizeof(circuit_guard_state_t));
  result->guard = entry_guard_handle_new(guard);
  result->state = state;
  result->state_set_at = approx_time();
  result->restrictions = rst;

  return result;
}

/**
 * Pick a suitable entry guard for a circuit in, and place that guard
 * in *<b>chosen_node_out</b>. Set *<b>guard_state_out</b> to an opaque
 * state object that will record whether the circuit is ready to be used
 * or not. Return 0 on success; on failure, return -1.
 *
 * If a restriction is provided in <b>rst</b>, do not return any guards that
 * violate it, and remember that restriction in <b>guard_state_out</b> for
 * later use. (Takes ownership of the <b>rst</b> object.)
 */
int
entry_guard_pick_for_circuit(guard_selection_t *gs,
                             guard_usage_t usage,
                             entry_guard_restriction_t *rst,
                             const node_t **chosen_node_out,
                             circuit_guard_state_t **guard_state_out)
{
  tor_assert(gs);
  tor_assert(chosen_node_out);
  tor_assert(guard_state_out);
  *chosen_node_out = NULL;
  *guard_state_out = NULL;

  unsigned state = 0;
  entry_guard_t *guard =
    select_entry_guard_for_circuit(gs, usage, rst, &state);
  if (! guard)
    goto fail;
  if (BUG(state == 0))
    goto fail;
  const node_t *node = node_get_by_id(guard->identity);
  // XXXX #20827 check Ed ID.
  if (! node)
    goto fail;
  if (BUG(usage != GUARD_USAGE_DIRGUARD &&
          !node_has_preferred_descriptor(node, 1)))
    goto fail;

  *chosen_node_out = node;
  *guard_state_out = circuit_guard_state_new(guard, state, rst);

  return 0;
 fail:
  entry_guard_restriction_free(rst);
  return -1;
}

/**
 * Called by the circuit building module when a circuit has succeeded: informs
 * the guards code that the guard in *<b>guard_state_p</b> is working, and
 * advances the state of the guard module.  On a GUARD_USABLE_NEVER return
 * value, the circuit is broken and should not be used.  On a GUARD_USABLE_NOW
 * return value, the circuit is ready to use.  On a GUARD_MAYBE_USABLE_LATER
 * return value, the circuit should not be used until we find out whether
 * preferred guards will work for us.
 */
guard_usable_t
entry_guard_succeeded(circuit_guard_state_t **guard_state_p)
{
  if (BUG(*guard_state_p == NULL))
    return GUARD_USABLE_NEVER;

  entry_guard_t *guard = entry_guard_handle_get((*guard_state_p)->guard);
  if (! guard || BUG(guard->in_selection == NULL))
    return GUARD_USABLE_NEVER;

  unsigned newstate =
    entry_guards_note_guard_success(guard->in_selection, guard,
                                    (*guard_state_p)->state);

  (*guard_state_p)->state = newstate;
  (*guard_state_p)->state_set_at = approx_time();

  if (newstate == GUARD_CIRC_STATE_COMPLETE) {
    return GUARD_USABLE_NOW;
  } else {
    return GUARD_MAYBE_USABLE_LATER;
  }
}

/** Cancel the selection of *<b>guard_state_p</b> without declaring
 * success or failure. It is safe to call this function if success or
 * failure _has_ already been declared. */
void
entry_guard_cancel(circuit_guard_state_t **guard_state_p)
{
  if (BUG(*guard_state_p == NULL))
    return;
  entry_guard_t *guard = entry_guard_handle_get((*guard_state_p)->guard);
  if (! guard)
    return;

  /* XXXX prop271 -- last_tried_to_connect_at will be erroneous here, but this
   * function will only get called in "bug" cases anyway. */
  guard->is_pending = 0;
  circuit_guard_state_free(*guard_state_p);
  *guard_state_p = NULL;
}

/**
 * Called by the circuit building module when a circuit has failed:
 * informs the guards code that the guard in *<b>guard_state_p</b> is
 * not working, and advances the state of the guard module.
 */
void
entry_guard_failed(circuit_guard_state_t **guard_state_p)
{
  if (BUG(*guard_state_p == NULL))
    return;

  entry_guard_t *guard = entry_guard_handle_get((*guard_state_p)->guard);
  if (! guard || BUG(guard->in_selection == NULL))
    return;

  entry_guards_note_guard_failure(guard->in_selection, guard);

  (*guard_state_p)->state = GUARD_CIRC_STATE_DEAD;
  (*guard_state_p)->state_set_at = approx_time();
}

/**
 * Run the entry_guard_failed() function on every circuit that is
 * pending on <b>chan</b>.
 */
void
entry_guard_chan_failed(channel_t *chan)
{
  if (!chan)
    return;

  smartlist_t *pending = smartlist_new();
  circuit_get_all_pending_on_channel(pending, chan);
  SMARTLIST_FOREACH_BEGIN(pending, circuit_t *, circ) {
    if (!CIRCUIT_IS_ORIGIN(circ))
      continue;

    origin_circuit_t *origin_circ = TO_ORIGIN_CIRCUIT(circ);
    if (origin_circ->guard_state) {
      /* We might have no guard state if we didn't use a guard on this
       * circuit (eg it's for a fallback directory). */
      entry_guard_failed(&origin_circ->guard_state);
    }
  } SMARTLIST_FOREACH_END(circ);
  smartlist_free(pending);
}

/**
 * Return true iff every primary guard in <b>gs</b> is believed to
 * be unreachable.
 */
STATIC int
entry_guards_all_primary_guards_are_down(guard_selection_t *gs)
{
  tor_assert(gs);
  if (!gs->primary_guards_up_to_date)
    entry_guards_update_primary(gs);
  SMARTLIST_FOREACH_BEGIN(gs->primary_entry_guards, entry_guard_t *, guard) {
    entry_guard_consider_retry(guard);
    if (guard->is_reachable != GUARD_REACHABLE_NO)
      return 0;
  } SMARTLIST_FOREACH_END(guard);
  return 1;
}

/** Wrapper for entry_guard_has_higher_priority that compares the
 * guard-priorities of a pair of circuits. Return 1 if <b>a</b> has higher
 * priority than <b>b</b>.
 *
 * If a restriction is provided in <b>rst</b>, then do not consider
 * <b>a</b> to have higher priority if it violates the restriction.
 */
static int
circ_state_has_higher_priority(origin_circuit_t *a,
                               const entry_guard_restriction_t *rst,
                               origin_circuit_t *b)
{
  circuit_guard_state_t *state_a = origin_circuit_get_guard_state(a);
  circuit_guard_state_t *state_b = origin_circuit_get_guard_state(b);

  tor_assert(state_a);
  tor_assert(state_b);

  entry_guard_t *guard_a = entry_guard_handle_get(state_a->guard);
  entry_guard_t *guard_b = entry_guard_handle_get(state_b->guard);

  if (! guard_a) {
    /* Unknown guard -- never higher priority. */
    return 0;
  } else if (! guard_b) {
    /* Known guard -- higher priority than any unknown guard. */
    return 1;
  } else  if (! entry_guard_obeys_restriction(guard_a, rst)) {
    /* Restriction violated; guard_a cannot have higher priority. */
    return 0;
  } else {
    /* Both known -- compare.*/
    return entry_guard_has_higher_priority(guard_a, guard_b);
  }
}

/**
 * Look at all of the origin_circuit_t * objects in <b>all_circuits_in</b>,
 * and see if any of them that were previously not ready to use for
 * guard-related reasons are now ready to use. Place those circuits
 * in <b>newly_complete_out</b>, and mark them COMPLETE.
 *
 * Return 1 if we upgraded any circuits, and 0 otherwise.
 */
int
entry_guards_upgrade_waiting_circuits(guard_selection_t *gs,
                                      const smartlist_t *all_circuits_in,
                                      smartlist_t *newly_complete_out)
{
  tor_assert(gs);
  tor_assert(all_circuits_in);
  tor_assert(newly_complete_out);

  if (! entry_guards_all_primary_guards_are_down(gs)) {
    /* We only upgrade a waiting circuit if the primary guards are all
     * down. */
    log_debug(LD_GUARD, "Considered upgrading guard-stalled circuits, "
              "but not all primary guards were definitely down.");
    return 0;
  }

  int n_waiting = 0;
  int n_complete = 0;
  int n_complete_blocking = 0;
  origin_circuit_t *best_waiting_circuit = NULL;
  smartlist_t *all_circuits = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(all_circuits_in, origin_circuit_t *, circ) {
    // We filter out circuits that aren't ours, or which we can't
    // reason about.
    circuit_guard_state_t *state = origin_circuit_get_guard_state(circ);
    if (state == NULL)
      continue;
    entry_guard_t *guard = entry_guard_handle_get(state->guard);
    if (!guard || guard->in_selection != gs)
      continue;

    smartlist_add(all_circuits, circ);
  } SMARTLIST_FOREACH_END(circ);

  SMARTLIST_FOREACH_BEGIN(all_circuits, origin_circuit_t *, circ) {
    circuit_guard_state_t *state = origin_circuit_get_guard_state(circ);
    if (BUG(state == NULL))
      continue;

    if (state->state == GUARD_CIRC_STATE_WAITING_FOR_BETTER_GUARD) {
      ++n_waiting;
      if (! best_waiting_circuit ||
          circ_state_has_higher_priority(circ, NULL, best_waiting_circuit)) {
        best_waiting_circuit = circ;
      }
    }
  } SMARTLIST_FOREACH_END(circ);

  if (! best_waiting_circuit) {
    log_debug(LD_GUARD, "Considered upgrading guard-stalled circuits, "
              "but didn't find any.");
    goto no_change;
  }

  /* We'll need to keep track of what restrictions were used when picking this
   * circuit, so that we don't allow any circuit without those restrictions to
   * block it. */
  const entry_guard_restriction_t *rst_on_best_waiting =
    origin_circuit_get_guard_state(best_waiting_circuit)->restrictions;

  /* First look at the complete circuits: Do any block this circuit? */
  SMARTLIST_FOREACH_BEGIN(all_circuits, origin_circuit_t *, circ) {
    /* "C2 "blocks" C1 if:
        * C2 obeys all the restrictions that C1 had to obey, AND
        * C2 has higher priority than C1, AND
        * Either C2 is <complete>, or C2 is <waiting_for_better_guard>,
          or C2 has been <usable_if_no_better_guard> for no more than
          {NONPRIMARY_GUARD_CONNECT_TIMEOUT} seconds."
    */
    circuit_guard_state_t *state = origin_circuit_get_guard_state(circ);
    if BUG((state == NULL))
      continue;
    if (state->state != GUARD_CIRC_STATE_COMPLETE)
      continue;
    ++n_complete;
    if (circ_state_has_higher_priority(circ, rst_on_best_waiting,
                                       best_waiting_circuit))
      ++n_complete_blocking;
  } SMARTLIST_FOREACH_END(circ);

  if (n_complete_blocking) {
    log_debug(LD_GUARD, "Considered upgrading guard-stalled circuits: found "
              "%d complete and %d guard-stalled. At least one complete "
              "circuit had higher priority, so not upgrading.",
              n_complete, n_waiting);
    goto no_change;
  }

  /* " * If any circuit C1 is <waiting_for_better_guard>, AND:
          * All primary guards have reachable status of <no>.
          * There is no circuit C2 that "blocks" C1.
         Then, upgrade C1 to <complete>.""
  */
  int n_blockers_found = 0;
  const time_t state_set_at_cutoff =
    approx_time() - get_nonprimary_guard_connect_timeout();
  SMARTLIST_FOREACH_BEGIN(all_circuits, origin_circuit_t *, circ) {
    circuit_guard_state_t *state = origin_circuit_get_guard_state(circ);
    if (BUG(state == NULL))
      continue;
    if (state->state != GUARD_CIRC_STATE_USABLE_IF_NO_BETTER_GUARD)
      continue;
    if (state->state_set_at <= state_set_at_cutoff)
      continue;
    if (circ_state_has_higher_priority(circ, rst_on_best_waiting,
                                       best_waiting_circuit))
      ++n_blockers_found;
  } SMARTLIST_FOREACH_END(circ);

  if (n_blockers_found) {
    log_debug(LD_GUARD, "Considered upgrading guard-stalled circuits: found "
              "%d guard-stalled, but %d pending circuit(s) had higher "
              "guard priority, so not upgrading.",
              n_waiting, n_blockers_found);
    goto no_change;
  }

  /* Okay. We have a best waiting circuit, and we aren't waiting for
     anything better.  Add all circuits with that priority to the
     list, and call them COMPLETE. */
  int n_succeeded = 0;
  SMARTLIST_FOREACH_BEGIN(all_circuits, origin_circuit_t *, circ) {
    circuit_guard_state_t *state = origin_circuit_get_guard_state(circ);
    if (BUG(state == NULL))
      continue;
    if (circ != best_waiting_circuit && rst_on_best_waiting) {
      /* Can't upgrade other circ with same priority as best; might
         be blocked. */
      continue;
    }
    if (state->state != GUARD_CIRC_STATE_WAITING_FOR_BETTER_GUARD)
      continue;
    if (circ_state_has_higher_priority(best_waiting_circuit, NULL, circ))
      continue;

    state->state = GUARD_CIRC_STATE_COMPLETE;
    state->state_set_at = approx_time();
    smartlist_add(newly_complete_out, circ);
    ++n_succeeded;
  } SMARTLIST_FOREACH_END(circ);

  log_info(LD_GUARD, "Considered upgrading guard-stalled circuits: found "
           "%d guard-stalled, %d complete. %d of the guard-stalled "
           "circuit(s) had high enough priority to upgrade.",
           n_waiting, n_complete, n_succeeded);

  tor_assert_nonfatal(n_succeeded >= 1);
  smartlist_free(all_circuits);
  return 1;

 no_change:
  smartlist_free(all_circuits);
  return 0;
}

/**
 * Return true iff the circuit whose state is <b>guard_state</b> should
 * expire.
 */
int
entry_guard_state_should_expire(circuit_guard_state_t *guard_state)
{
  if (guard_state == NULL)
    return 0;
  const time_t expire_if_waiting_since =
    approx_time() - get_nonprimary_guard_idle_timeout();
  return (guard_state->state == GUARD_CIRC_STATE_WAITING_FOR_BETTER_GUARD
          && guard_state->state_set_at < expire_if_waiting_since);
}

/**
 * Update all derived pieces of the guard selection state in <b>gs</b>.
 * Return true iff we should stop using all previously generated circuits.
 */
int
entry_guards_update_all(guard_selection_t *gs)
{
  sampled_guards_update_from_consensus(gs);
  entry_guards_update_filtered_sets(gs);
  entry_guards_update_confirmed(gs);
  entry_guards_update_primary(gs);
  return 0;
}

/**
 * Return a newly allocated string for encoding the persistent parts of
 * <b>guard</b> to the state file.
 */
STATIC char *
entry_guard_encode_for_state(entry_guard_t *guard)
{
  /*
   * The meta-format we use is K=V K=V K=V... where K can be any
   * characters excepts space and =, and V can be any characters except
   * space.  The order of entries is not allowed to matter.
   * Unrecognized K=V entries are persisted; recognized but erroneous
   * entries are corrected.
   */

  smartlist_t *result = smartlist_new();
  char tbuf[ISO_TIME_LEN+1];

  tor_assert(guard);

  smartlist_add_asprintf(result, "in=%s", guard->selection_name);
  smartlist_add_asprintf(result, "rsa_id=%s",
                         hex_str(guard->identity, DIGEST_LEN));
  if (guard->bridge_addr) {
    smartlist_add_asprintf(result, "bridge_addr=%s:%d",
                           fmt_and_decorate_addr(&guard->bridge_addr->addr),
                           guard->bridge_addr->port);
  }
  if (strlen(guard->nickname) && is_legal_nickname(guard->nickname)) {
    smartlist_add_asprintf(result, "nickname=%s", guard->nickname);
  }

  format_iso_time_nospace(tbuf, guard->sampled_on_date);
  smartlist_add_asprintf(result, "sampled_on=%s", tbuf);

  if (guard->sampled_by_version) {
    smartlist_add_asprintf(result, "sampled_by=%s",
                           guard->sampled_by_version);
  }

  if (guard->unlisted_since_date > 0) {
    format_iso_time_nospace(tbuf, guard->unlisted_since_date);
    smartlist_add_asprintf(result, "unlisted_since=%s", tbuf);
  }

  smartlist_add_asprintf(result, "listed=%d",
                         (int)guard->currently_listed);

  if (guard->confirmed_idx >= 0) {
    format_iso_time_nospace(tbuf, guard->confirmed_on_date);
    smartlist_add_asprintf(result, "confirmed_on=%s", tbuf);

    smartlist_add_asprintf(result, "confirmed_idx=%d", guard->confirmed_idx);
  }

  const double EPSILON = 1.0e-6;

  /* Make a copy of the pathbias object, since we will want to update
     some of them */
  guard_pathbias_t *pb = tor_memdup(&guard->pb, sizeof(*pb));
  pb->use_successes = pathbias_get_use_success_count(guard);
  pb->successful_circuits_closed = pathbias_get_close_success_count(guard);

  #define PB_FIELD(field) do {                                          \
      if (pb->field >= EPSILON) {                                       \
        smartlist_add_asprintf(result, "pb_" #field "=%f", pb->field);  \
      }                                                                 \
    } while (0)
  PB_FIELD(use_attempts);
  PB_FIELD(use_successes);
  PB_FIELD(circ_attempts);
  PB_FIELD(circ_successes);
  PB_FIELD(successful_circuits_closed);
  PB_FIELD(collapsed_circuits);
  PB_FIELD(unusable_circuits);
  PB_FIELD(timeouts);
  tor_free(pb);
#undef PB_FIELD

  if (guard->extra_state_fields)
    smartlist_add_strdup(result, guard->extra_state_fields);

  char *joined = smartlist_join_strings(result, " ", 0, NULL);
  SMARTLIST_FOREACH(result, char *, cp, tor_free(cp));
  smartlist_free(result);

  return joined;
}

/**
 * Given a string generated by entry_guard_encode_for_state(), parse it
 * (if possible) and return an entry_guard_t object for it.  Return NULL
 * on complete failure.
 */
STATIC entry_guard_t *
entry_guard_parse_from_state(const char *s)
{
  /* Unrecognized entries get put in here. */
  smartlist_t *extra = smartlist_new();

  /* These fields get parsed from the string. */
  char *in = NULL;
  char *rsa_id = NULL;
  char *nickname = NULL;
  char *sampled_on = NULL;
  char *sampled_by = NULL;
  char *unlisted_since = NULL;
  char *listed  = NULL;
  char *confirmed_on = NULL;
  char *confirmed_idx = NULL;
  char *bridge_addr = NULL;

  // pathbias
  char *pb_use_attempts = NULL;
  char *pb_use_successes = NULL;
  char *pb_circ_attempts = NULL;
  char *pb_circ_successes = NULL;
  char *pb_successful_circuits_closed = NULL;
  char *pb_collapsed_circuits = NULL;
  char *pb_unusable_circuits = NULL;
  char *pb_timeouts = NULL;

  /* Split up the entries.  Put the ones we know about in strings and the
   * rest in "extra". */
  {
    smartlist_t *entries = smartlist_new();

    strmap_t *vals = strmap_new(); // Maps keyword to location
#define FIELD(f) \
    strmap_set(vals, #f, &f);
    FIELD(in);
    FIELD(rsa_id);
    FIELD(nickname);
    FIELD(sampled_on);
    FIELD(sampled_by);
    FIELD(unlisted_since);
    FIELD(listed);
    FIELD(confirmed_on);
    FIELD(confirmed_idx);
    FIELD(bridge_addr);
    FIELD(pb_use_attempts);
    FIELD(pb_use_successes);
    FIELD(pb_circ_attempts);
    FIELD(pb_circ_successes);
    FIELD(pb_successful_circuits_closed);
    FIELD(pb_collapsed_circuits);
    FIELD(pb_unusable_circuits);
    FIELD(pb_timeouts);
#undef FIELD

    smartlist_split_string(entries, s, " ",
                           SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);

    SMARTLIST_FOREACH_BEGIN(entries, char *, entry) {
      const char *eq = strchr(entry, '=');
      if (!eq) {
        smartlist_add(extra, entry);
        continue;
      }
      char *key = tor_strndup(entry, eq-entry);
      char **target = strmap_get(vals, key);
      if (target == NULL || *target != NULL) {
        /* unrecognized or already set */
        smartlist_add(extra, entry);
        tor_free(key);
        continue;
      }

      *target = tor_strdup(eq+1);
      tor_free(key);
      tor_free(entry);
    } SMARTLIST_FOREACH_END(entry);

    smartlist_free(entries);
    strmap_free(vals, NULL);
  }

  entry_guard_t *guard = tor_malloc_zero(sizeof(entry_guard_t));
  guard->is_persistent = 1;

  if (in == NULL) {
    log_warn(LD_CIRC, "Guard missing 'in' field");
    goto err;
  }

  guard->selection_name = in;
  in = NULL;

  if (rsa_id == NULL) {
    log_warn(LD_CIRC, "Guard missing RSA ID field");
    goto err;
  }

  /* Process the identity and nickname. */
  if (base16_decode(guard->identity, sizeof(guard->identity),
                    rsa_id, strlen(rsa_id)) != DIGEST_LEN) {
    log_warn(LD_CIRC, "Unable to decode guard identity %s", escaped(rsa_id));
    goto err;
  }

  if (nickname) {
    strlcpy(guard->nickname, nickname, sizeof(guard->nickname));
  } else {
    guard->nickname[0]='$';
    base16_encode(guard->nickname+1, sizeof(guard->nickname)-1,
                  guard->identity, DIGEST_LEN);
  }

  if (bridge_addr) {
    tor_addr_port_t res;
    memset(&res, 0, sizeof(res));
    int r = tor_addr_port_parse(LOG_WARN, bridge_addr,
                                &res.addr, &res.port, -1);
    if (r == 0)
      guard->bridge_addr = tor_memdup(&res, sizeof(res));
    /* On error, we already warned. */
  }

  /* Process the various time fields. */

#define HANDLE_TIME(field) do {                                 \
    if (field) {                                                \
      int r = parse_iso_time_nospace(field, &field ## _time);   \
      if (r < 0) {                                              \
        log_warn(LD_CIRC, "Unable to parse %s %s from guard",   \
                 #field, escaped(field));                       \
        field##_time = -1;                                      \
      }                                                         \
    }                                                           \
  } while (0)

  time_t sampled_on_time = 0;
  time_t unlisted_since_time = 0;
  time_t confirmed_on_time = 0;

  HANDLE_TIME(sampled_on);
  HANDLE_TIME(unlisted_since);
  HANDLE_TIME(confirmed_on);

  if (sampled_on_time <= 0)
    sampled_on_time = approx_time();
  if (unlisted_since_time < 0)
    unlisted_since_time = 0;
  if (confirmed_on_time < 0)
    confirmed_on_time = 0;

  #undef HANDLE_TIME

  guard->sampled_on_date = sampled_on_time;
  guard->unlisted_since_date = unlisted_since_time;
  guard->confirmed_on_date = confirmed_on_time;

  /* Take sampled_by_version verbatim. */
  guard->sampled_by_version = sampled_by;
  sampled_by = NULL; /* prevent free */

  /* Listed is a boolean */
  if (listed && strcmp(listed, "0"))
    guard->currently_listed = 1;

  /* The index is a nonnegative integer. */
  guard->confirmed_idx = -1;
  if (confirmed_idx) {
    int ok=1;
    long idx = tor_parse_long(confirmed_idx, 10, 0, INT_MAX, &ok, NULL);
    if (! ok) {
      log_warn(LD_GUARD, "Guard has invalid confirmed_idx %s",
               escaped(confirmed_idx));
    } else {
      guard->confirmed_idx = (int)idx;
    }
  }

  /* Anything we didn't recognize gets crammed together */
  if (smartlist_len(extra) > 0) {
    guard->extra_state_fields = smartlist_join_strings(extra, " ", 0, NULL);
  }

  /* initialize non-persistent fields */
  guard->is_reachable = GUARD_REACHABLE_MAYBE;

#define PB_FIELD(field)                                                 \
  do {                                                                  \
    if (pb_ ## field) {                                                 \
      int ok = 1;                                                       \
      double r = tor_parse_double(pb_ ## field, 0.0, 1e9, &ok, NULL);   \
      if (! ok) {                                                       \
        log_warn(LD_CIRC, "Guard has invalid pb_%s %s",                 \
                 #field, pb_ ## field);                                 \
      } else {                                                          \
        guard->pb.field = r;                                            \
      }                                                                 \
    }                                                                   \
  } while (0)
  PB_FIELD(use_attempts);
  PB_FIELD(use_successes);
  PB_FIELD(circ_attempts);
  PB_FIELD(circ_successes);
  PB_FIELD(successful_circuits_closed);
  PB_FIELD(collapsed_circuits);
  PB_FIELD(unusable_circuits);
  PB_FIELD(timeouts);
#undef PB_FIELD

  pathbias_check_use_success_count(guard);
  pathbias_check_close_success_count(guard);

  /* We update everything on this guard later, after we've parsed
   * everything.  */

  goto done;

 err:
  // only consider it an error if the guard state was totally unparseable.
  entry_guard_free(guard);
  guard = NULL;

 done:
  tor_free(in);
  tor_free(rsa_id);
  tor_free(nickname);
  tor_free(sampled_on);
  tor_free(sampled_by);
  tor_free(unlisted_since);
  tor_free(listed);
  tor_free(confirmed_on);
  tor_free(confirmed_idx);
  tor_free(bridge_addr);
  tor_free(pb_use_attempts);
  tor_free(pb_use_successes);
  tor_free(pb_circ_attempts);
  tor_free(pb_circ_successes);
  tor_free(pb_successful_circuits_closed);
  tor_free(pb_collapsed_circuits);
  tor_free(pb_unusable_circuits);
  tor_free(pb_timeouts);

  SMARTLIST_FOREACH(extra, char *, cp, tor_free(cp));
  smartlist_free(extra);

  return guard;
}

/**
 * Replace the Guards entries in <b>state</b> with a list of all our sampled
 * guards.
 */
static void
entry_guards_update_guards_in_state(or_state_t *state)
{
  if (!guard_contexts)
    return;
  config_line_t *lines = NULL;
  config_line_t **nextline = &lines;

  SMARTLIST_FOREACH_BEGIN(guard_contexts, guard_selection_t *, gs) {
    SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, guard) {
      if (guard->is_persistent == 0)
        continue;
      *nextline = tor_malloc_zero(sizeof(config_line_t));
      (*nextline)->key = tor_strdup("Guard");
      (*nextline)->value = entry_guard_encode_for_state(guard);
      nextline = &(*nextline)->next;
    } SMARTLIST_FOREACH_END(guard);
  } SMARTLIST_FOREACH_END(gs);

  config_free_lines(state->Guard);
  state->Guard = lines;
}

/**
 * Replace our sampled guards from the Guards entries in <b>state</b>. Return 0
 * on success, -1 on failure. (If <b>set</b> is true, replace nothing -- only
 * check whether replacing would work.)
 */
static int
entry_guards_load_guards_from_state(or_state_t *state, int set)
{
  const config_line_t *line = state->Guard;
  int n_errors = 0;

  if (!guard_contexts)
    guard_contexts = smartlist_new();

  /* Wipe all our existing guard info. (we shouldn't have any, but
   * let's be safe.) */
  if (set) {
    SMARTLIST_FOREACH_BEGIN(guard_contexts, guard_selection_t *, gs) {
      guard_selection_free(gs);
      if (curr_guard_context == gs)
        curr_guard_context = NULL;
      SMARTLIST_DEL_CURRENT(guard_contexts, gs);
    } SMARTLIST_FOREACH_END(gs);
  }

  for ( ; line != NULL; line = line->next) {
    entry_guard_t *guard = entry_guard_parse_from_state(line->value);
    if (guard == NULL) {
      ++n_errors;
      continue;
    }
    tor_assert(guard->selection_name);
    if (!strcmp(guard->selection_name, "legacy")) {
      ++n_errors;
      entry_guard_free(guard);
      continue;
    }

    if (set) {
      guard_selection_t *gs;
      gs = get_guard_selection_by_name(guard->selection_name,
                                       GS_TYPE_INFER, 1);
      tor_assert(gs);
      smartlist_add(gs->sampled_entry_guards, guard);
      guard->in_selection = gs;
    } else {
      entry_guard_free(guard);
    }
  }

  if (set) {
    SMARTLIST_FOREACH_BEGIN(guard_contexts, guard_selection_t *, gs) {
      entry_guards_update_all(gs);
    } SMARTLIST_FOREACH_END(gs);
  }
  return n_errors ? -1 : 0;
}

/** If <b>digest</b> matches the identity of any node in the
 * entry_guards list for the provided guard selection state,
 return that node. Else return NULL. */
entry_guard_t *
entry_guard_get_by_id_digest_for_guard_selection(guard_selection_t *gs,
                                                 const char *digest)
{
  return get_sampled_guard_with_id(gs, (const uint8_t*)digest);
}

/** Return the node_t associated with a single entry_guard_t. May
 * return NULL if the guard is not currently in the consensus. */
const node_t *
entry_guard_find_node(const entry_guard_t *guard)
{
  tor_assert(guard);
  return node_get_by_id(guard->identity);
}

/** If <b>digest</b> matches the identity of any node in the
 * entry_guards list for the default guard selection state,
 return that node. Else return NULL. */
entry_guard_t *
entry_guard_get_by_id_digest(const char *digest)
{
  return entry_guard_get_by_id_digest_for_guard_selection(
      get_guard_selection_info(), digest);
}

/** We are about to connect to bridge with identity <b>digest</b> to fetch its
 *  descriptor. Create a new guard state for this connection and return it. */
circuit_guard_state_t *
get_guard_state_for_bridge_desc_fetch(const char *digest)
{
  circuit_guard_state_t *guard_state = NULL;
  entry_guard_t *guard = NULL;

  guard = entry_guard_get_by_id_digest_for_guard_selection(
                                    get_guard_selection_info(), digest);
  if (!guard) {
    return NULL;
  }

  /* Update the guard last_tried_to_connect time since it's checked by the
   * guard susbsystem. */
  guard->last_tried_to_connect = approx_time();

  /* Create the guard state */
  guard_state = circuit_guard_state_new(guard,
                                        GUARD_CIRC_STATE_USABLE_ON_COMPLETION,
                                        NULL);

  return guard_state;
}

/** Release all storage held by <b>e</b>. */
STATIC void
entry_guard_free_(entry_guard_t *e)
{
  if (!e)
    return;
  entry_guard_handles_clear(e);
  tor_free(e->sampled_by_version);
  tor_free(e->extra_state_fields);
  tor_free(e->selection_name);
  tor_free(e->bridge_addr);
  tor_free(e);
}

/** Return 0 if we're fine adding arbitrary routers out of the
 * directory to our entry guard list, or return 1 if we have a
 * list already and we must stick to it.
 */
int
entry_list_is_constrained(const or_options_t *options)
{
  // XXXX #21425 look at the current selection.
  if (options->EntryNodes)
    return 1;
  if (options->UseBridges)
    return 1;
  return 0;
}

/** Return the number of bridges that have descriptors that are marked with
 * purpose 'bridge' and are running. If use_maybe_reachable is
 * true, include bridges that might be reachable in the count.
 * Otherwise, if it is false, only include bridges that have recently been
 * found running in the count.
 *
 * We use this function to decide if we're ready to start building
 * circuits through our bridges, or if we need to wait until the
 * directory "server/authority" requests finish. */
MOCK_IMPL(int,
num_bridges_usable,(int use_maybe_reachable))
{
  int n_options = 0;

  if (BUG(!get_options()->UseBridges)) {
    return 0;
  }
  guard_selection_t *gs  = get_guard_selection_info();
  if (BUG(gs->type != GS_TYPE_BRIDGE)) {
    return 0;
  }

  SMARTLIST_FOREACH_BEGIN(gs->sampled_entry_guards, entry_guard_t *, guard) {
    /* Definitely not usable */
    if (guard->is_reachable == GUARD_REACHABLE_NO)
      continue;
    /* If we want to be really sure the bridges will work, skip maybes */
    if (!use_maybe_reachable && guard->is_reachable == GUARD_REACHABLE_MAYBE)
      continue;
    if (tor_digest_is_zero(guard->identity))
      continue;
    const node_t *node = node_get_by_id(guard->identity);
    if (node && node->ri)
      ++n_options;
  } SMARTLIST_FOREACH_END(guard);

  return n_options;
}

/** Check the pathbias use success count of <b>node</b> and disable it if it
 *  goes over our thresholds. */
static void
pathbias_check_use_success_count(entry_guard_t *node)
{
  const or_options_t *options = get_options();
  const double EPSILON = 1.0e-9;

  /* Note: We rely on the < comparison here to allow us to set a 0
   * rate and disable the feature entirely. If refactoring, don't
   * change to <= */
  if (node->pb.use_attempts > EPSILON &&
      pathbias_get_use_success_count(node)/node->pb.use_attempts
      < pathbias_get_extreme_use_rate(options) &&
      pathbias_get_dropguards(options)) {
    node->pb.path_bias_disabled = 1;
    log_info(LD_GENERAL,
             "Path use bias is too high (%f/%f); disabling node %s",
             node->pb.circ_successes, node->pb.circ_attempts,
             node->nickname);
  }
}

/** Check the pathbias close count of <b>node</b> and disable it if it goes
 *  over our thresholds. */
static void
pathbias_check_close_success_count(entry_guard_t *node)
{
  const or_options_t *options = get_options();
  const double EPSILON = 1.0e-9;

  /* Note: We rely on the < comparison here to allow us to set a 0
   * rate and disable the feature entirely. If refactoring, don't
   * change to <= */
  if (node->pb.circ_attempts > EPSILON &&
      pathbias_get_close_success_count(node)/node->pb.circ_attempts
      < pathbias_get_extreme_rate(options) &&
      pathbias_get_dropguards(options)) {
    node->pb.path_bias_disabled = 1;
    log_info(LD_GENERAL,
             "Path bias is too high (%f/%f); disabling node %s",
             node->pb.circ_successes, node->pb.circ_attempts,
             node->nickname);
  }
}

/** Parse <b>state</b> and learn about the entry guards it describes.
 * If <b>set</b> is true, and there are no errors, replace the guard
 * list in the default guard selection context with what we find.
 * On success, return 0. On failure, alloc into *<b>msg</b> a string
 * describing the error, and return -1.
 */
int
entry_guards_parse_state(or_state_t *state, int set, char **msg)
{
  entry_guards_dirty = 0;
  int r1 = entry_guards_load_guards_from_state(state, set);
  entry_guards_dirty = 0;

  if (r1 < 0) {
    if (msg && *msg == NULL) {
      *msg = tor_strdup("parsing error");
    }
    return -1;
  }
  return 0;
}

/** How long will we let a change in our guard nodes stay un-saved
 * when we are trying to avoid disk writes? */
#define SLOW_GUARD_STATE_FLUSH_TIME 600
/** How long will we let a change in our guard nodes stay un-saved
 * when we are not trying to avoid disk writes? */
#define FAST_GUARD_STATE_FLUSH_TIME 30

/** Our list of entry guards has changed for a particular guard selection
 * context, or some element of one of our entry guards has changed for one.
 * Write the changes to disk within the next few minutes.
 */
void
entry_guards_changed_for_guard_selection(guard_selection_t *gs)
{
  time_t when;

  tor_assert(gs != NULL);

  entry_guards_dirty = 1;

  if (get_options()->AvoidDiskWrites)
    when = time(NULL) + SLOW_GUARD_STATE_FLUSH_TIME;
  else
    when = time(NULL) + FAST_GUARD_STATE_FLUSH_TIME;

  /* or_state_save() will call entry_guards_update_state() and
     entry_guards_update_guards_in_state()
  */
  or_state_mark_dirty(get_or_state(), when);
}

/** Our list of entry guards has changed for the default guard selection
 * context, or some element of one of our entry guards has changed. Write
 * the changes to disk within the next few minutes.
 */
void
entry_guards_changed(void)
{
  entry_guards_changed_for_guard_selection(get_guard_selection_info());
}

/** If the entry guard info has not changed, do nothing and return.
 * Otherwise, free the EntryGuards piece of <b>state</b> and create
 * a new one out of the global entry_guards list, and then mark
 * <b>state</b> dirty so it will get saved to disk.
 */
void
entry_guards_update_state(or_state_t *state)
{
  entry_guards_dirty = 0;

  // Handles all guard info.
  entry_guards_update_guards_in_state(state);

  entry_guards_dirty = 0;

  if (!get_options()->AvoidDiskWrites)
    or_state_mark_dirty(get_or_state(), 0);
  entry_guards_dirty = 0;
}

/** Return true iff the circuit's guard can succeed that is can be used. */
int
entry_guard_could_succeed(const circuit_guard_state_t *guard_state)
{
  if (!guard_state) {
    return 0;
  }

  entry_guard_t *guard = entry_guard_handle_get(guard_state->guard);
  if (!guard || BUG(guard->in_selection == NULL)) {
    return 0;
  }

  return 1;
}

/**
 * Format a single entry guard in the format expected by the controller.
 * Return a newly allocated string.
 */
STATIC char *
getinfo_helper_format_single_entry_guard(const entry_guard_t *e)
{
  const char *status = NULL;
  time_t when = 0;
  const node_t *node;
  char tbuf[ISO_TIME_LEN+1];
  char nbuf[MAX_VERBOSE_NICKNAME_LEN+1];

  /* This is going to be a bit tricky, since the status
   * codes weren't really intended for prop271 guards.
   *
   * XXXX use a more appropriate format for exporting this information
   */
  if (e->confirmed_idx < 0) {
    status = "never-connected";
  } else if (! e->currently_listed) {
    when = e->unlisted_since_date;
    status = "unusable";
  } else if (! e->is_filtered_guard) {
    status = "unusable";
  } else if (e->is_reachable == GUARD_REACHABLE_NO) {
    when = e->failing_since;
    status = "down";
  } else {
    status = "up";
  }

  node = entry_guard_find_node(e);
  if (node) {
    node_get_verbose_nickname(node, nbuf);
  } else {
    nbuf[0] = '$';
    base16_encode(nbuf+1, sizeof(nbuf)-1, e->identity, DIGEST_LEN);
    /* e->nickname field is not very reliable if we don't know about
     * this router any longer; don't include it. */
  }

  char *result = NULL;
  if (when) {
    format_iso_time(tbuf, when);
    tor_asprintf(&result, "%s %s %s\n", nbuf, status, tbuf);
  } else {
    tor_asprintf(&result, "%s %s\n", nbuf, status);
  }
  return result;
}

/** If <b>question</b> is the string "entry-guards", then dump
 * to *<b>answer</b> a newly allocated string describing all of
 * the nodes in the global entry_guards list. See control-spec.txt
 * for details.
 * For backward compatibility, we also handle the string "helper-nodes".
 *
 * XXX this should be totally redesigned after prop 271 too, and that's
 * going to take some control spec work.
 * */
int
getinfo_helper_entry_guards(control_connection_t *conn,
                            const char *question, char **answer,
                            const char **errmsg)
{
  guard_selection_t *gs = get_guard_selection_info();

  tor_assert(gs != NULL);

  (void) conn;
  (void) errmsg;

  if (!strcmp(question,"entry-guards") ||
      !strcmp(question,"helper-nodes")) {
    const smartlist_t *guards;
    guards = gs->sampled_entry_guards;

    smartlist_t *sl = smartlist_new();

    SMARTLIST_FOREACH_BEGIN(guards, const entry_guard_t *, e) {
      char *cp = getinfo_helper_format_single_entry_guard(e);
      smartlist_add(sl, cp);
    } SMARTLIST_FOREACH_END(e);
    *answer = smartlist_join_strings(sl, "", 0, NULL);
    SMARTLIST_FOREACH(sl, char *, c, tor_free(c));
    smartlist_free(sl);
  }
  return 0;
}

/* Given the original bandwidth of a guard and its guardfraction,
 * calculate how much bandwidth the guard should have as a guard and
 * as a non-guard.
 *
 * Quoting from proposal236:
 *
 *   Let Wpf denote the weight from the 'bandwidth-weights' line a
 *   client would apply to N for position p if it had the guard
 *   flag, Wpn the weight if it did not have the guard flag, and B the
 *   measured bandwidth of N in the consensus.  Then instead of choosing
 *   N for position p proportionally to Wpf*B or Wpn*B, clients should
 *   choose N proportionally to F*Wpf*B + (1-F)*Wpn*B.
 *
 * This function fills the <b>guardfraction_bw</b> structure. It sets
 * <b>guard_bw</b> to F*B and <b>non_guard_bw</b> to (1-F)*B.
 */
void
guard_get_guardfraction_bandwidth(guardfraction_bandwidth_t *guardfraction_bw,
                                  int orig_bandwidth,
                                  uint32_t guardfraction_percentage)
{
  double guardfraction_fraction;

  /* Turn the percentage into a fraction. */
  tor_assert(guardfraction_percentage <= 100);
  guardfraction_fraction = guardfraction_percentage / 100.0;

  long guard_bw = tor_lround(guardfraction_fraction * orig_bandwidth);
  tor_assert(guard_bw <= INT_MAX);

  guardfraction_bw->guard_bw = (int) guard_bw;

  guardfraction_bw->non_guard_bw = orig_bandwidth - (int) guard_bw;
}

/** Helper: Update the status of all entry guards, in whatever algorithm
 * is used. Return true if we should stop using all previously generated
 * circuits, by calling circuit_mark_all_unused_circs() and
 * circuit_mark_all_dirty_circs_as_unusable().
 */
int
guards_update_all(void)
{
  int mark_circuits = 0;
  if (update_guard_selection_choice(get_options()))
    mark_circuits = 1;

  tor_assert(curr_guard_context);

  if (entry_guards_update_all(curr_guard_context))
    mark_circuits = 1;

  return mark_circuits;
}

/** Helper: pick a guard for a circuit, with whatever algorithm is
    used. */
const node_t *
guards_choose_guard(cpath_build_state_t *state,
                    uint8_t purpose,
                    circuit_guard_state_t **guard_state_out)
{
  const node_t *r = NULL;
  const uint8_t *exit_id = NULL;
  entry_guard_restriction_t *rst = NULL;

  /* Only apply restrictions if we have a specific exit node in mind, and only
   * if we are not doing vanguard circuits: we don't want to apply guard
   * restrictions to vanguard circuits. */
  if (state && !circuit_should_use_vanguards(purpose) &&
      (exit_id = build_state_get_exit_rsa_id(state))) {
    /* We're building to a targeted exit node, so that node can't be
     * chosen as our guard for this circuit.  Remember that fact in a
     * restriction. */
    rst = guard_create_exit_restriction(exit_id);
    tor_assert(rst);
  }
  if (entry_guard_pick_for_circuit(get_guard_selection_info(),
                                   GUARD_USAGE_TRAFFIC,
                                   rst,
                                   &r,
                                   guard_state_out) < 0) {
    tor_assert(r == NULL);
  }
  return r;
}

/** Remove all currently listed entry guards for a given guard selection
 * context.  This frees and replaces <b>gs</b>, so don't use <b>gs</b>
 * after calling this function. */
void
remove_all_entry_guards_for_guard_selection(guard_selection_t *gs)
{
  // This function shouldn't exist. XXXX
  tor_assert(gs != NULL);
  char *old_name = tor_strdup(gs->name);
  guard_selection_type_t old_type = gs->type;

  SMARTLIST_FOREACH(gs->sampled_entry_guards, entry_guard_t *, entry, {
    control_event_guard(entry->nickname, entry->identity, "DROPPED");
  });

  if (gs == curr_guard_context) {
    curr_guard_context = NULL;
  }

  smartlist_remove(guard_contexts, gs);
  guard_selection_free(gs);

  gs = get_guard_selection_by_name(old_name, old_type, 1);
  entry_guards_changed_for_guard_selection(gs);
  tor_free(old_name);
}

/** Remove all currently listed entry guards, so new ones will be chosen.
 *
 * XXXX This function shouldn't exist -- it's meant to support the DROPGUARDS
 * command, which is deprecated.
 */
void
remove_all_entry_guards(void)
{
  remove_all_entry_guards_for_guard_selection(get_guard_selection_info());
}

/** Helper: pick a directory guard, with whatever algorithm is used. */
const node_t *
guards_choose_dirguard(uint8_t dir_purpose,
                       circuit_guard_state_t **guard_state_out)
{
  const node_t *r = NULL;
  entry_guard_restriction_t *rst = NULL;

  /* If we are fetching microdescs, don't query outdated dirservers. */
  if (dir_purpose == DIR_PURPOSE_FETCH_MICRODESC) {
    rst = guard_create_dirserver_md_restriction();
  }

  if (entry_guard_pick_for_circuit(get_guard_selection_info(),
                                   GUARD_USAGE_DIRGUARD,
                                   rst,
                                   &r,
                                   guard_state_out) < 0) {
    tor_assert(r == NULL);
  }
  return r;
}

/**
 * If we're running with a constrained guard set, then maybe mark our guards
 * usable.  Return 1 if we do; 0 if we don't.
 */
int
guards_retry_optimistic(const or_options_t *options)
{
  if (! entry_list_is_constrained(options))
    return 0;

  mark_primary_guards_maybe_reachable(get_guard_selection_info());

  return 1;
}

/**
 * Check if we are missing any crucial dirinfo for the guard subsystem to
 * work. Return NULL if everything went well, otherwise return a newly
 * allocated string with an informative error message. In the latter case, use
 * the genreal descriptor information <b>using_mds</b>, <b>num_present</b> and
 * <b>num_usable</b> to improve the error message. */
char *
guard_selection_get_err_str_if_dir_info_missing(guard_selection_t *gs,
                                        int using_mds,
                                        int num_present, int num_usable)
{
  if (!gs->primary_guards_up_to_date)
    entry_guards_update_primary(gs);

  char *ret_str = NULL;
  int n_missing_descriptors = 0;
  int n_considered = 0;
  int num_primary_to_check;

  /* We want to check for the descriptor of at least the first two primary
   * guards in our list, since these are the guards that we typically use for
   * circuits. */
  num_primary_to_check = get_n_primary_guards_to_use(GUARD_USAGE_TRAFFIC);
  num_primary_to_check++;

  SMARTLIST_FOREACH_BEGIN(gs->primary_entry_guards, entry_guard_t *, guard) {
    entry_guard_consider_retry(guard);
    if (guard->is_reachable == GUARD_REACHABLE_NO)
      continue;
    n_considered++;
    if (!guard_has_descriptor(guard))
      n_missing_descriptors++;
    if (n_considered >= num_primary_to_check)
      break;
  } SMARTLIST_FOREACH_END(guard);

  /* If we are not missing any descriptors, return NULL. */
  if (!n_missing_descriptors) {
    return NULL;
  }

  /* otherwise return a helpful error string */
  tor_asprintf(&ret_str, "We're missing descriptors for %d/%d of our "
               "primary entry guards (total %sdescriptors: %d/%d).",
               n_missing_descriptors, num_primary_to_check,
               using_mds?"micro":"", num_present, num_usable);

  return ret_str;
}

/** As guard_selection_have_enough_dir_info_to_build_circuits, but uses
 * the default guard selection. */
char *
entry_guards_get_err_str_if_dir_info_missing(int using_mds,
                                     int num_present, int num_usable)
{
  return guard_selection_get_err_str_if_dir_info_missing(
                                                 get_guard_selection_info(),
                                                 using_mds,
                                                 num_present, num_usable);
}

/** Free one guard selection context */
STATIC void
guard_selection_free_(guard_selection_t *gs)
{
  if (!gs) return;

  tor_free(gs->name);

  if (gs->sampled_entry_guards) {
    SMARTLIST_FOREACH(gs->sampled_entry_guards, entry_guard_t *, e,
                      entry_guard_free(e));
    smartlist_free(gs->sampled_entry_guards);
    gs->sampled_entry_guards = NULL;
  }

  smartlist_free(gs->confirmed_entry_guards);
  smartlist_free(gs->primary_entry_guards);

  tor_free(gs);
}

/** Release all storage held by the list of entry guards and related
 * memory structs. */
void
entry_guards_free_all(void)
{
  /* Null out the default */
  curr_guard_context = NULL;
  /* Free all the guard contexts */
  if (guard_contexts != NULL) {
    SMARTLIST_FOREACH_BEGIN(guard_contexts, guard_selection_t *, gs) {
      guard_selection_free(gs);
    } SMARTLIST_FOREACH_END(gs);
    smartlist_free(guard_contexts);
    guard_contexts = NULL;
  }
  circuit_build_times_free_timeouts(get_circuit_build_times_mutable());
}
