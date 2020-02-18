# serial 8
# See if we need to provide futimens replacement.

dnl Copyright (C) 2009-2020 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Written by Eric Blake.

AC_DEFUN([gl_FUNC_FUTIMENS],
[
  AC_REQUIRE([gl_SYS_STAT_H_DEFAULTS])
  AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])
  AC_CHECK_FUNCS_ONCE([futimens])
  if test $ac_cv_func_futimens = no; then
    HAVE_FUTIMENS=0
  else
    AC_CACHE_CHECK([whether futimens works],
      [gl_cv_func_futimens_works],
      [AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
]], [[struct timespec ts[2];
      int fd = creat ("conftest.file", 0600);
      struct stat st;
      if (fd < 0) return 1;
      ts[0].tv_sec = 1;
      ts[0].tv_nsec = UTIME_OMIT;
      ts[1].tv_sec = 1;
      ts[1].tv_nsec = UTIME_NOW;
      errno = 0;
      if (futimens (AT_FDCWD, NULL) == 0) return 2;
      if (errno != EBADF) return 3;
      if (futimens (fd, ts)) return 4;
      sleep (1);
      ts[0].tv_nsec = UTIME_NOW;
      ts[1].tv_nsec = UTIME_OMIT;
      if (futimens (fd, ts)) return 5;
      if (fstat (fd, &st)) return 6;
      if (st.st_ctime < st.st_atime) return 7;
      ]])],
         [gl_cv_func_futimens_works=yes],
         [gl_cv_func_futimens_works=no],
         [case "$host_os" in
                           # Guess no on glibc systems.
            *-gnu* | gnu*) gl_cv_func_futimens_works="guessing no" ;;
                           # Guess no on musl systems.
            *-musl*)       gl_cv_func_futimens_works="guessing no" ;;
                           # Guess yes otherwise.
            *)             gl_cv_func_futimens_works="guessing yes" ;;
          esac
         ])
      rm -f conftest.file])
    case "$gl_cv_func_futimens_works" in
      *yes) ;;
      *)
        REPLACE_FUTIMENS=1
        ;;
    esac
  fi
])
