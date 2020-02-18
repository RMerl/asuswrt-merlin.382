# glob.m4 serial 23
dnl Copyright (C) 2005-2007, 2009-2020 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# The glob module assumes you want GNU glob, with glob_pattern_p etc,
# rather than vanilla POSIX glob.  This means your code should
# always include <glob.h> for the glob prototypes.

AC_DEFUN([gl_GLOB],
[
  AC_REQUIRE([gl_GLOB_H])

  AC_CHECK_FUNCS_ONCE([glob glob_pattern_p])
  if test $ac_cv_func_glob = no; then
    HAVE_GLOB=0
  else

    AC_CACHE_CHECK([for GNU glob interface version 1 or 2],
      [gl_cv_gnu_glob_interface_version_1_2],
[     AC_COMPILE_IFELSE([AC_LANG_SOURCE(
[[#include <gnu-versions.h>
char a[_GNU_GLOB_INTERFACE_VERSION == 1 || _GNU_GLOB_INTERFACE_VERSION == 2 ? 1 : -1];]])],
        [gl_cv_gnu_glob_interface_version_1_2=yes],
        [gl_cv_gnu_glob_interface_version_1_2=no])])
    if test "$gl_cv_gnu_glob_interface_version_1_2" = "no"; then
      REPLACE_GLOB=1
    fi

    if test $REPLACE_GLOB = 0; then
      AC_CACHE_CHECK([whether glob lists broken symlinks],
                     [gl_cv_glob_lists_symlinks],
        [if test $cross_compiling != yes; then
           if ln -s conf-doesntexist conf$$-globtest 2>/dev/null; then
             gl_cv_glob_lists_symlinks=maybe
           else
             # If we can't make a symlink, then we cannot test this issue.  Be
             # pessimistic about this.
             gl_cv_glob_lists_symlinks=no
           fi
           if test $gl_cv_glob_lists_symlinks = maybe; then
             AC_RUN_IFELSE(
               [AC_LANG_PROGRAM(
                  [[#include <stddef.h>
                    #include <glob.h>]],
                  [[glob_t found;
                    if (glob ("conf*-globtest", 0, NULL, &found) == GLOB_NOMATCH)
                      return 1;
                  ]])],
               [gl_cv_glob_lists_symlinks=yes],
               [gl_cv_glob_lists_symlinks=no],
               [dnl We don't get here.
                :
               ])
           fi
           rm -f conf$$-globtest
         else
           gl_cv_glob_lists_symlinks="$gl_cross_guess_normal"
         fi
        ])
      case "$gl_cv_glob_lists_symlinks" in
        *yes) ;;
        *) REPLACE_GLOB=1 ;;
      esac
    fi

  fi

  if test $ac_cv_func_glob_pattern_p = no; then
    HAVE_GLOB_PATTERN_P=0
  else
    if test $REPLACE_GLOB = 1; then
      REPLACE_GLOB_PATTERN_P=1
    fi
  fi

  if test $HAVE_GLOB = 0 || test $REPLACE_GLOB = 1; then
    gl_REPLACE_GLOB_H
  fi
])

# Prerequisites of lib/glob.c and lib/globfree.c.
AC_DEFUN([gl_PREREQ_GLOB],
[
  AC_REQUIRE([gl_CHECK_TYPE_STRUCT_DIRENT_D_TYPE])
  AC_CHECK_HEADERS_ONCE([unistd.h])
  AC_CHECK_FUNCS_ONCE([getlogin_r getpwnam_r])
])
