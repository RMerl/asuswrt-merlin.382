/* Copyright (c) 2003, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file fp.c
 *
 * \brief Basic floating-point compatibility and convenience code.
 **/

#include "orconfig.h"
#include "lib/math/fp.h"

#include <math.h>

/**
 * Returns the natural logarithm of d base e.  We defined this wrapper here so
 * to avoid conflicts with old versions of tor_log(), which were named log().
 */
double
tor_mathlog(double d)
{
  return log(d);
}

/** Return the long integer closest to <b>d</b>. We define this wrapper
 * here so that not all users of math.h need to use the right incantations
 * to get the c99 functions. */
long
tor_lround(double d)
{
#if defined(HAVE_LROUND)
  return lround(d);
#elif defined(HAVE_RINT)
  return (long)rint(d);
#else
  return (long)(d > 0 ? d + 0.5 : ceil(d - 0.5));
#endif /* defined(HAVE_LROUND) || ... */
}

/** Return the 64-bit integer closest to d.  We define this wrapper here so
 * that not all users of math.h need to use the right incantations to get the
 * c99 functions. */
int64_t
tor_llround(double d)
{
#if defined(HAVE_LLROUND)
  return (int64_t)llround(d);
#elif defined(HAVE_RINT)
  return (int64_t)rint(d);
#else
  return (int64_t)(d > 0 ? d + 0.5 : ceil(d - 0.5));
#endif /* defined(HAVE_LLROUND) || ... */
}

/** Cast a given double value to a int64_t. Return 0 if number is NaN.
 * Returns either INT64_MIN or INT64_MAX if number is outside of the int64_t
 * range. */
int64_t
clamp_double_to_int64(double number)
{
  int exponent;

#if (defined(MINGW_ANY)||defined(__FreeBSD__)) && GCC_VERSION >= 409
/*
  Mingw's math.h uses gcc's __builtin_choose_expr() facility to declare
  isnan, isfinite, and signbit.  But as implemented in at least some
  versions of gcc, __builtin_choose_expr() can generate type warnings
  even from branches that are not taken.  So, suppress those warnings.

  FreeBSD's math.h uses an __fp_type_select() macro, which dispatches
  based on sizeof -- again, this can generate type warnings from
  branches that are not taken.
*/
#define PROBLEMATIC_FLOAT_CONVERSION_WARNING
DISABLE_GCC_WARNING(float-conversion)
#endif /* defined(MINGW_ANY) && GCC_VERSION >= 409 */

/*
  With clang 4.0 we apparently run into "double promotion" warnings here,
  since clang thinks we're promoting a double to a long double.
 */
#if defined(__clang__)
#if __has_warning("-Wdouble-promotion")
#define PROBLEMATIC_DOUBLE_PROMOTION_WARNING
DISABLE_GCC_WARNING(double-promotion)
#endif
#endif /* defined(__clang__) */

  /* NaN is a special case that can't be used with the logic below. */
  if (isnan(number)) {
    return 0;
  }

  /* Time to validate if result can overflows a int64_t value. Fun with
   * float! Find that exponent exp such that
   *    number == x * 2^exp
   * for some x with abs(x) in [0.5, 1.0). Note that this implies that the
   * magnitude of number is strictly less than 2^exp.
   *
   * If number is infinite, the call to frexp is legal but the contents of
   * are exponent unspecified. */
  frexp(number, &exponent);

  /* If the magnitude of number is strictly less than 2^63, the truncated
   * version of number is guaranteed to be representable. The only
   * representable integer for which this is not the case is INT64_MIN, but
   * it is covered by the logic below. */
  if (isfinite(number) && exponent <= 63) {
    return (int64_t)number;
  }

  /* Handle infinities and finite numbers with magnitude >= 2^63. */
  return signbit(number) ? INT64_MIN : INT64_MAX;

#ifdef PROBLEMATIC_DOUBLE_PROMOTION_WARNING
ENABLE_GCC_WARNING(double-promotion)
#endif
#ifdef PROBLEMATIC_FLOAT_CONVERSION_WARNING
ENABLE_GCC_WARNING(float-conversion)
#endif
}
