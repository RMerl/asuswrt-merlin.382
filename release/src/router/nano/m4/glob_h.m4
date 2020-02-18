# glob_h.m4 serial 5
dnl Copyright (C) 2018-2020 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

AC_DEFUN_ONCE([gl_GLOB_H],
[
  AC_REQUIRE([gl_GLOB_H_DEFAULTS])
  m4_ifdef([gl_ANSI_CXX], [AC_REQUIRE([gl_ANSI_CXX])])
  AC_REQUIRE([AC_C_RESTRICT])
  AC_CHECK_HEADERS_ONCE([glob.h])
  gl_CHECK_NEXT_HEADERS([glob.h])

  if test $ac_cv_header_glob_h = yes; then
    HAVE_GLOB_H=1
  else
    HAVE_GLOB_H=0
  fi
  AC_SUBST([HAVE_GLOB_H])

  m4_ifdef([gl_POSIXCHECK],
    [GLOB_H=glob.h],
    [GLOB_H=''
     if m4_ifdef([gl_ANSI_CXX], [test "$CXX" != no], [false]); then
       dnl Override <glob.h> always, to support the C++ GNULIB_NAMESPACE.
       GLOB_H=glob.h
     else
       if test $ac_cv_header_glob_h != yes; then
         dnl Provide a substitute <glob.h> file.
         GLOB_H=glob.h
       fi
     fi
    ])
  AC_SUBST([GLOB_H])
  AM_CONDITIONAL([GL_GENERATE_GLOB_H], [test -n "$GLOB_H"])

  dnl Check for declarations of anything we want to poison if the
  dnl corresponding gnulib module is not in use.
  gl_WARN_ON_USE_PREPARE([[#include <glob.h>
    ]],
    [glob globfree glob_pattern_p])
])

dnl Unconditionally enables the replacement of <glob.h>.
AC_DEFUN([gl_REPLACE_GLOB_H],
[
  AC_REQUIRE([gl_GLOB_H_DEFAULTS])
  GLOB_H='glob.h'
  AM_CONDITIONAL([GL_GENERATE_GLOB_H], [test -n "$GLOB_H"])
])

AC_DEFUN([gl_GLOB_MODULE_INDICATOR],
[
  dnl Use AC_REQUIRE here, so that the default settings are expanded once only.
  AC_REQUIRE([gl_GLOB_H_DEFAULTS])
  gl_MODULE_INDICATOR_SET_VARIABLE([$1])
  dnl Define it also as a C macro, for the benefit of the unit tests.
  gl_MODULE_INDICATOR_FOR_TESTS([$1])
])

AC_DEFUN([gl_GLOB_H_DEFAULTS],
[
  GNULIB_GLOB=0;             AC_SUBST([GNULIB_GLOB])
  dnl Assume POSIX and GNU behavior unless another module says otherwise.
  HAVE_GLOB=1;               AC_SUBST([HAVE_GLOB])
  HAVE_GLOB_PATTERN_P=1;     AC_SUBST([HAVE_GLOB_PATTERN_P])
  REPLACE_GLOB=0;            AC_SUBST([REPLACE_GLOB])
  REPLACE_GLOB_PATTERN_P=0;  AC_SUBST([REPLACE_GLOB_PATTERN_P])
])
