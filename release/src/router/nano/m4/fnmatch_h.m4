# fnmatch_h.m4 serial 4
dnl Copyright (C) 2009-2020 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

AC_DEFUN_ONCE([gl_FNMATCH_H],
[
  AC_REQUIRE([gl_FNMATCH_H_DEFAULTS])
  m4_ifdef([gl_ANSI_CXX], [AC_REQUIRE([gl_ANSI_CXX])])
  AC_CHECK_HEADERS_ONCE([fnmatch.h])
  gl_CHECK_NEXT_HEADERS([fnmatch.h])

  dnl Persuade glibc <fnmatch.h> to declare FNM_CASEFOLD etc.
  dnl This is only needed if gl_fnmatch_required = GNU. It would be possible
  dnl to avoid this dependency for gl_FUNC_FNMATCH_POSIX by putting
  dnl gl_FUNC_FNMATCH_GNU into a separate .m4 file.
  AC_REQUIRE([AC_USE_SYSTEM_EXTENSIONS])

  if test $ac_cv_header_fnmatch_h = yes; then
    HAVE_FNMATCH_H=1
  else
    HAVE_FNMATCH_H=0
  fi
  AC_SUBST([HAVE_FNMATCH_H])

  m4_ifdef([gl_POSIXCHECK],
    [FNMATCH_H=fnmatch.h],
    [FNMATCH_H=''
     if m4_ifdef([gl_ANSI_CXX], [test "$CXX" != no], [false]); then
       dnl Override <fnmatch.h> always, to support the C++ GNULIB_NAMESPACE.
       FNMATCH_H=fnmatch.h
     else
       if test $ac_cv_header_fnmatch_h != yes; then
         dnl Provide a substitute <fnmatch.h> file.
         FNMATCH_H=fnmatch.h
       fi
     fi
    ])
  AC_SUBST([FNMATCH_H])
  AM_CONDITIONAL([GL_GENERATE_FNMATCH_H], [test -n "$FNMATCH_H"])

  dnl Check for declarations of anything we want to poison if the
  dnl corresponding gnulib module is not in use.
  gl_WARN_ON_USE_PREPARE([[#include <fnmatch.h>
    ]],
    [fnmatch])
])

dnl Unconditionally enables the replacement of <fnmatch.h>.
AC_DEFUN([gl_REPLACE_FNMATCH_H],
[
  AC_REQUIRE([gl_FNMATCH_H_DEFAULTS])
  FNMATCH_H='fnmatch.h'
  AM_CONDITIONAL([GL_GENERATE_FNMATCH_H], [test -n "$FNMATCH_H"])
])

AC_DEFUN([gl_FNMATCH_MODULE_INDICATOR],
[
  dnl Use AC_REQUIRE here, so that the default settings are expanded once only.
  AC_REQUIRE([gl_FNMATCH_H_DEFAULTS])
  gl_MODULE_INDICATOR_SET_VARIABLE([$1])
  dnl Define it also as a C macro, for the benefit of the unit tests.
  gl_MODULE_INDICATOR_FOR_TESTS([$1])
])

AC_DEFUN([gl_FNMATCH_H_DEFAULTS],
[
  GNULIB_FNMATCH=0;          AC_SUBST([GNULIB_FNMATCH])
  dnl Assume POSIX behavior unless another module says otherwise.
  HAVE_FNMATCH=1;            AC_SUBST([HAVE_FNMATCH])
  REPLACE_FNMATCH=0;         AC_SUBST([REPLACE_FNMATCH])
])
