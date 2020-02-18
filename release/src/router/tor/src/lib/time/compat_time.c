/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file compat_time.c
 * \brief Portable wrappers for finding out the current time, running
 *   timers, etc.
 **/

#define COMPAT_TIME_PRIVATE
#include "lib/time/compat_time.h"

#include "lib/err/torerr.h"
#include "lib/log/log.h"
#include "lib/log/util_bug.h"
#include "lib/intmath/muldiv.h"
#include "lib/intmath/bits.h"
#include "lib/fs/winlib.h"
#include "lib/wallclock/timeval.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef TOR_UNIT_TESTS
#if !defined(HAVE_USLEEP) && defined(HAVE_SYS_SELECT_H)
/* as fallback implementation for tor_sleep_msec */
#include <sys/select.h>
#endif
#endif /* defined(TOR_UNIT_TESTS) */

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#undef HAVE_CLOCK_GETTIME
#endif

#ifdef TOR_UNIT_TESTS
/** Delay for <b>msec</b> milliseconds.  Only used in tests. */
void
tor_sleep_msec(int msec)
{
#ifdef _WIN32
  Sleep(msec);
#elif defined(HAVE_USLEEP)
  sleep(msec / 1000);
  /* Some usleep()s hate sleeping more than 1 sec */
  usleep((msec % 1000) * 1000);
#elif defined(HAVE_SYS_SELECT_H)
  struct timeval tv = { msec / 1000, (msec % 1000) * 1000};
  select(0, NULL, NULL, NULL, &tv);
#else
  sleep(CEIL_DIV(msec, 1000));
#endif /* defined(_WIN32) || ... */
}
#endif /* defined(TOR_UNIT_TESTS) */

#define ONE_MILLION ((int64_t) (1000 * 1000))
#define ONE_BILLION ((int64_t) (1000 * 1000 * 1000))

/** True iff monotime_init has been called. */
static int monotime_initialized = 0;

static monotime_t initialized_at;
#ifdef MONOTIME_COARSE_FN_IS_DIFFERENT
static monotime_coarse_t initialized_at_coarse;
#endif

#ifdef TOR_UNIT_TESTS
/** True if we are running unit tests and overriding the current monotonic
 * time.  Note that mocked monotonic time might not be monotonic.
 */
static int monotime_mocking_enabled = 0;
static monotime_t initialized_at_saved;

static int64_t mock_time_nsec = 0;
#ifdef MONOTIME_COARSE_FN_IS_DIFFERENT
static int64_t mock_time_nsec_coarse = 0;
static monotime_coarse_t initialized_at_coarse_saved;
#endif

void
monotime_enable_test_mocking(void)
{
  if (BUG(monotime_initialized == 0)) {
    monotime_init();
  }

  tor_assert_nonfatal(monotime_mocking_enabled == 0);
  monotime_mocking_enabled = 1;
  memcpy(&initialized_at_saved,
         &initialized_at, sizeof(monotime_t));
  memset(&initialized_at, 0, sizeof(monotime_t));
#ifdef MONOTIME_COARSE_FN_IS_DIFFERENT
  memcpy(&initialized_at_coarse_saved,
         &initialized_at_coarse, sizeof(monotime_coarse_t));
  memset(&initialized_at_coarse, 0, sizeof(monotime_coarse_t));
#endif
}

void
monotime_disable_test_mocking(void)
{
  tor_assert_nonfatal(monotime_mocking_enabled == 1);
  monotime_mocking_enabled = 0;

  memcpy(&initialized_at,
         &initialized_at_saved, sizeof(monotime_t));
#ifdef MONOTIME_COARSE_FN_IS_DIFFERENT
  memcpy(&initialized_at_coarse,
         &initialized_at_coarse_saved, sizeof(monotime_coarse_t));
#endif
}

void
monotime_set_mock_time_nsec(int64_t nsec)
{
  tor_assert_nonfatal(monotime_mocking_enabled == 1);
  mock_time_nsec = nsec;
}

#ifdef MONOTIME_COARSE_FN_IS_DIFFERENT
void
monotime_coarse_set_mock_time_nsec(int64_t nsec)
{
  tor_assert_nonfatal(monotime_mocking_enabled == 1);
  mock_time_nsec_coarse = nsec;
}
#endif /* defined(MONOTIME_COARSE_FN_IS_DIFFERENT) */
#endif /* defined(TOR_UNIT_TESTS) */

/* "ratchet" functions for monotonic time. */

#if defined(_WIN32) || defined(TOR_UNIT_TESTS)

/** Protected by lock: last value returned by monotime_get(). */
static int64_t last_pctr = 0;
/** Protected by lock: offset we must add to monotonic time values. */
static int64_t pctr_offset = 0;
/* If we are using GetTickCount(), how many times has it rolled over? */
static uint32_t rollover_count = 0;
/* If we are using GetTickCount(), what's the last value it returned? */
static int64_t last_tick_count = 0;

/** Helper for windows: Called with a sequence of times that are supposed
 * to be monotonic; increments them as appropriate so that they actually
 * _are_ monotonic.
 *
 * Caller must hold lock. */
STATIC int64_t
ratchet_performance_counter(int64_t count_raw)
{
  /* must hold lock */
  const int64_t count_adjusted = count_raw + pctr_offset;

  if (PREDICT_UNLIKELY(count_adjusted < last_pctr)) {
    /* Monotonicity failed! Pretend no time elapsed. */
    pctr_offset = last_pctr - count_raw;
    return last_pctr;
  } else {
    last_pctr = count_adjusted;
    return count_adjusted;
  }
}

STATIC int64_t
ratchet_coarse_performance_counter(const int64_t count_raw)
{
  int64_t count = count_raw + (((int64_t)rollover_count) << 32);
  while (PREDICT_UNLIKELY(count < last_tick_count)) {
    ++rollover_count;
    count = count_raw + (((int64_t)rollover_count) << 32);
  }
  last_tick_count = count;
  return count;
}
#endif /* defined(_WIN32) || defined(TOR_UNIT_TESTS) */

#if defined(MONOTIME_USING_GETTIMEOFDAY) || defined(TOR_UNIT_TESTS)
static struct timeval last_timeofday = { 0, 0 };
static struct timeval timeofday_offset = { 0, 0 };

/** Helper for gettimeofday(): Called with a sequence of times that are
 * supposed to be monotonic; increments them as appropriate so that they
 * actually _are_ monotonic.
 *
 * Caller must hold lock. */
STATIC void
ratchet_timeval(const struct timeval *timeval_raw, struct timeval *out)
{
  /* must hold lock */
  timeradd(timeval_raw, &timeofday_offset, out);
  if (PREDICT_UNLIKELY(timercmp(out, &last_timeofday, OP_LT))) {
    /* time ran backwards. Instead, declare that no time occurred. */
    timersub(&last_timeofday, timeval_raw, &timeofday_offset);
    memcpy(out, &last_timeofday, sizeof(struct timeval));
  } else {
    memcpy(&last_timeofday, out, sizeof(struct timeval));
  }
}
#endif /* defined(MONOTIME_USING_GETTIMEOFDAY) || defined(TOR_UNIT_TESTS) */

#ifdef TOR_UNIT_TESTS
/** For testing: reset all the ratchets */
void
monotime_reset_ratchets_for_testing(void)
{
  last_pctr = pctr_offset = last_tick_count = 0;
  rollover_count = 0;
  memset(&last_timeofday, 0, sizeof(struct timeval));
  memset(&timeofday_offset, 0, sizeof(struct timeval));
}
#endif /* defined(TOR_UNIT_TESTS) */

#ifdef __APPLE__

/** Initialized on startup: tells is how to convert from ticks to
 * nanoseconds.
 */
static struct mach_timebase_info mach_time_info;
static struct mach_timebase_info mach_time_info_msec_cvt;
static int32_t mach_time_msec_cvt_threshold;
static int monotime_shift = 0;

static void
monotime_init_internal(void)
{
  tor_assert(!monotime_initialized);
  int r = mach_timebase_info(&mach_time_info);
  tor_assert(r == 0);
  tor_assert(mach_time_info.denom != 0);

  {
    // approximate only.
    uint64_t ns_per_tick = mach_time_info.numer / mach_time_info.denom;
    uint64_t ms_per_tick = ns_per_tick * ONE_MILLION;
    // requires that tor_log2(0) == 0.
    monotime_shift = tor_log2(ms_per_tick);
  }
  {
    // For converting ticks to milliseconds in a 32-bit-friendly way, we
    // will first right-shift by 20, and then multiply by 2048/1953, since
    // (1<<20) * 1953/2048 is about 1e6.  We precompute a new numerator and
    // denominator here to avoid multiple multiplies.
    mach_time_info_msec_cvt.numer = mach_time_info.numer * 2048;
    mach_time_info_msec_cvt.denom = mach_time_info.denom * 1953;
    // For any value above this amount, we should divide before multiplying,
    // to avoid overflow.  For a value below this, we should multiply
    // before dividing, to improve accuracy.
    mach_time_msec_cvt_threshold = INT32_MAX / mach_time_info_msec_cvt.numer;
  }
}

/**
 * Set "out" to the most recent monotonic time value
 */
void
monotime_get(monotime_t *out)
{
#ifdef TOR_UNIT_TESTS
  if (monotime_mocking_enabled) {
    out->abstime_ = (mock_time_nsec * mach_time_info.denom)
      / mach_time_info.numer;
    return;
  }
#endif /* defined(TOR_UNIT_TESTS) */
  out->abstime_ = mach_absolute_time();
}

#if defined(HAVE_MACH_APPROXIMATE_TIME)
void
monotime_coarse_get(monotime_coarse_t *out)
{
#ifdef TOR_UNIT_TESTS
  if (monotime_mocking_enabled) {
    out->abstime_ = (mock_time_nsec_coarse * mach_time_info.denom)
      / mach_time_info.numer;
    return;
  }
#endif /* defined(TOR_UNIT_TESTS) */
  out->abstime_ = mach_approximate_time();
}
#endif

/**
 * Return the number of nanoseconds between <b>start</b> and <b>end</b>.
 */
int64_t
monotime_diff_nsec(const monotime_t *start,
                   const monotime_t *end)
{
  if (BUG(mach_time_info.denom == 0)) {
    monotime_init();
  }
  const int64_t diff_ticks = end->abstime_ - start->abstime_;
  const int64_t diff_nsec =
    (diff_ticks * mach_time_info.numer) / mach_time_info.denom;
  return diff_nsec;
}

int32_t
monotime_coarse_diff_msec32_(const monotime_coarse_t *start,
                             const monotime_coarse_t *end)
{
  if (BUG(mach_time_info.denom == 0)) {
    monotime_init();
  }
  const int64_t diff_ticks = end->abstime_ - start->abstime_;

  /* We already require in di_ops.c that right-shift performs a sign-extend. */
  const int32_t diff_microticks = (int32_t)(diff_ticks >> 20);

  if (diff_microticks >= mach_time_msec_cvt_threshold) {
    return (diff_microticks / mach_time_info_msec_cvt.denom) *
      mach_time_info_msec_cvt.numer;
  } else {
    return (diff_microticks * mach_time_info_msec_cvt.numer) /
      mach_time_info_msec_cvt.denom;
  }
}

uint32_t
monotime_coarse_to_stamp(const monotime_coarse_t *t)
{
  return (uint32_t)(t->abstime_ >> monotime_shift);
}

int
monotime_is_zero(const monotime_t *val)
{
  return val->abstime_ == 0;
}

void
monotime_add_msec(monotime_t *out, const monotime_t *val, uint32_t msec)
{
  const uint64_t nsec = msec * ONE_MILLION;
  const uint64_t ticks = (nsec * mach_time_info.denom) / mach_time_info.numer;
  out->abstime_ = val->abstime_ + ticks;
}

/* end of "__APPLE__" */
#elif defined(HAVE_CLOCK_GETTIME)

#ifdef CLOCK_MONOTONIC_COARSE
/**
 * Which clock should we use for coarse-grained monotonic time? By default
 * this is CLOCK_MONOTONIC_COARSE, but it might not work -- for example,
 * if we're compiled with newer Linux headers and then we try to run on
 * an old Linux kernel. In that case, we will fall back to CLOCK_MONOTONIC.
 */
static int clock_monotonic_coarse = CLOCK_MONOTONIC_COARSE;
#endif /* defined(CLOCK_MONOTONIC_COARSE) */

static void
monotime_init_internal(void)
{
#ifdef CLOCK_MONOTONIC_COARSE
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) < 0) {
    log_info(LD_GENERAL, "CLOCK_MONOTONIC_COARSE isn't working (%s); "
             "falling back to CLOCK_MONOTONIC.", strerror(errno));
    clock_monotonic_coarse = CLOCK_MONOTONIC;
  }
#endif /* defined(CLOCK_MONOTONIC_COARSE) */
}

void
monotime_get(monotime_t *out)
{
#ifdef TOR_UNIT_TESTS
  if (monotime_mocking_enabled) {
    out->ts_.tv_sec = (time_t) (mock_time_nsec / ONE_BILLION);
    out->ts_.tv_nsec = (int) (mock_time_nsec % ONE_BILLION);
    return;
  }
#endif /* defined(TOR_UNIT_TESTS) */
  int r = clock_gettime(CLOCK_MONOTONIC, &out->ts_);
  tor_assert(r == 0);
}

#ifdef CLOCK_MONOTONIC_COARSE
void
monotime_coarse_get(monotime_coarse_t *out)
{
#ifdef TOR_UNIT_TESTS
  if (monotime_mocking_enabled) {
    out->ts_.tv_sec = (time_t) (mock_time_nsec_coarse / ONE_BILLION);
    out->ts_.tv_nsec = (int) (mock_time_nsec_coarse % ONE_BILLION);
    return;
  }
#endif /* defined(TOR_UNIT_TESTS) */
  int r = clock_gettime(clock_monotonic_coarse, &out->ts_);
  if (PREDICT_UNLIKELY(r < 0) &&
      errno == EINVAL &&
      clock_monotonic_coarse == CLOCK_MONOTONIC_COARSE) {
    /* We should have caught this at startup in monotime_init_internal!
     */
    log_warn(LD_BUG, "Falling back to non-coarse monotonic time %s initial "
             "system start?", monotime_initialized?"after":"without");
    clock_monotonic_coarse = CLOCK_MONOTONIC;
    r = clock_gettime(clock_monotonic_coarse, &out->ts_);
  }

  tor_assert(r == 0);
}
#endif /* defined(CLOCK_MONOTONIC_COARSE) */

int64_t
monotime_diff_nsec(const monotime_t *start,
                   const monotime_t *end)
{
  const int64_t diff_sec = end->ts_.tv_sec - start->ts_.tv_sec;
  const int64_t diff_nsec = diff_sec * ONE_BILLION +
    (end->ts_.tv_nsec - start->ts_.tv_nsec);

  return diff_nsec;
}

int32_t
monotime_coarse_diff_msec32_(const monotime_coarse_t *start,
                             const monotime_coarse_t *end)
{
  const int32_t diff_sec = (int32_t)(end->ts_.tv_sec - start->ts_.tv_sec);
  const int32_t diff_nsec = (int32_t)(end->ts_.tv_nsec - start->ts_.tv_nsec);
  return diff_sec * 1000 + diff_nsec / ONE_MILLION;
}

/* This value is ONE_BILLION >> 20. */
static const uint32_t STAMP_TICKS_PER_SECOND = 953;

uint32_t
monotime_coarse_to_stamp(const monotime_coarse_t *t)
{
  uint32_t nsec = (uint32_t)t->ts_.tv_nsec;
  uint32_t sec = (uint32_t)t->ts_.tv_sec;

  return (sec * STAMP_TICKS_PER_SECOND) + (nsec >> 20);
}

int
monotime_is_zero(const monotime_t *val)
{
  return val->ts_.tv_sec == 0 && val->ts_.tv_nsec == 0;
}

void
monotime_add_msec(monotime_t *out, const monotime_t *val, uint32_t msec)
{
  const uint32_t sec = msec / 1000;
  const uint32_t msec_remainder = msec % 1000;
  out->ts_.tv_sec = val->ts_.tv_sec + sec;
  out->ts_.tv_nsec = val->ts_.tv_nsec + (msec_remainder * ONE_MILLION);
  if (out->ts_.tv_nsec > ONE_BILLION) {
    out->ts_.tv_nsec -= ONE_BILLION;
    out->ts_.tv_sec += 1;
  }
}

/* end of "HAVE_CLOCK_GETTIME" */
#elif defined (_WIN32)

/** Result of QueryPerformanceFrequency, in terms needed to
 * convert ticks to nanoseconds. */
static int64_t nsec_per_tick_numer = 1;
static int64_t nsec_per_tick_denom = 1;

/** Lock to protect last_pctr and pctr_offset */
static CRITICAL_SECTION monotime_lock;
/** Lock to protect rollover_count and last_tick_count */
static CRITICAL_SECTION monotime_coarse_lock;

typedef ULONGLONG (WINAPI *GetTickCount64_fn_t)(void);
static GetTickCount64_fn_t GetTickCount64_fn = NULL;

static void
monotime_init_internal(void)
{
  tor_assert(!monotime_initialized);
  BOOL ok = InitializeCriticalSectionAndSpinCount(&monotime_lock, 200);
  tor_assert(ok);
  ok = InitializeCriticalSectionAndSpinCount(&monotime_coarse_lock, 200);
  tor_assert(ok);
  LARGE_INTEGER li;
  ok = QueryPerformanceFrequency(&li);
  tor_assert(ok);
  tor_assert(li.QuadPart);

  uint64_t n = ONE_BILLION;
  uint64_t d = li.QuadPart;
  /* We need to simplify this or we'll probably overflow the int64. */
  simplify_fraction64(&n, &d);
  tor_assert(n <= INT64_MAX);
  tor_assert(d <= INT64_MAX);

  nsec_per_tick_numer = (int64_t) n;
  nsec_per_tick_denom = (int64_t) d;

  last_pctr = 0;
  pctr_offset = 0;

  HANDLE h = load_windows_system_library(TEXT("kernel32.dll"));
  if (h) {
    GetTickCount64_fn = (GetTickCount64_fn_t) (void(*)(void))
      GetProcAddress(h, "GetTickCount64");
  }
  // FreeLibrary(h) ?
}

void
monotime_get(monotime_t *out)
{
  if (BUG(monotime_initialized == 0)) {
    monotime_init();
  }

#ifdef TOR_UNIT_TESTS
  if (monotime_mocking_enabled) {
    out->pcount_ = (mock_time_nsec * nsec_per_tick_denom)
      / nsec_per_tick_numer;
    return;
  }
#endif /* defined(TOR_UNIT_TESTS) */

  /* Alas, QueryPerformanceCounter is not always monotonic: see bug list at

    https://www.python.org/dev/peps/pep-0418/#windows-queryperformancecounter
   */

  EnterCriticalSection(&monotime_lock);
  LARGE_INTEGER res;
  BOOL ok = QueryPerformanceCounter(&res);
  tor_assert(ok);
  const int64_t count_raw = res.QuadPart;
  out->pcount_ = ratchet_performance_counter(count_raw);
  LeaveCriticalSection(&monotime_lock);
}

void
monotime_coarse_get(monotime_coarse_t *out)
{
#ifdef TOR_UNIT_TESTS
  if (monotime_mocking_enabled) {
    out->tick_count_ = mock_time_nsec_coarse / ONE_MILLION;
    return;
  }
#endif /* defined(TOR_UNIT_TESTS) */

  if (GetTickCount64_fn) {
    out->tick_count_ = (int64_t)GetTickCount64_fn();
  } else {
    EnterCriticalSection(&monotime_coarse_lock);
    DWORD tick = GetTickCount();
    out->tick_count_ = ratchet_coarse_performance_counter(tick);
    LeaveCriticalSection(&monotime_coarse_lock);
  }
}

int64_t
monotime_diff_nsec(const monotime_t *start,
                   const monotime_t *end)
{
  if (BUG(monotime_initialized == 0)) {
    monotime_init();
  }
  const int64_t diff_ticks = end->pcount_ - start->pcount_;
  return (diff_ticks * nsec_per_tick_numer) / nsec_per_tick_denom;
}

int64_t
monotime_coarse_diff_msec(const monotime_coarse_t *start,
                          const monotime_coarse_t *end)
{
  const int64_t diff_ticks = end->tick_count_ - start->tick_count_;
  return diff_ticks;
}

int32_t
monotime_coarse_diff_msec32_(const monotime_coarse_t *start,
                             const monotime_coarse_t *end)
{
  return (int32_t)monotime_coarse_diff_msec(start, end);
}

int64_t
monotime_coarse_diff_usec(const monotime_coarse_t *start,
                          const monotime_coarse_t *end)
{
  return monotime_coarse_diff_msec(start, end) * 1000;
}

int64_t
monotime_coarse_diff_nsec(const monotime_coarse_t *start,
                          const monotime_coarse_t *end)
{
  return monotime_coarse_diff_msec(start, end) * ONE_MILLION;
}

static const uint32_t STAMP_TICKS_PER_SECOND = 1000;

uint32_t
monotime_coarse_to_stamp(const monotime_coarse_t *t)
{
  return (uint32_t) t->tick_count_;
}

int
monotime_is_zero(const monotime_t *val)
{
  return val->pcount_ == 0;
}

int
monotime_coarse_is_zero(const monotime_coarse_t *val)
{
  return val->tick_count_ == 0;
}

void
monotime_add_msec(monotime_t *out, const monotime_t *val, uint32_t msec)
{
  const uint64_t nsec = msec * ONE_MILLION;
  const uint64_t ticks = (nsec * nsec_per_tick_denom) / nsec_per_tick_numer;
  out->pcount_ = val->pcount_ + ticks;
}

void
monotime_coarse_add_msec(monotime_coarse_t *out, const monotime_coarse_t *val,
                         uint32_t msec)
{
  out->tick_count_ = val->tick_count_ + msec;
}

/* end of "_WIN32" */
#elif defined(MONOTIME_USING_GETTIMEOFDAY)

static tor_mutex_t monotime_lock;

/** Initialize the monotonic timer subsystem. */
static void
monotime_init_internal(void)
{
  tor_assert(!monotime_initialized);
  tor_mutex_init(&monotime_lock);
}

void
monotime_get(monotime_t *out)
{
  if (BUG(monotime_initialized == 0)) {
    monotime_init();
  }

  tor_mutex_acquire(&monotime_lock);
  struct timeval timeval_raw;
  tor_gettimeofday(&timeval_raw);
  ratchet_timeval(&timeval_raw, &out->tv_);
  tor_mutex_release(&monotime_lock);
}

int64_t
monotime_diff_nsec(const monotime_t *start,
                   const monotime_t *end)
{
  struct timeval diff;
  timersub(&end->tv_, &start->tv_, &diff);
  return (diff.tv_sec * ONE_BILLION + diff.tv_usec * 1000);
}

int32_t
monotime_coarse_diff_msec32_(const monotime_coarse_t *start,
                             const monotime_coarse_t *end)
{
  struct timeval diff;
  timersub(&end->tv_, &start->tv_, &diff);
  return diff.tv_sec * 1000 + diff.tv_usec / 1000;
}

/* This value is ONE_MILLION >> 10. */
static const uint32_t STAMP_TICKS_PER_SECOND = 976;

uint32_t
monotime_coarse_to_stamp(const monotime_coarse_t *t)
{
  const uint32_t usec = (uint32_t)t->tv_.tv_usec;
  const uint32_t sec = (uint32_t)t->tv_.tv_sec;
  return (sec * STAMP_TICKS_PER_SECOND) | (nsec >> 10);
}

int
monotime_is_zero(const monotime_t *val)
{
  return val->tv_.tv_sec == 0 && val->tv_.tv_usec == 0;
}

void
monotime_add_msec(monotime_t *out, const monotime_t *val, uint32_t msec)
{
  const uint32_t sec = msec / 1000;
  const uint32_t msec_remainder = msec % 1000;
  out->tv_.tv_sec = val->tv_.tv_sec + sec;
  out->tv_.tv_usec = val->tv_.tv_nsec + (msec_remainder * 1000);
  if (out->tv_.tv_usec > ONE_MILLION) {
    out->tv_.tv_usec -= ONE_MILLION;
    out->tv_.tv_sec += 1;
  }
}

/* end of "MONOTIME_USING_GETTIMEOFDAY" */
#else
#error "No way to implement monotonic timers."
#endif /* defined(__APPLE__) || ... */

/**
 * Initialize the monotonic timer subsystem. Must be called before any
 * monotonic timer functions. This function is idempotent.
 */
void
monotime_init(void)
{
  if (!monotime_initialized) {
    monotime_init_internal();
    monotime_initialized = 1;
    monotime_get(&initialized_at);
#ifdef MONOTIME_COARSE_FN_IS_DIFFERENT
    monotime_coarse_get(&initialized_at_coarse);
#endif
  }
}

void
monotime_zero(monotime_t *out)
{
  memset(out, 0, sizeof(*out));
}
#ifdef MONOTIME_COARSE_TYPE_IS_DIFFERENT
void
monotime_coarse_zero(monotime_coarse_t *out)
{
  memset(out, 0, sizeof(*out));
}
#endif

int64_t
monotime_diff_usec(const monotime_t *start,
                   const monotime_t *end)
{
  const int64_t nsec = monotime_diff_nsec(start, end);
  return CEIL_DIV(nsec, 1000);
}

int64_t
monotime_diff_msec(const monotime_t *start,
                   const monotime_t *end)
{
  const int64_t nsec = monotime_diff_nsec(start, end);
  return CEIL_DIV(nsec, ONE_MILLION);
}

uint64_t
monotime_absolute_nsec(void)
{
  monotime_t now;
  if (BUG(monotime_initialized == 0)) {
    monotime_init();
  }

  monotime_get(&now);
  return monotime_diff_nsec(&initialized_at, &now);
}

uint64_t
monotime_absolute_usec(void)
{
  return monotime_absolute_nsec() / 1000;
}

uint64_t
monotime_absolute_msec(void)
{
  return monotime_absolute_nsec() / ONE_MILLION;
}

#ifdef MONOTIME_COARSE_FN_IS_DIFFERENT
uint64_t
monotime_coarse_absolute_nsec(void)
{
  if (BUG(monotime_initialized == 0)) {
    monotime_init();
  }

  monotime_coarse_t now;
  monotime_coarse_get(&now);
  return monotime_coarse_diff_nsec(&initialized_at_coarse, &now);
}

uint64_t
monotime_coarse_absolute_usec(void)
{
  return monotime_coarse_absolute_nsec() / 1000;
}

uint64_t
monotime_coarse_absolute_msec(void)
{
  return monotime_coarse_absolute_nsec() / ONE_MILLION;
}
#else
#define initialized_at_coarse initialized_at
#endif /* defined(MONOTIME_COARSE_FN_IS_DIFFERENT) */

/**
 * Return the current time "stamp" as described by monotime_coarse_to_stamp.
 */
uint32_t
monotime_coarse_get_stamp(void)
{
  monotime_coarse_t now;
  monotime_coarse_get(&now);
  return monotime_coarse_to_stamp(&now);
}

#ifdef __APPLE__
uint64_t
monotime_coarse_stamp_units_to_approx_msec(uint64_t units)
{
  /* Recover as much precision as we can. */
  uint64_t abstime_diff = (units << monotime_shift);
  return (abstime_diff * mach_time_info.numer) /
    (mach_time_info.denom * ONE_MILLION);
}
uint64_t
monotime_msec_to_approx_coarse_stamp_units(uint64_t msec)
{
  uint64_t abstime_val =
    (((uint64_t)msec) * ONE_MILLION * mach_time_info.denom) /
    mach_time_info.numer;
  return abstime_val >> monotime_shift;
}
#else
uint64_t
monotime_coarse_stamp_units_to_approx_msec(uint64_t units)
{
  return (units * 1000) / STAMP_TICKS_PER_SECOND;
}
uint64_t
monotime_msec_to_approx_coarse_stamp_units(uint64_t msec)
{
  return (msec * STAMP_TICKS_PER_SECOND) / 1000;
}
#endif
