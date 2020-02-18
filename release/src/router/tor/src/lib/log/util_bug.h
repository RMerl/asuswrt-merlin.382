/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file util_bug.h
 *
 * \brief Macros to manage assertions, fatal and non-fatal.
 *
 * Guidelines: All the different kinds of assertion in this file are for
 * bug-checking only. Don't write code that can assert based on bad inputs.
 *
 * We provide two kinds of assertion here: "fatal" and "nonfatal". Use
 * nonfatal assertions for any bug you can reasonably recover from -- and
 * please, try to recover!  Many severe bugs in Tor have been caused by using
 * a regular assertion when a nonfatal assertion would have been better.
 *
 * If you need to check a condition with a nonfatal assertion, AND recover
 * from that same condition, consider using the BUG() macro inside a
 * conditional.  For example:
 *
 * <code>
 *  // wrong -- use tor_assert_nonfatal() if you just want an assertion.
 *  BUG(ptr == NULL);
 *
 *  // okay, but needlessly verbose
 *  tor_assert_nonfatal(ptr != NULL);
 *  if (ptr == NULL) { ... }
 *
 *  // this is how we do it:
 *  if (BUG(ptr == NULL)) { ... }
 * </code>
 **/

#ifndef TOR_UTIL_BUG_H
#define TOR_UTIL_BUG_H

#include "orconfig.h"
#include "lib/cc/compat_compiler.h"
#include "lib/log/log.h"
#include "lib/testsupport/testsupport.h"

/* Replace assert() with a variant that sends failures to the log before
 * calling assert() normally.
 */
#ifdef NDEBUG
/* Nobody should ever want to build with NDEBUG set.  99% of our asserts will
 * be outside the critical path anyway, so it's silly to disable bug-checking
 * throughout the entire program just because a few asserts are slowing you
 * down.  Profile, optimize the critical path, and keep debugging on.
 *
 * And I'm not just saying that because some of our asserts check
 * security-critical properties.
 */
#error "Sorry; we don't support building with NDEBUG."
#endif /* defined(NDEBUG) */

#if defined(TOR_UNIT_TESTS) && defined(__GNUC__)
/* We define this GCC macro as a replacement for PREDICT_UNLIKELY() in this
 * header, so that in our unit test builds, we'll get compiler warnings about
 * stuff like tor_assert(n = 5).
 *
 * The key here is that (e) is wrapped in exactly one layer of parentheses,
 * and then passed right to a conditional.  If you do anything else to the
 * expression here, or introduce any more parentheses, the compiler won't
 * help you.
 *
 * We only do this for the unit-test build case because it interferes with
 * the likely-branch labeling.  Note below that in the other case, we define
 * these macros to just be synonyms for PREDICT_(UN)LIKELY.
 */
#define ASSERT_PREDICT_UNLIKELY_(e)             \
  ( {                                           \
    int tor__assert_tmp_value__;                \
    if (e)                                      \
      tor__assert_tmp_value__ = 1;              \
    else                                        \
      tor__assert_tmp_value__ = 0;              \
    tor__assert_tmp_value__;                    \
  } )
#define ASSERT_PREDICT_LIKELY_(e) ASSERT_PREDICT_UNLIKELY_(e)
#else
#define ASSERT_PREDICT_UNLIKELY_(e) PREDICT_UNLIKELY(e)
#define ASSERT_PREDICT_LIKELY_(e) PREDICT_LIKELY(e)
#endif

/* Sometimes we don't want to use assertions during branch coverage tests; it
 * leads to tons of unreached branches which in reality are only assertions we
 * didn't hit. */
#if defined(TOR_UNIT_TESTS) && defined(DISABLE_ASSERTS_IN_UNIT_TESTS)
#define tor_assert(a) STMT_BEGIN                                        \
  (void)(a);                                                            \
  STMT_END
#else
/** Like assert(3), but send assertion failures to the log as well as to
 * stderr. */
#define tor_assert(expr) STMT_BEGIN                                     \
  if (ASSERT_PREDICT_LIKELY_(expr)) {                                   \
  } else {                                                              \
    tor_assertion_failed_(SHORT_FILE__, __LINE__, __func__, #expr);     \
    tor_abort_();                                                       \
  } STMT_END
#endif /* defined(TOR_UNIT_TESTS) && defined(DISABLE_ASSERTS_IN_UNIT_TESTS) */

#define tor_assert_unreached()                                  \
  STMT_BEGIN {                                                  \
    tor_assertion_failed_(SHORT_FILE__, __LINE__, __func__,     \
                          "line should be unreached");          \
    tor_abort_();                                               \
  } STMT_END

/* Non-fatal bug assertions. The "unreached" variants mean "this line should
 * never be reached." The "once" variants mean "Don't log a warning more than
 * once".
 *
 * The 'BUG' macro checks a boolean condition and logs an error message if it
 * is true.  Example usage:
 *   if (BUG(x == NULL))
 *     return -1;
 */

#ifdef __COVERITY__
#undef BUG
// Coverity defines this in global headers; let's override it.  This is a
// magic coverity-only preprocessor thing.
#nodef BUG(x) (x)
#endif /* defined(__COVERITY__) */

#if defined(__COVERITY__) || defined(__clang_analyzer__)
// We're running with a static analysis tool: let's treat even nonfatal
// assertion failures as something that we need to avoid.
#define ALL_BUGS_ARE_FATAL
#endif

#ifdef ALL_BUGS_ARE_FATAL
#define tor_assert_nonfatal_unreached() tor_assert(0)
#define tor_assert_nonfatal(cond) tor_assert((cond))
#define tor_assert_nonfatal_unreached_once() tor_assert(0)
#define tor_assert_nonfatal_once(cond) tor_assert((cond))
#define BUG(cond)                                                       \
  (ASSERT_PREDICT_UNLIKELY_(cond) ?                                     \
   (tor_assertion_failed_(SHORT_FILE__,__LINE__,__func__,"!("#cond")"), \
    tor_abort_(), 1)                                                    \
   : 0)
#elif defined(TOR_UNIT_TESTS) && defined(DISABLE_ASSERTS_IN_UNIT_TESTS)
#define tor_assert_nonfatal_unreached() STMT_NIL
#define tor_assert_nonfatal(cond) ((void)(cond))
#define tor_assert_nonfatal_unreached_once() STMT_NIL
#define tor_assert_nonfatal_once(cond) ((void)(cond))
#define BUG(cond) (ASSERT_PREDICT_UNLIKELY_(cond) ? 1 : 0)
#else /* Normal case, !ALL_BUGS_ARE_FATAL, !DISABLE_ASSERTS_IN_UNIT_TESTS */
#define tor_assert_nonfatal_unreached() STMT_BEGIN                      \
  tor_bug_occurred_(SHORT_FILE__, __LINE__, __func__, NULL, 0);         \
  STMT_END
#define tor_assert_nonfatal(cond) STMT_BEGIN                            \
  if (ASSERT_PREDICT_LIKELY_(cond)) {                                   \
  } else {                                                              \
    tor_bug_occurred_(SHORT_FILE__, __LINE__, __func__, #cond, 0);      \
  }                                                                     \
  STMT_END
#define tor_assert_nonfatal_unreached_once() STMT_BEGIN                 \
  static int warning_logged__ = 0;                                      \
  if (!warning_logged__) {                                              \
    warning_logged__ = 1;                                               \
    tor_bug_occurred_(SHORT_FILE__, __LINE__, __func__, NULL, 1);       \
  }                                                                     \
  STMT_END
#define tor_assert_nonfatal_once(cond) STMT_BEGIN                       \
  static int warning_logged__ = 0;                                      \
  if (ASSERT_PREDICT_LIKELY_(cond)) {                                   \
  } else if (!warning_logged__) {                                       \
    warning_logged__ = 1;                                               \
    tor_bug_occurred_(SHORT_FILE__, __LINE__, __func__, #cond, 1);      \
  }                                                                     \
  STMT_END
#define BUG(cond)                                                       \
  (ASSERT_PREDICT_UNLIKELY_(cond) ?                                     \
   (tor_bug_occurred_(SHORT_FILE__,__LINE__,__func__,"!("#cond")",0), 1) \
   : 0)
#endif /* defined(ALL_BUGS_ARE_FATAL) || ... */

#ifdef __GNUC__
#define IF_BUG_ONCE__(cond,var)                                         \
  if (( {                                                               \
      static int var = 0;                                               \
      int bool_result = !!(cond);                                       \
      if (bool_result && !var) {                                        \
        var = 1;                                                        \
        tor_bug_occurred_(SHORT_FILE__, __LINE__, __func__,             \
                          "!("#cond")", 1);                             \
      }                                                                 \
      bool_result; } ))
#else /* !(defined(__GNUC__)) */
#define IF_BUG_ONCE__(cond,var)                                         \
  static int var = 0;                                                   \
  if ((cond) ?                                                          \
      (var ? 1 :                                                        \
       (var=1,                                                          \
        tor_bug_occurred_(SHORT_FILE__, __LINE__, __func__,             \
                           "!("#cond")", 1),                            \
        1))                                                             \
      : 0)
#endif /* defined(__GNUC__) */
#define IF_BUG_ONCE_VARNAME_(a)               \
  warning_logged_on_ ## a ## __
#define IF_BUG_ONCE_VARNAME__(a)              \
  IF_BUG_ONCE_VARNAME_(a)

/** This macro behaves as 'if (bug(x))', except that it only logs its
 * warning once, no matter how many times it triggers.
 */

#define IF_BUG_ONCE(cond)                                       \
  IF_BUG_ONCE__(ASSERT_PREDICT_UNLIKELY_(cond),                 \
                IF_BUG_ONCE_VARNAME__(__LINE__))

/** Define this if you want Tor to crash when any problem comes up,
 * so you can get a coredump and track things down. */
// #define tor_fragile_assert() tor_assert_unreached(0)
#define tor_fragile_assert() tor_assert_nonfatal_unreached_once()

void tor_assertion_failed_(const char *fname, unsigned int line,
                           const char *func, const char *expr);
void tor_bug_occurred_(const char *fname, unsigned int line,
                       const char *func, const char *expr,
                       int once);

void tor_abort_(void) ATTR_NORETURN;

#ifdef _WIN32
#define SHORT_FILE__ (tor_fix_source_file(__FILE__))
const char *tor_fix_source_file(const char *fname);
#else
#define SHORT_FILE__ (__FILE__)
#define tor_fix_source_file(s) (s)
#endif /* defined(_WIN32) */

#ifdef TOR_UNIT_TESTS
void tor_capture_bugs_(int n);
void tor_end_capture_bugs_(void);
const struct smartlist_t *tor_get_captured_bug_log_(void);
void tor_set_failed_assertion_callback(void (*fn)(void));
#endif /* defined(TOR_UNIT_TESTS) */

#endif /* !defined(TOR_UTIL_BUG_H) */
