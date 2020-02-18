/* Copyright (c) 2013-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file backtrace.c
 *
 * \brief Functions to produce backtraces on bugs, crashes, or assertion
 * failures.
 *
 * Currently, we've only got an implementation here using the backtrace()
 * family of functions, which are sometimes provided by libc and sometimes
 * provided by libexecinfo.  We tie into the sigaction() backend in order to
 * detect crashes.
 *
 * This is one of the lowest-level modules, since nearly everything needs to
 * be able to log an error.  As such, it doesn't call the log module or any
 * other higher-level modules directly.
 */

#include "orconfig.h"
#include "lib/err/torerr.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CYGWIN_SIGNAL_H
#include <cygwin/signal.h>
#elif defined(HAVE_SYS_UCONTEXT_H)
#include <sys/ucontext.h>
#elif defined(HAVE_UCONTEXT_H)
#include <ucontext.h>
#endif /* defined(HAVE_CYGWIN_SIGNAL_H) || ... */

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#define EXPOSE_CLEAN_BACKTRACE
#include "lib/err/backtrace.h"
#include "lib/err/torerr.h"

#if defined(HAVE_EXECINFO_H) && defined(HAVE_BACKTRACE) && \
  defined(HAVE_BACKTRACE_SYMBOLS_FD) && defined(HAVE_SIGACTION)
#define USE_BACKTRACE
#endif

#if !defined(USE_BACKTRACE)
#define NO_BACKTRACE_IMPL
#endif

// Redundant with util.h, but doing it here so we can avoid that dependency.
#define raw_free free

/** Version of Tor to report in backtrace messages. */
static char bt_version[128] = "";

#ifdef USE_BACKTRACE
/** Largest stack depth to try to dump. */
#define MAX_DEPTH 256
/** Static allocation of stack to dump. This is static so we avoid stack
 * pressure. */
static void *cb_buf[MAX_DEPTH];
/** Protects cb_buf from concurrent access. Pthreads, since this code
 * is Unix-only, and since this code needs to be lowest-level. */
static pthread_mutex_t cb_buf_mutex = PTHREAD_MUTEX_INITIALIZER;

/** Change a stacktrace in <b>stack</b> of depth <b>depth</b> so that it will
 * log the correct function from which a signal was received with context
 * <b>ctx</b>.  (When we get a signal, the current function will not have
 * called any other function, and will therefore have not pushed its address
 * onto the stack.  Fortunately, we usually have the program counter in the
 * ucontext_t structure.
 */
void
clean_backtrace(void **stack, size_t depth, const ucontext_t *ctx)
{
#ifdef PC_FROM_UCONTEXT
#if defined(__linux__)
  const size_t n = 1;
#elif defined(__darwin__) || defined(__APPLE__) || defined(OpenBSD) \
  || defined(__FreeBSD__)
  const size_t n = 2;
#else
  const size_t n = 1;
#endif /* defined(__linux__) || ... */
  if (depth <= n)
    return;

  stack[n] = (void*) ctx->PC_FROM_UCONTEXT;
#else /* !(defined(PC_FROM_UCONTEXT)) */
  (void) depth;
  (void) ctx;
  (void) stack;
#endif /* defined(PC_FROM_UCONTEXT) */
}

/** Log a message <b>msg</b> at <b>severity</b> in <b>domain</b>, and follow
 * that with a backtrace log.  Send messages via the tor_log function at
 * logger". */
void
log_backtrace_impl(int severity, int domain, const char *msg,
                   tor_log_fn logger)
{
  size_t depth;
  char **symbols;
  size_t i;

  pthread_mutex_lock(&cb_buf_mutex);

  depth = backtrace(cb_buf, MAX_DEPTH);
  symbols = backtrace_symbols(cb_buf, (int)depth);

  logger(severity, domain, "%s: %s. Stack trace:", bt_version, msg);
  if (!symbols) {
    /* LCOV_EXCL_START -- we can't provoke this. */
    logger(severity, domain, "    Unable to generate backtrace.");
    goto done;
    /* LCOV_EXCL_STOP */
  }
  for (i=0; i < depth; ++i) {
    logger(severity, domain, "    %s", symbols[i]);
  }
  raw_free(symbols);

 done:
  pthread_mutex_unlock(&cb_buf_mutex);
}

static void crash_handler(int sig, siginfo_t *si, void *ctx_)
  __attribute__((noreturn));

/** Signal handler: write a crash message with a stack trace, and die. */
static void
crash_handler(int sig, siginfo_t *si, void *ctx_)
{
  char buf[40];
  size_t depth;
  ucontext_t *ctx = (ucontext_t *) ctx_;
  int n_fds, i;
  const int *fds = NULL;

  (void) si;

  depth = backtrace(cb_buf, MAX_DEPTH);
  /* Clean up the top stack frame so we get the real function
   * name for the most recently failing function. */
  clean_backtrace(cb_buf, depth, ctx);

  format_dec_number_sigsafe((unsigned)sig, buf, sizeof(buf));

  tor_log_err_sigsafe(bt_version, " died: Caught signal ", buf, "\n",
                      NULL);

  n_fds = tor_log_get_sigsafe_err_fds(&fds);
  for (i=0; i < n_fds; ++i)
    backtrace_symbols_fd(cb_buf, (int)depth, fds[i]);

  abort();
}

/** Write a backtrace to all of the emergency-error fds. */
void
dump_stack_symbols_to_error_fds(void)
{
  int n_fds, i;
  const int *fds = NULL;
  size_t depth;

  depth = backtrace(cb_buf, MAX_DEPTH);

  n_fds = tor_log_get_sigsafe_err_fds(&fds);
  for (i=0; i < n_fds; ++i)
    backtrace_symbols_fd(cb_buf, (int)depth, fds[i]);
}

/** Install signal handlers as needed so that when we crash, we produce a
 * useful stack trace. Return 0 on success, -errno on failure. */
static int
install_bt_handler(void)
{
  int trap_signals[] = { SIGSEGV, SIGILL, SIGFPE, SIGBUS, SIGSYS,
                         SIGIO, -1 };
  int i, rv=0;

  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = crash_handler;
  sa.sa_flags = SA_SIGINFO;
  sigfillset(&sa.sa_mask);

  for (i = 0; trap_signals[i] >= 0; ++i) {
    if (sigaction(trap_signals[i], &sa, NULL) == -1) {
      /* LCOV_EXCL_START */
      rv = -errno;
      /* LCOV_EXCL_STOP */
    }
  }

  {
    /* Now, generate (but do not log) a backtrace.  This ensures that
     * libc has pre-loaded the symbols we need to dump things, so that later
     * reads won't be denied by the sandbox code */
    char **symbols;
    size_t depth = backtrace(cb_buf, MAX_DEPTH);
    symbols = backtrace_symbols(cb_buf, (int) depth);
    if (symbols)
      raw_free(symbols);
  }

  return rv;
}

/** Uninstall crash handlers. */
static void
remove_bt_handler(void)
{
}
#endif /* defined(USE_BACKTRACE) */

#ifdef NO_BACKTRACE_IMPL
void
log_backtrace_impl(int severity, int domain, const char *msg,
                   tor_log_fn logger)
{
  logger(severity, domain, "%s: %s. (Stack trace not available)",
         bt_version, msg);
}

static int
install_bt_handler(void)
{
  return 0;
}

static void
remove_bt_handler(void)
{
}

void
dump_stack_symbols_to_error_fds(void)
{
}
#endif /* defined(NO_BACKTRACE_IMPL) */

/** Return the tor version used for error messages on crashes.
 * Signal-safe: returns a pointer to a static array. */
const char *
get_tor_backtrace_version(void)
{
  return bt_version;
}

/** Set up code to handle generating error messages on crashes. */
int
configure_backtrace_handler(const char *tor_version)
{
  char version[128] = "Tor\0";

  if (tor_version) {
    int snp_rv = 0;
    /* We can't use strlcat() here, because it is defined in
     * string/compat_string.h on some platforms, and string uses torerr. */
    snp_rv = snprintf(version, sizeof(version), "Tor %s", tor_version);
    /* It's safe to call raw_assert() here, because raw_assert() does not
     * call configure_backtrace_handler(). */
    raw_assert(snp_rv < (int)sizeof(version));
    raw_assert(snp_rv >= 0);
  }

  char *str_rv = NULL;
  /* We can't use strlcpy() here, see the note about strlcat() above. */
  str_rv = strncpy(bt_version, version, sizeof(bt_version) - 1);
  /* We must terminate bt_version, then raw_assert(), because raw_assert()
   * uses bt_version. */
  bt_version[sizeof(bt_version) - 1] = 0;
  raw_assert(str_rv == bt_version);

  return install_bt_handler();
}

/** Perform end-of-process cleanup for code that generates error messages on
 * crashes.  */
void
clean_up_backtrace_handler(void)
{
  remove_bt_handler();
}
