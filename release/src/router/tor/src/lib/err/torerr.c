/* Copyright (c) 2001, Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file torerr.c
 *
 * \brief Handling code for unrecoverable emergencies, at a lower level
 *   than the logging code.
 *
 * There are plenty of places that things can go wrong in Tor's backend
 * libraries: the allocator can fail, the locking subsystem can fail, and so
 * on.  But since these subsystems are used themselves by the logging module,
 * they can't use the logging code directly to report their errors.
 *
 * As a workaround, the logging code provides this module with a set of raw
 * fds to be used for reporting errors in the lowest-level Tor code.
 */

#include "orconfig.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "lib/err/torerr.h"
#include "lib/err/backtrace.h"

/** Array of fds to log crash-style warnings to. */
static int sigsafe_log_fds[TOR_SIGSAFE_LOG_MAX_FDS] = { STDERR_FILENO };
/** The number of elements used in sigsafe_log_fds */
static int n_sigsafe_log_fds = 1;
/** Log granularity in milliseconds. */
static int log_granularity = 1000;

/** Write <b>s</b> to each element of sigsafe_log_fds. Return 0 on success, -1
 * on failure. */
static int
tor_log_err_sigsafe_write(const char *s)
{
  int i;
  ssize_t r;
  size_t len = strlen(s);
  int err = 0;
  for (i=0; i < n_sigsafe_log_fds; ++i) {
    r = write(sigsafe_log_fds[i], s, len);
    err += (r != (ssize_t)len);
  }
  return err ? -1 : 0;
}

/** Given a list of string arguments ending with a NULL, writes them
 * to our logs and to stderr (if possible).  This function is safe to call
 * from within a signal handler. */
void
tor_log_err_sigsafe(const char *m, ...)
{
  va_list ap;
  const char *x;
  char timebuf[33];
  time_t now = time(NULL);

  if (!m)
    return;
  if (log_granularity >= 2000) {
    int g = log_granularity / 1000;
    now -= now % g;
  }
  timebuf[0] = now < 0 ? '-' : ' ';
  if (now < 0) now = -now;
  timebuf[1] = '\0';
  format_dec_number_sigsafe(now, timebuf+1, sizeof(timebuf)-1);
  tor_log_err_sigsafe_write("\n=========================================="
                             "================== T=");
  tor_log_err_sigsafe_write(timebuf);
  tor_log_err_sigsafe_write("\n");
  tor_log_err_sigsafe_write(m);
  va_start(ap, m);
  while ((x = va_arg(ap, const char*))) {
    tor_log_err_sigsafe_write(x);
  }
  va_end(ap);
}

/** Set *<b>out</b> to a pointer to an array of the fds to log errors to from
 * inside a signal handler or other emergency condition. Return the number of
 * elements in the array. */
int
tor_log_get_sigsafe_err_fds(const int **out)
{
  *out = sigsafe_log_fds;
  return n_sigsafe_log_fds;
}

/**
 * Update the list of fds that get errors from inside a signal handler or
 * other emergency condition. Ignore any beyond the first
 * TOR_SIGSAFE_LOG_MAX_FDS.
 */
void
tor_log_set_sigsafe_err_fds(const int *fds, int n)
{
  if (n > TOR_SIGSAFE_LOG_MAX_FDS) {
    n = TOR_SIGSAFE_LOG_MAX_FDS;
  }

  memcpy(sigsafe_log_fds, fds, n * sizeof(int));
  n_sigsafe_log_fds = n;
}

/**
 * Set the granularity (in ms) to use when reporting fatal errors outside
 * the logging system.
 */
void
tor_log_sigsafe_err_set_granularity(int ms)
{
  log_granularity = ms;
}

/**
 * Log an emergency assertion failure message.
 *
 * This kind of message is safe to send from within a log handler,
 * a signal handler, or other emergency situation.
 */
void
tor_raw_assertion_failed_msg_(const char *file, int line, const char *expr,
                              const char *msg)
{
  char linebuf[16];
  format_dec_number_sigsafe(line, linebuf, sizeof(linebuf));
  tor_log_err_sigsafe("INTERNAL ERROR: Raw assertion failed in ",
                      get_tor_backtrace_version(), " at ",
                      file, ":", linebuf, ": ", expr, "\n", NULL);
  if (msg) {
    tor_log_err_sigsafe_write(msg);
    tor_log_err_sigsafe_write("\n");
  }

  dump_stack_symbols_to_error_fds();

  /* Some platforms (macOS, maybe others?) can swallow the last write before an
   * abort. This issue is probably caused by a race condition between write
   * buffer cache flushing, and process termination. So we write an extra
   * newline, to make sure that the message always gets through. */
  tor_log_err_sigsafe_write("\n");
}

/* As format_{hex,dex}_number_sigsafe, but takes a <b>radix</b> argument
 * in range 2..16 inclusive. */
static int
format_number_sigsafe(unsigned long x, char *buf, int buf_len,
                      unsigned int radix)
{
  unsigned long tmp;
  int len;
  char *cp;

  /* NOT tor_assert. This needs to be safe to run from within a signal
   * handler, and from within the 'tor_assert() has failed' code.  Not even
   * raw_assert(), since raw_assert() calls this function on failure. */
  if (radix < 2 || radix > 16)
    return 0;

  /* Count how many digits we need. */
  tmp = x;
  len = 1;
  while (tmp >= radix) {
    tmp /= radix;
    ++len;
  }

  /* Not long enough */
  if (!buf || len >= buf_len)
    return 0;

  cp = buf + len;
  *cp = '\0';
  do {
    unsigned digit = (unsigned) (x % radix);
    if (cp <= buf) {
      /* Not tor_assert(); see above. */
      abort();
    }
    --cp;
    *cp = "0123456789ABCDEF"[digit];
    x /= radix;
  } while (x);

  /* NOT tor_assert; see above. */
  if (cp != buf) {
    abort(); // LCOV_EXCL_LINE
  }

  return len;
}

/**
 * Helper function to output hex numbers from within a signal handler.
 *
 * Writes the nul-terminated hexadecimal digits of <b>x</b> into a buffer
 * <b>buf</b> of size <b>buf_len</b>, and return the actual number of digits
 * written, not counting the terminal NUL.
 *
 * If there is insufficient space, write nothing and return 0.
 *
 * This accepts an unsigned int because format_helper_exit_status() needs to
 * call it with a signed int and an unsigned char, and since the C standard
 * does not guarantee that an int is wider than a char (an int must be at
 * least 16 bits but it is permitted for a char to be that wide as well), we
 * can't assume a signed int is sufficient to accommodate an unsigned char.
 * Thus, format_helper_exit_status() will still need to emit any require '-'
 * on its own.
 *
 * For most purposes, you'd want to use tor_snprintf("%x") instead of this
 * function; it's designed to be used in code paths where you can't call
 * arbitrary C functions.
 */
int
format_hex_number_sigsafe(unsigned long x, char *buf, int buf_len)
{
  return format_number_sigsafe(x, buf, buf_len, 16);
}

/** As format_hex_number_sigsafe, but format the number in base 10. */
int
format_dec_number_sigsafe(unsigned long x, char *buf, int buf_len)
{
  return format_number_sigsafe(x, buf, buf_len, 10);
}
