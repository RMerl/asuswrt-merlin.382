/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file printf.c
 * \brief Compatibility wrappers around snprintf and its friends
 **/

#include "lib/string/printf.h"
#include "lib/err/torerr.h"
#include "lib/cc/torint.h"
#include "lib/malloc/malloc.h"

#include <stdlib.h>
#include <stdio.h>

/** Replacement for snprintf.  Differs from platform snprintf in two
 * ways: First, always NUL-terminates its output.  Second, always
 * returns -1 if the result is truncated.  (Note that this return
 * behavior does <i>not</i> conform to C99; it just happens to be
 * easier to emulate "return -1" with conformant implementations than
 * it is to emulate "return number that would be written" with
 * non-conformant implementations.) */
int
tor_snprintf(char *str, size_t size, const char *format, ...)
{
  va_list ap;
  int r;
  va_start(ap,format);
  r = tor_vsnprintf(str,size,format,ap);
  va_end(ap);
  return r;
}

/** Replacement for vsnprintf; behavior differs as tor_snprintf differs from
 * snprintf.
 */
int
tor_vsnprintf(char *str, size_t size, const char *format, va_list args)
{
  int r;
  if (size == 0)
    return -1; /* no place for the NUL */
  if (size > SIZE_T_CEILING)
    return -1;
#ifdef _WIN32
  r = _vsnprintf(str, size, format, args);
#else
  r = vsnprintf(str, size, format, args);
#endif
  str[size-1] = '\0';
  if (r < 0 || r >= (ssize_t)size)
    return -1;
  return r;
}

/**
 * Portable asprintf implementation.  Does a printf() into a newly malloc'd
 * string.  Sets *<b>strp</b> to this string, and returns its length (not
 * including the terminating NUL character).
 *
 * You can treat this function as if its implementation were something like
   <pre>
     char buf[_INFINITY_];
     tor_snprintf(buf, sizeof(buf), fmt, args);
     *strp = tor_strdup(buf);
     return strlen(*strp):
   </pre>
 * Where _INFINITY_ is an imaginary constant so big that any string can fit
 * into it.
 */
int
tor_asprintf(char **strp, const char *fmt, ...)
{
  int r;
  va_list args;
  va_start(args, fmt);
  r = tor_vasprintf(strp, fmt, args);
  va_end(args);
  if (!*strp || r < 0) {
    /* LCOV_EXCL_START */
    raw_assert_unreached_msg("Internal error in asprintf");
    /* LCOV_EXCL_STOP */
  }
  return r;
}

/**
 * Portable vasprintf implementation.  Does a printf() into a newly malloc'd
 * string.  Differs from regular vasprintf in the same ways that
 * tor_asprintf() differs from regular asprintf.
 */
int
tor_vasprintf(char **strp, const char *fmt, va_list args)
{
  /* use a temporary variable in case *strp is in args. */
  char *strp_tmp=NULL;
#ifdef HAVE_VASPRINTF
  /* If the platform gives us one, use it. */
  int r = vasprintf(&strp_tmp, fmt, args);
  if (r < 0)
    *strp = NULL; // LCOV_EXCL_LINE -- no cross-platform way to force this
  else
    *strp = strp_tmp;
  return r;
#elif defined(HAVE__VSCPRINTF)
  /* On Windows, _vsnprintf won't tell us the length of the string if it
   * overflows, so we need to use _vcsprintf to tell how much to allocate */
  int len, r;
  va_list tmp_args;
  va_copy(tmp_args, args);
  len = _vscprintf(fmt, tmp_args);
  va_end(tmp_args);
  if (len < 0) {
    *strp = NULL;
    return -1;
  }
  strp_tmp = tor_malloc(len + 1);
  r = _vsnprintf(strp_tmp, len+1, fmt, args);
  if (r != len) {
    tor_free(strp_tmp);
    *strp = NULL;
    return -1;
  }
  *strp = strp_tmp;
  return len;
#else
  /* Everywhere else, we have a decent vsnprintf that tells us how many
   * characters we need.  We give it a try on a short buffer first, since
   * it might be nice to avoid the second vsnprintf call.
   */
  /* XXXX This code spent a number of years broken (see bug 30651). It is
   * possible that no Tor users actually run on systems without vasprintf() or
   * _vscprintf(). If so, we should consider removing this code. */
  char buf[128];
  int len, r;
  va_list tmp_args;
  va_copy(tmp_args, args);
  /* Use vsnprintf to retrieve needed length.  tor_vsnprintf() is not an
   * option here because it will simply return -1 if buf is not large enough
   * to hold the complete string.
   */
  len = vsnprintf(buf, sizeof(buf), fmt, tmp_args);
  va_end(tmp_args);
  buf[sizeof(buf) - 1] = '\0';
  if (len < 0) {
    *strp = NULL;
    return -1;
  }
  if (len < (int)sizeof(buf)) {
    *strp = tor_strdup(buf);
    return len;
  }
  strp_tmp = tor_malloc(len+1);
  /* use of tor_vsnprintf() will ensure string is null terminated */
  r = tor_vsnprintf(strp_tmp, len+1, fmt, args);
  if (r != len) {
    tor_free(strp_tmp);
    *strp = NULL;
    return -1;
  }
  *strp = strp_tmp;
  return len;
#endif /* defined(HAVE_VASPRINTF) || ... */
}
