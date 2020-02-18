/* glob.h -- Find a path matching a pattern.

   Copyright (C) 2005-2007, 2009-2020 Free Software Foundation, Inc.

   Written by Derek Price <derek@ximbiot.com> & Paul Eggert <eggert@CS.UCLA.EDU>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <https://www.gnu.org/licenses/>.  */

#ifndef _@GUARD_PREFIX@_GLOB_H

#if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
#endif
@PRAGMA_COLUMNS@

/* The include_next requires a split double-inclusion guard.  */
#if @HAVE_GLOB_H@ && !@REPLACE_GLOB@
# @INCLUDE_NEXT@ @NEXT_GLOB_H@
#endif

#ifndef _@GUARD_PREFIX@_GLOB_H
#define _@GUARD_PREFIX@_GLOB_H

/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */

/* The definition of _GL_ARG_NONNULL is copied here.  */

/* The definition of _GL_WARN_ON_USE is copied here.  */

/* GCC 2.95 and later have "__restrict"; C99 compilers have
   "restrict", and "configure" may have defined "restrict".
   Other compilers use __restrict, __restrict__, and _Restrict, and
   'configure' might #define 'restrict' to those words, so pick a
   different name.  */
#ifndef _Restrict_
# if 199901L <= __STDC_VERSION__
#  define _Restrict_ restrict
# elif 2 < __GNUC__ || (2 == __GNUC__ && 95 <= __GNUC_MINOR__)
#  define _Restrict_ __restrict
# else
#  define _Restrict_
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
typedef int (*_gl_glob_errfunc_fn) (const char *, int);
#ifdef __cplusplus
}
#endif


#if !@HAVE_GLOB_H@ || @REPLACE_GLOB@


/* Preparations for including the standard GNU C Library header.  */

# include <libc-config.h>

# include <stddef.h>

/* On some systems, such as AIX 5.1, <sys/stat.h> does a "#define stat stat64".
   Make sure this definition is seen before glob-libc.h defines types that
   rely on 'struct stat'.  */
# include <sys/stat.h>

# ifndef __USE_GNU
#  define __USE_GNU    1
# endif

# if @REPLACE_GLOB@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define glob rpl_glob
#   define globfree rpl_globfree
#  endif
# endif
# if @REPLACE_GLOB_PATTERN_P@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define glob_pattern_p rpl_glob_pattern_p
#  endif
# endif
# define __glob_pattern_p glob_pattern_p

# define __GLOB_GNULIB 1


/* Now the standard GNU C Library header should work.  */
# include "glob-libc.h"


#endif


#if @GNULIB_GLOB@
# if @REPLACE_GLOB@
_GL_FUNCDECL_RPL (glob, int, (const char *_Restrict_ __pattern, int __flags,
                              _gl_glob_errfunc_fn __errfunc,
                              glob_t *_Restrict_ __pglob)
                              _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (glob, int, (const char *_Restrict_ __pattern, int __flags,
                              _gl_glob_errfunc_fn __errfunc,
                              glob_t *_Restrict_ __pglob));
# else
#  if !@HAVE_GLOB@
_GL_FUNCDECL_SYS (glob, int, (const char *_Restrict_ __pattern, int __flags,
                              _gl_glob_errfunc_fn __errfunc,
                              glob_t *_Restrict_ __pglob)
                              _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (glob, int, (const char *_Restrict_ __pattern, int __flags,
                              _gl_glob_errfunc_fn __errfunc,
                              glob_t *_Restrict_ __pglob));
# endif
_GL_CXXALIASWARN (glob);
#elif defined GNULIB_POSIXCHECK
# undef glob
# if HAVE_RAW_DECL_GLOB
_GL_WARN_ON_USE (glob,
                 "glob is unportable - "
                 "use gnulib module glob for portability");
# endif
#endif

#if @GNULIB_GLOB@
# if @REPLACE_GLOB@
_GL_FUNCDECL_RPL (globfree, void, (glob_t *__pglob) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (globfree, void, (glob_t *__pglob));
# else
#  if !@HAVE_GLOB@
_GL_FUNCDECL_SYS (globfree, void, (glob_t *__pglob) _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (globfree, void, (glob_t *__pglob));
# endif
_GL_CXXALIASWARN (globfree);
#elif defined GNULIB_POSIXCHECK
# undef globfree
# if HAVE_RAW_DECL_GLOBFREE
_GL_WARN_ON_USE (globfree,
                 "globfree is unportable - "
                 "use gnulib module glob for portability");
# endif
#endif

#if @GNULIB_GLOB@
# if @REPLACE_GLOB_PATTERN_P@
_GL_FUNCDECL_RPL (glob_pattern_p, int, (const char *__pattern, int __quote)
                                       _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (glob_pattern_p, int, (const char *__pattern, int __quote));
# else
#  if !@HAVE_GLOB_PATTERN_P@
_GL_FUNCDECL_SYS (glob_pattern_p, int, (const char *__pattern, int __quote)
                                       _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (glob_pattern_p, int, (const char *__pattern, int __quote));
# endif
_GL_CXXALIASWARN (glob_pattern_p);
#elif defined GNULIB_POSIXCHECK
# undef glob_pattern_p
# if HAVE_RAW_DECL_GLOB_PATTERN_P
_GL_WARN_ON_USE (glob_pattern_p,
                 "glob_pattern_p is unportable - "
                 "use gnulib module glob for portability");
# endif
#endif


#endif /* _@GUARD_PREFIX@_GLOB_H */
#endif /* _@GUARD_PREFIX@_GLOB_H */
