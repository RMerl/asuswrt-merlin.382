/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file meminfo.c
 *
 * \brief Functions to query total memory, and access meta-information about
 * the allocator.
 **/

#include "lib/meminfo/meminfo.h"

#include "lib/cc/compat_compiler.h"
#include "lib/cc/torint.h"
#include "lib/fs/files.h"
#include "lib/log/log.h"
#include "lib/malloc/malloc.h"

#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif
#include <string.h>

DISABLE_GCC_WARNING(aggregate-return)
/** Call the platform malloc info function, and dump the results to the log at
 * level <b>severity</b>.  If no such function exists, do nothing. */
void
tor_log_mallinfo(int severity)
{
#ifdef HAVE_MALLINFO
  struct mallinfo mi;
  memset(&mi, 0, sizeof(mi));
  mi = mallinfo();
  tor_log(severity, LD_MM,
      "mallinfo() said: arena=%d, ordblks=%d, smblks=%d, hblks=%d, "
      "hblkhd=%d, usmblks=%d, fsmblks=%d, uordblks=%d, fordblks=%d, "
      "keepcost=%d",
      mi.arena, mi.ordblks, mi.smblks, mi.hblks,
      mi.hblkhd, mi.usmblks, mi.fsmblks, mi.uordblks, mi.fordblks,
      mi.keepcost);
#else /* !(defined(HAVE_MALLINFO)) */
  (void)severity;
#endif /* defined(HAVE_MALLINFO) */
}
ENABLE_GCC_WARNING(aggregate-return)

#if defined(HW_PHYSMEM64)
/* OpenBSD and NetBSD define this */
#define INT64_HW_MEM HW_PHYSMEM64
#elif defined(HW_MEMSIZE)
/* OSX defines this one */
#define INT64_HW_MEM HW_MEMSIZE
#endif /* defined(HW_PHYSMEM64) || ... */

/**
 * Helper: try to detect the total system memory, and return it. On failure,
 * return 0.
 */
static uint64_t
get_total_system_memory_impl(void)
{
#if defined(__linux__)
  /* On linux, sysctl is deprecated. Because proc is so awesome that you
   * shouldn't _want_ to write portable code, I guess? */
  unsigned long long result=0;
  int fd = -1;
  char *s = NULL;
  const char *cp;
  size_t file_size=0;
  if (-1 == (fd = tor_open_cloexec("/proc/meminfo",O_RDONLY,0)))
    return 0;
  s = read_file_to_str_until_eof(fd, 65536, &file_size);
  if (!s)
    goto err;
  cp = strstr(s, "MemTotal:");
  if (!cp)
    goto err;
  /* Use the system sscanf so that space will match a wider number of space */
  if (sscanf(cp, "MemTotal: %llu kB\n", &result) != 1)
    goto err;

  close(fd);
  tor_free(s);
  return result * 1024;

  /* LCOV_EXCL_START Can't reach this unless proc is broken. */
 err:
  tor_free(s);
  close(fd);
  return 0;
  /* LCOV_EXCL_STOP */
#elif defined (_WIN32)
  /* Windows has MEMORYSTATUSEX; pretty straightforward. */
  MEMORYSTATUSEX ms;
  memset(&ms, 0, sizeof(ms));
  ms.dwLength = sizeof(ms);
  if (! GlobalMemoryStatusEx(&ms))
    return 0;

  return ms.ullTotalPhys;

#elif defined(HAVE_SYSCTL) && defined(INT64_HW_MEM)
  /* On many systems, HW_PHYSMEM is clipped to 32 bits; let's use a better
   * variant if we know about it. */
  uint64_t memsize = 0;
  size_t len = sizeof(memsize);
  int mib[2] = {CTL_HW, INT64_HW_MEM};
  if (sysctl(mib,2,&memsize,&len,NULL,0))
    return 0;

  return memsize;

#elif defined(HAVE_SYSCTL) && defined(HW_PHYSMEM)
  /* On some systems (like FreeBSD I hope) you can use a size_t with
   * HW_PHYSMEM. */
  size_t memsize=0;
  size_t len = sizeof(memsize);
  int mib[2] = {CTL_HW, HW_PHYSMEM};
  if (sysctl(mib,2,&memsize,&len,NULL,0))
    return 0;

  return memsize;

#else
  /* I have no clue. */
  return 0;
#endif /* defined(__linux__) || ... */
}

/**
 * Try to find out how much physical memory the system has. On success,
 * return 0 and set *<b>mem_out</b> to that value. On failure, return -1.
 */
MOCK_IMPL(int,
get_total_system_memory, (size_t *mem_out))
{
  static size_t mem_cached=0;
  uint64_t m = get_total_system_memory_impl();
  if (0 == m) {
    /* LCOV_EXCL_START -- can't make this happen without mocking. */
    /* We couldn't find our memory total */
    if (0 == mem_cached) {
      /* We have no cached value either */
      *mem_out = 0;
      return -1;
    }

    *mem_out = mem_cached;
    return 0;
    /* LCOV_EXCL_STOP */
  }

#if SIZE_MAX != UINT64_MAX
  if (m > SIZE_MAX) {
    /* I think this could happen if we're a 32-bit Tor running on a 64-bit
     * system: we could have more system memory than would fit in a
     * size_t. */
    m = SIZE_MAX;
  }
#endif /* SIZE_MAX != UINT64_MAX */

  *mem_out = mem_cached = (size_t) m;

  return 0;
}
