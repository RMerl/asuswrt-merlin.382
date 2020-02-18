/* Copyright (c) 2015-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file periodic.c
 *
 * \brief Generic backend for handling periodic events.
 *
 * The events in this module are used by main.c to track items that need
 * to fire once every N seconds, possibly picking a new interval each time
 * that they fire.  See periodic_events[] in main.c for examples.
 */

#include "core/or/or.h"
#include "lib/evloop/compat_libevent.h"
#include "app/config/config.h"
#include "core/mainloop/mainloop.h"
#include "core/mainloop/periodic.h"
#include "lib/evloop/compat_libevent.h"

/** We disable any interval greater than this number of seconds, on the
 * grounds that it is probably an absolute time mistakenly passed in as a
 * relative time.
 */
static const int MAX_INTERVAL = 10 * 365 * 86400;

/** Set the event <b>event</b> to run in <b>next_interval</b> seconds from
 * now. */
static void
periodic_event_set_interval(periodic_event_item_t *event,
                            time_t next_interval)
{
  tor_assert(next_interval < MAX_INTERVAL);
  struct timeval tv;
  tv.tv_sec = next_interval;
  tv.tv_usec = 0;
  mainloop_event_schedule(event->ev, &tv);
}

/** Wraps dispatches for periodic events, <b>data</b> will be a pointer to the
 * event that needs to be called */
static void
periodic_event_dispatch(mainloop_event_t *ev, void *data)
{
  periodic_event_item_t *event = data;
  tor_assert(ev == event->ev);

  if (BUG(!periodic_event_is_enabled(event))) {
    return;
  }

  time_t now = time(NULL);
  update_current_time(now);
  const or_options_t *options = get_options();
//  log_debug(LD_GENERAL, "Dispatching %s", event->name);
  int r = event->fn(now, options);
  int next_interval = 0;

  if (!periodic_event_is_enabled(event)) {
    /* The event got disabled from inside its callback; no need to
     * reschedule. */
    return;
  }

  /* update the last run time if action was taken */
  if (r==0) {
    log_err(LD_BUG, "Invalid return value for periodic event from %s.",
                      event->name);
    tor_assert(r != 0);
  } else if (r > 0) {
    event->last_action_time = now;
    /* If the event is meant to happen after ten years, that's likely
     * a bug, and somebody gave an absolute time rather than an interval.
     */
    tor_assert(r < MAX_INTERVAL);
    next_interval = r;
  } else {
    /* no action was taken, it is likely a precondition failed,
     * we should reschedule for next second incase the precondition
     * passes then */
    next_interval = 1;
  }

//  log_debug(LD_GENERAL, "Scheduling %s for %d seconds", event->name,
//           next_interval);
  struct timeval tv = { next_interval , 0 };
  mainloop_event_schedule(ev, &tv);
}

/** Schedules <b>event</b> to run as soon as possible from now. */
void
periodic_event_reschedule(periodic_event_item_t *event)
{
  /* Don't reschedule a disabled event. */
  if (periodic_event_is_enabled(event)) {
    periodic_event_set_interval(event, 1);
  }
}

/** Initializes the libevent backend for a periodic event. */
void
periodic_event_setup(periodic_event_item_t *event)
{
  if (event->ev) { /* Already setup? This is a bug */
    log_err(LD_BUG, "Initial dispatch should only be done once.");
    tor_assert(0);
  }

  event->ev = mainloop_event_new(periodic_event_dispatch,
                                 event);
  tor_assert(event->ev);
}

/** Handles initial dispatch for periodic events. It should happen 1 second
 * after the events are created to mimic behaviour before #3199's refactor */
void
periodic_event_launch(periodic_event_item_t *event)
{
  if (! event->ev) { /* Not setup? This is a bug */
    log_err(LD_BUG, "periodic_event_launch without periodic_event_setup");
    tor_assert(0);
  }
  /* Event already enabled? This is a bug */
  if (periodic_event_is_enabled(event)) {
    log_err(LD_BUG, "periodic_event_launch on an already enabled event");
    tor_assert(0);
  }

  // Initial dispatch
  event->enabled = 1;
  periodic_event_dispatch(event->ev, event);
}

/** Release all storage associated with <b>event</b> */
void
periodic_event_destroy(periodic_event_item_t *event)
{
  if (!event)
    return;

  /* First disable the event so we first cancel the event and set its enabled
   * flag properly. */
  periodic_event_disable(event);

  mainloop_event_free(event->ev);
  event->last_action_time = 0;
}

/** Enable the given event by setting its "enabled" flag and scheduling it to
 * run immediately in the event loop. This can be called for an event that is
 * already enabled. */
void
periodic_event_enable(periodic_event_item_t *event)
{
  tor_assert(event);
  /* Safely and silently ignore if this event is already enabled. */
  if (periodic_event_is_enabled(event)) {
    return;
  }

  tor_assert(event->ev);
  event->enabled = 1;
  mainloop_event_activate(event->ev);
}

/** Disable the given event which means the event is destroyed and then the
 * event's enabled flag is unset. This can be called for an event that is
 * already disabled. */
void
periodic_event_disable(periodic_event_item_t *event)
{
  tor_assert(event);
  /* Safely and silently ignore if this event is already disabled. */
  if (!periodic_event_is_enabled(event)) {
    return;
  }
  mainloop_event_cancel(event->ev);
  event->enabled = 0;
}
