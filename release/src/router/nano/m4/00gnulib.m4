# 00gnulib.m4 serial 7
dnl Copyright (C) 2009-2020 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This file must be named something that sorts before all other
dnl gnulib-provided .m4 files.  The first part is needed until such time
dnl as we can assume Autoconf 2.64, with its improved AC_DEFUN_ONCE and
dnl m4_divert semantics.  The second part is needed until the clang fix
dnl has been included in Autoconf.

# Until autoconf 2.63, handling of the diversion stack required m4_init
# to be called first; but this does not happen with aclocal.  Wrapping
# the entire execution in another layer of the diversion stack fixes this.
# Worse, prior to autoconf 2.62, m4_wrap depended on the underlying m4
# for whether it was FIFO or LIFO; in order to properly balance with
# m4_init, we need to undo our push just before anything wrapped within
# the m4_init body.  The way to ensure this is to wrap both sides of
# m4_init with a one-shot macro that does the pop at the right time.
m4_ifndef([_m4_divert_diversion],
[m4_divert_push([KILL])
m4_define([gl_divert_fixup], [m4_divert_pop()m4_define([$0])])
m4_define([m4_init],
  [gl_divert_fixup()]m4_defn([m4_init])[gl_divert_fixup()])])


# AC_DEFUN_ONCE([NAME], VALUE)
# ----------------------------
# Define NAME to expand to VALUE on the first use (whether by direct
# expansion, or by AC_REQUIRE), and to nothing on all subsequent uses.
# Avoid bugs in AC_REQUIRE in Autoconf 2.63 and earlier.  This
# definition is slower than the version in Autoconf 2.64, because it
# can only use interfaces that existed since 2.59; but it achieves the
# same effect.  Quoting is necessary to avoid confusing Automake.
m4_version_prereq([2.63.263], [],
[m4_define([AC][_DEFUN_ONCE],
  [AC][_DEFUN([$1],
    [AC_REQUIRE([_gl_DEFUN_ONCE([$1])],
      [m4_indir([_gl_DEFUN_ONCE([$1])])])])]dnl
[AC][_DEFUN([_gl_DEFUN_ONCE([$1])], [$2])])])

# The following definitions arrange to use a compiler option
# -Werror=implicit-function-declaration in AC_CHECK_DECL, when the
# compiler is clang.  Without it, clang implicitly declares "known"
# library functions in C mode, but not in C++ mode, which would cause
# Gnulib to omit a declaration and thus later produce an error in C++
# mode.  As of clang 9.0, these "known" functions are identified through
# LIBBUILTIN invocations in the LLVM source file
# llvm/tools/clang/include/clang/Basic/Builtins.def.
# It's not possible to AC_REQUIRE the extra tests from AC_CHECK_DECL,
# because AC_CHECK_DECL, like other Autoconf built-ins, is not supposed
# to AC_REQUIRE anything: some configure.ac files have their first
# AC_CHECK_DECL executed conditionally.  Therefore append the extra tests
# to AC_PROG_CC.
AC_DEFUN([gl_COMPILER_CLANG],
[
dnl AC_REQUIRE([AC_PROG_CC])
  AC_CACHE_CHECK([whether the compiler is clang],
    [gl_cv_compiler_clang],
    [dnl Use _AC_COMPILE_IFELSE instead of AC_EGREP_CPP, to avoid error
     dnl "circular dependency of AC_LANG_COMPILER(C)" if AC_PROG_CC has
     dnl not yet been invoked.
     _AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([[
           #ifdef __clang__
           barfbarf
           #endif
           ]],[[]])
        ],
        [gl_cv_compiler_clang=no],
        [gl_cv_compiler_clang=yes])
    ])
])
AC_DEFUN([gl_COMPILER_PREPARE_CHECK_DECL],
[
dnl AC_REQUIRE([AC_PROG_CC])
dnl AC_REQUIRE([gl_COMPILER_CLANG])
  AC_CACHE_CHECK([for compiler option needed when checking for declarations],
    [gl_cv_compiler_check_decl_option],
    [if test $gl_cv_compiler_clang = yes; then
       dnl Test whether the compiler supports the option
       dnl '-Werror=implicit-function-declaration'.
       save_ac_compile="$ac_compile"
       ac_compile="$ac_compile -Werror=implicit-function-declaration"
       dnl Use _AC_COMPILE_IFELSE instead of AC_COMPILE_IFELSE, to avoid a
       dnl warning "AC_COMPILE_IFELSE was called before AC_USE_SYSTEM_EXTENSIONS".
       _AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],[[]])],
         [gl_cv_compiler_check_decl_option='-Werror=implicit-function-declaration'],
         [gl_cv_compiler_check_decl_option=none])
       ac_compile="$save_ac_compile"
     else
       gl_cv_compiler_check_decl_option=none
     fi
    ])
  if test "x$gl_cv_compiler_check_decl_option" != xnone; then
    ac_compile_for_check_decl="$ac_compile $gl_cv_compiler_check_decl_option"
  else
    ac_compile_for_check_decl="$ac_compile"
  fi
])
dnl Redefine _AC_CHECK_DECL_BODY so that it references ac_compile_for_check_decl
dnl instead of ac_compile.  If, for whatever reason, the override of AC_PROG_CC
dnl in zzgnulib.m4 is inactive, use the original ac_compile.
m4_define([_AC_CHECK_DECL_BODY],
[  ac_save_ac_compile="$ac_compile"
  if test -n "$ac_compile_for_check_decl"; then
    ac_compile="$ac_compile_for_check_decl"
  fi]
m4_defn([_AC_CHECK_DECL_BODY])[  ac_compile="$ac_save_ac_compile"
])

# gl_00GNULIB
# -----------
# Witness macro that this file has been included.  Needed to force
# Automake to include this file prior to all other gnulib .m4 files.
AC_DEFUN([gl_00GNULIB])
