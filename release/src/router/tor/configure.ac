dnl Copyright (c) 2001-2004, Roger Dingledine
dnl Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson
dnl Copyright (c) 2007-2019, The Tor Project, Inc.
dnl See LICENSE for licensing information

AC_PREREQ([2.63])
AC_INIT([tor],[0.3.5.8])
AC_CONFIG_SRCDIR([src/app/main/tor_main.c])
AC_CONFIG_MACRO_DIR([m4])

# "foreign" means we don't follow GNU package layout standards
# "1.11" means we require automake version 1.11 or newer
# "subdir-objects" means put .o files in the same directory as the .c files
AM_INIT_AUTOMAKE([foreign 1.11 subdir-objects -Wall -Werror])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_HEADERS([orconfig.h])

AC_USE_SYSTEM_EXTENSIONS
AC_CANONICAL_HOST

PKG_PROG_PKG_CONFIG

AC_ARG_ENABLE(openbsd-malloc,
   AS_HELP_STRING(--enable-openbsd-malloc, [use malloc code from OpenBSD.  Linux only. Deprecated: see --with-malloc]))
AC_ARG_ENABLE(static-openssl,
   AS_HELP_STRING(--enable-static-openssl, [link against a static openssl library. Requires --with-openssl-dir]))
AC_ARG_ENABLE(static-libevent,
   AS_HELP_STRING(--enable-static-libevent, [link against a static libevent library. Requires --with-libevent-dir]))
AC_ARG_ENABLE(static-zlib,
   AS_HELP_STRING(--enable-static-zlib, [link against a static zlib library. Requires --with-zlib-dir]))
AC_ARG_ENABLE(static-tor,
   AS_HELP_STRING(--enable-static-tor, [create an entirely static Tor binary. Requires --with-openssl-dir and --with-libevent-dir and --with-zlib-dir]))
AC_ARG_ENABLE(unittests,
   AS_HELP_STRING(--disable-unittests, [don't build unit tests for Tor. Risky!]))
AC_ARG_ENABLE(coverage,
   AS_HELP_STRING(--enable-coverage, [enable coverage support in the unit-test build]))
AC_ARG_ENABLE(asserts-in-tests,
   AS_HELP_STRING(--disable-asserts-in-tests, [disable tor_assert() calls in the unit tests, for branch coverage]))
AC_ARG_ENABLE(system-torrc,
   AS_HELP_STRING(--disable-system-torrc, [don't look for a system-wide torrc file]))
AC_ARG_ENABLE(libfuzzer,
   AS_HELP_STRING(--enable-libfuzzer, [build extra fuzzers based on 'libfuzzer']))
AC_ARG_ENABLE(oss-fuzz,
   AS_HELP_STRING(--enable-oss-fuzz, [build extra fuzzers based on 'oss-fuzz' environment]))
AC_ARG_ENABLE(memory-sentinels,
   AS_HELP_STRING(--disable-memory-sentinels, [disable code that tries to prevent some kinds of memory access bugs. For fuzzing only.]))
AC_ARG_ENABLE(rust,
   AS_HELP_STRING(--enable-rust, [enable rust integration]))
AC_ARG_ENABLE(cargo-online-mode,
   AS_HELP_STRING(--enable-cargo-online-mode, [Allow cargo to make network requests to fetch crates. For builds with rust only.]))
AC_ARG_ENABLE(restart-debugging,
   AS_HELP_STRING(--enable-restart-debugging, [Build Tor with support for debugging in-process restart. Developers only.]))
AC_ARG_ENABLE(zstd-advanced-apis,
   AS_HELP_STRING(--disable-zstd-advanced-apis, [Build without support for zstd's "static-only" APIs.]))
AC_ARG_ENABLE(nss,
   AS_HELP_STRING(--enable-nss, [Use Mozilla's NSS TLS library. (EXPERIMENTAL)]))
AC_ARG_ENABLE(pic,
   AS_HELP_STRING(--enable-pic, [Build Tor's binaries as position-independent code, suitable to link as a library.]))

if test "x$enable_coverage" != "xyes" -a "x$enable_asserts_in_tests" = "xno" ; then
    AC_MSG_ERROR([Can't disable assertions outside of coverage build])
fi

AM_CONDITIONAL(UNITTESTS_ENABLED, test "x$enable_unittests" != "xno")
AM_CONDITIONAL(COVERAGE_ENABLED, test "x$enable_coverage" = "xyes")
AM_CONDITIONAL(DISABLE_ASSERTS_IN_UNIT_TESTS, test "x$enable_asserts_in_tests" = "xno")
AM_CONDITIONAL(LIBFUZZER_ENABLED, test "x$enable_libfuzzer" = "xyes")
AM_CONDITIONAL(OSS_FUZZ_ENABLED, test "x$enable_oss_fuzz" = "xyes")
AM_CONDITIONAL(USE_RUST, test "x$enable_rust" = "xyes")
AM_CONDITIONAL(USE_NSS, test "x$enable_nss" = "xyes")
AM_CONDITIONAL(USE_OPENSSL, test "x$enable_nss" != "xyes")

if test "x$enable_nss" = "xyes"; then
  AC_DEFINE(ENABLE_NSS, 1,
  	    [Defined if we're building with NSS.])
else
  AC_DEFINE(ENABLE_OPENSSL, 1,
            [Defined if we're building with OpenSSL or LibreSSL])
fi

if test "$enable_static_tor" = "yes"; then
  enable_static_libevent="yes";
  enable_static_openssl="yes";
  enable_static_zlib="yes";
  CFLAGS="$CFLAGS -static"
fi

if test "$enable_system_torrc" = "no"; then
  AC_DEFINE(DISABLE_SYSTEM_TORRC, 1,
            [Defined if we're not going to look for a torrc in SYSCONF])
fi

if test "$enable_memory_sentinels" = "no"; then
  AC_DEFINE(DISABLE_MEMORY_SENTINELS, 1,
           [Defined if we're turning off memory safety code to look for bugs])
fi

AC_ARG_ENABLE(asciidoc,
     AS_HELP_STRING(--disable-asciidoc, [don't use asciidoc (disables building of manpages)]),
     [case "${enableval}" in
        "yes") asciidoc=true ;;
        "no")  asciidoc=false ;;
        *) AC_MSG_ERROR(bad value for --disable-asciidoc) ;;
      esac], [asciidoc=true])

# systemd notify support
AC_ARG_ENABLE(systemd,
      AS_HELP_STRING(--enable-systemd, [enable systemd notification support]),
      [case "${enableval}" in
        "yes") systemd=true ;;
        "no")  systemd=false ;;
        * ) AC_MSG_ERROR(bad value for --enable-systemd) ;;
      esac], [systemd=auto])

if test "$enable_restart_debugging" = "yes"; then
  AC_DEFINE(ENABLE_RESTART_DEBUGGING, 1,
            [Defined if we're building with support for in-process restart debugging.])
fi

if test "$enable_zstd_advanced_apis" != "no"; then
   AC_DEFINE(ENABLE_ZSTD_ADVANCED_APIS, 1,
             [Defined if we're going to try to use zstd's "static-only" APIs.])
fi

# systemd support
if test "x$enable_systemd" = "xno"; then
    have_systemd=no;
else
    PKG_CHECK_MODULES(SYSTEMD,
        [libsystemd-daemon],
        have_systemd=yes,
        have_systemd=no)
    if test "x$have_systemd" = "xno"; then
        AC_MSG_NOTICE([Okay, checking for systemd a different way...])
        PKG_CHECK_MODULES(SYSTEMD,
            [libsystemd],
            have_systemd=yes,
            have_systemd=no)
    fi
fi

if test "x$have_systemd" = "xyes"; then
    AC_DEFINE(HAVE_SYSTEMD,1,[Have systemd])
    TOR_SYSTEMD_CFLAGS="${SYSTEMD_CFLAGS}"
    TOR_SYSTEMD_LIBS="${SYSTEMD_LIBS}"
    PKG_CHECK_MODULES(LIBSYSTEMD209, [libsystemd >= 209],
         [AC_DEFINE(HAVE_SYSTEMD_209,1,[Have systemd v209 or more])], [])
fi
AC_SUBST(TOR_SYSTEMD_CFLAGS)
AC_SUBST(TOR_SYSTEMD_LIBS)

if test "x$enable_systemd" = "xyes" -a "x$have_systemd" != "xyes" ; then
    AC_MSG_ERROR([Explicitly requested systemd support, but systemd not found])
fi

case "$host" in
   *-*-solaris* )
     AC_DEFINE(_REENTRANT, 1, [Define on some platforms to activate x_r() functions in time.h])
     ;;
esac

AC_ARG_ENABLE(gcc-warnings,
     AS_HELP_STRING(--enable-gcc-warnings, [deprecated alias for enable-fatal-warnings]))
AC_ARG_ENABLE(fatal-warnings,
     AS_HELP_STRING(--enable-fatal-warnings, [tell the compiler to treat all warnings as errors.]))
AC_ARG_ENABLE(gcc-warnings-advisory,
     AS_HELP_STRING(--disable-gcc-warnings-advisory, [disable the regular verbose warnings]))

dnl Others suggest '/gs /safeseh /nxcompat /dynamicbase' for non-gcc on Windows
AC_ARG_ENABLE(gcc-hardening,
    AS_HELP_STRING(--disable-gcc-hardening, [disable compiler security checks]))

dnl Deprecated --enable-expensive-hardening but keep it for now for backward compat.
AC_ARG_ENABLE(expensive-hardening,
    AS_HELP_STRING(--enable-expensive-hardening, [enable more fragile and expensive compiler hardening; makes Tor slower]))
AC_ARG_ENABLE(fragile-hardening,
    AS_HELP_STRING(--enable-fragile-hardening, [enable more fragile and expensive compiler hardening; makes Tor slower]))
if test "x$enable_expensive_hardening" = "xyes" || test "x$enable_fragile_hardening" = "xyes"; then
  fragile_hardening="yes"
  AC_DEFINE(DEBUG_SMARTLIST, 1, [Enable smartlist debugging])
fi

dnl Linker hardening options
dnl Currently these options are ELF specific - you can't use this with MacOSX
AC_ARG_ENABLE(linker-hardening,
    AS_HELP_STRING(--disable-linker-hardening, [disable linker security fixups]))

AC_ARG_ENABLE(local-appdata,
   AS_HELP_STRING(--enable-local-appdata, [default to host local application data paths on Windows]))
if test "$enable_local_appdata" = "yes"; then
  AC_DEFINE(ENABLE_LOCAL_APPDATA, 1,
            [Defined if we default to host local appdata paths on Windows])
fi

AC_ARG_ENABLE(tool-name-check,
     AS_HELP_STRING(--disable-tool-name-check, [check for sanely named toolchain when cross-compiling]))

AC_ARG_ENABLE(seccomp,
     AS_HELP_STRING(--disable-seccomp, [do not attempt to use libseccomp]))

AC_ARG_ENABLE(libscrypt,
     AS_HELP_STRING(--disable-libscrypt, [do not attempt to use libscrypt]))

dnl Enable event tracing which are transformed to debug log statement.
AC_ARG_ENABLE(event-tracing-debug,
     AS_HELP_STRING(--enable-event-tracing-debug, [build with event tracing to debug log]))
AM_CONDITIONAL([USE_EVENT_TRACING_DEBUG], [test "x$enable_event_tracing_debug" = "xyes"])

if test x$enable_event_tracing_debug = xyes; then
  AC_DEFINE([USE_EVENT_TRACING_DEBUG], [1], [Tracing framework to log debug])
  AC_DEFINE([TOR_EVENT_TRACING_ENABLED], [1], [Compile the event tracing instrumentation])
fi

dnl Enable Android only features.
AC_ARG_ENABLE(android,
     AS_HELP_STRING(--enable-android, [build with Android features enabled]))
AM_CONDITIONAL([USE_ANDROID], [test "x$enable_android" = "xyes"])

if test "x$enable_android" = "xyes"; then
  AC_DEFINE([USE_ANDROID], [1], [Compile with Android specific features enabled])

  dnl Check if the Android log library is available.
  AC_CHECK_HEADERS([android/log.h])
  AC_SEARCH_LIBS(__android_log_write, [log])

fi

dnl ---
dnl Tor modules options. These options are namespaced with --disable-module-XXX
dnl ---

dnl All our modules.
m4_define(MODULES, dirauth)

dnl Directory Authority module.
AC_ARG_ENABLE([module-dirauth],
              AS_HELP_STRING([--disable-module-dirauth],
                             [Do not build tor with the dirauth module]),
              [], dnl Action if-given
              AC_DEFINE([HAVE_MODULE_DIRAUTH], [1],
                        [Compile with Directory Authority feature support]))
AM_CONDITIONAL(BUILD_MODULE_DIRAUTH, [test "x$enable_module_dirauth" != "xno"])

dnl Helper variables.
TOR_MODULES_ALL_ENABLED=
AC_DEFUN([ADD_MODULE], [
    MODULE=m4_toupper($1)
    TOR_MODULES_ALL_ENABLED="${TOR_MODULES_ALL_ENABLED} -DHAVE_MODULE_${MODULE}=1"
])
m4_foreach_w([module], MODULES, [ADD_MODULE([module])])
AC_SUBST(TOR_MODULES_ALL_ENABLED)

dnl check for the correct "ar" when cross-compiling.
dnl   (AM_PROG_AR was new in automake 1.11.2, which we do not yet require,
dnl    so kludge up a replacement for the case where it isn't there yet.)
m4_ifdef([AM_PROG_AR],
         [AM_PROG_AR],
         [AN_MAKEVAR([AR], [AC_PROG_AR])
          AN_PROGRAM([ar], [AC_PROG_AR])
          AC_DEFUN([AC_PROG_AR], [AC_CHECK_TOOL([AR], [ar], [:])])
          AC_PROG_AR])

dnl Check whether the above macro has settled for a simply named tool even
dnl though we're cross compiling. We must do this before running AC_PROG_CC,
dnl because that will find any cc on the system, not only the cross-compiler,
dnl and then verify that a binary built with this compiler runs on the
dnl build system. It will then come to the false conclusion that we're not
dnl cross-compiling.
if test "x$enable_tool_name_check" != "xno"; then
    if test "x$ac_tool_warned" = "xyes"; then
        AC_MSG_ERROR([We are cross compiling but could not find a properly named toolchain. Do you have your cross-compiling toolchain in PATH? (You can --disable-tool-name-check to ignore this.)])
	elif test "x$ac_ct_AR" != "x" -a "x$cross_compiling" = "xmaybe"; then
		AC_MSG_ERROR([We think we are cross compiling but could not find a properly named toolchain. Do you have your cross-compiling toolchain in PATH? (You can --disable-tool-name-check to ignore this.)])
	fi
fi

AC_PROG_CC
AC_PROG_CPP
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_SED

AC_ARG_VAR([PERL], [path to Perl binary])
AC_CHECK_PROGS([PERL], [perl])
AM_CONDITIONAL(USE_PERL, [test "x$ac_cv_prog_PERL" != "x"])

dnl check for asciidoc and a2x
AC_PATH_PROG([ASCIIDOC], [asciidoc], none)
AC_PATH_PROGS([A2X], [a2x a2x.py], none)

AM_CONDITIONAL(USE_ASCIIDOC, test "x$asciidoc" = "xtrue")

AM_PROG_CC_C_O
AC_PROG_CC_C99

AC_ARG_VAR([PYTHON], [path to Python binary])
AC_CHECK_PROGS(PYTHON, [ \
	python3 \
	python3.8 python3.7 python3.6 python3.5 python3.4 \
	python \
	python2 python2.7])
if test "x$PYTHON" = "x"; then
  AC_MSG_WARN([Python unavailable; some tests will not be run.])
fi
AM_CONDITIONAL(USEPYTHON, [test "x$PYTHON" != "x"])

dnl List all external rust crates we depend on here. Include the version
rust_crates=" \
    digest-0.7.2 \
    libc-0.2.39 \
"
AC_SUBST(rust_crates)

ifdef([AC_C_FLEXIBLE_ARRAY_MEMBER], [
AC_C_FLEXIBLE_ARRAY_MEMBER
], [
 dnl Maybe we've got an old autoconf...
 AC_CACHE_CHECK([for flexible array members],
     tor_cv_c_flexarray,
     [AC_COMPILE_IFELSE(
       AC_LANG_PROGRAM([
 struct abc { int a; char b[]; };
], [
 struct abc *def = malloc(sizeof(struct abc)+sizeof(char));
 def->b[0] = 33;
]),
  [tor_cv_c_flexarray=yes],
  [tor_cv_c_flexarray=no])])
 if test "$tor_cv_flexarray" = "yes"; then
   AC_DEFINE([FLEXIBLE_ARRAY_MEMBER], [], [Define to nothing if C supports flexible array members, and to 1 if it does not.])
 else
   AC_DEFINE([FLEXIBLE_ARRAY_MEMBER], [1], [Define to nothing if C supports flexible array members, and to 1 if it does not.])
 fi
])

AC_CACHE_CHECK([for working C99 mid-block declaration syntax],
      tor_cv_c_c99_decl,
      [AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM([], [int x; x = 3; int y; y = 4 + x;])],
	 [tor_cv_c_c99_decl=yes],
	 [tor_cv_c_c99_decl=no] )])
if test "$tor_cv_c_c99_decl" != "yes"; then
  AC_MSG_ERROR([Your compiler doesn't support c99 mid-block declarations. This is required as of Tor 0.2.6.x])
fi

AC_CACHE_CHECK([for working C99 designated initializers],
      tor_cv_c_c99_designated_init,
      [AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM([struct s { int a; int b; };],
  	       [[ struct s ss = { .b = 5, .a = 6 }; ]])],
	 [tor_cv_c_c99_designated_init=yes],
	 [tor_cv_c_c99_designated_init=no] )])

if test "$tor_cv_c_c99_designated_init" != "yes"; then
  AC_MSG_ERROR([Your compiler doesn't support c99 designated initializers. This is required as of Tor 0.2.6.x])
fi

TORUSER=_tor
AC_ARG_WITH(tor-user,
        AS_HELP_STRING(--with-tor-user=NAME, [specify username for tor daemon]),
        [
           TORUSER=$withval
        ]
)
AC_SUBST(TORUSER)

TORGROUP=_tor
AC_ARG_WITH(tor-group,
        AS_HELP_STRING(--with-tor-group=NAME, [specify group name for tor daemon]),
        [
           TORGROUP=$withval
        ]
)
AC_SUBST(TORGROUP)


dnl If _WIN32 is defined and non-zero, we are building for win32
AC_MSG_CHECKING([for win32])
AC_RUN_IFELSE([AC_LANG_SOURCE([
int main(int c, char **v) {
#ifdef _WIN32
#if _WIN32
  return 0;
#else
  return 1;
#endif
#else
  return 2;
#endif
}])],
bwin32=true; AC_MSG_RESULT([yes]),
bwin32=false; AC_MSG_RESULT([no]),
bwin32=cross; AC_MSG_RESULT([cross])
)

if test "$bwin32" = "cross"; then
AC_MSG_CHECKING([for win32 (cross)])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#ifdef _WIN32
int main(int c, char **v) {return 0;}
#else
#error
int main(int c, char **v) {return x(y);}
#endif
])],
bwin32=true; AC_MSG_RESULT([yes]),
bwin32=false; AC_MSG_RESULT([no]))
fi

AH_BOTTOM([
#ifdef _WIN32
/* Defined to access windows functions and definitions for >=WinXP */
# ifndef WINVER
#  define WINVER 0x0501
# endif

/* Defined to access _other_ windows functions and definitions for >=WinXP */
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0501
# endif

/* Defined to avoid including some windows headers as part of Windows.h */
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN 1
# endif
#endif
])

AM_CONDITIONAL(WIN32, test "x$bwin32" = "xtrue")
AM_CONDITIONAL(BUILD_NT_SERVICES, test "x$bwin32" = "xtrue")
AM_CONDITIONAL(BUILD_LIBTORRUNNER, test "x$bwin32" != "xtrue")

dnl Enable C99 when compiling with MIPSpro
AC_MSG_CHECKING([for MIPSpro compiler])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(, [
#if (defined(__sgi) && defined(_COMPILER_VERSION))
#error
  return x(y);
#endif
])],
bmipspro=false; AC_MSG_RESULT(no),
bmipspro=true; AC_MSG_RESULT(yes))

if test "$bmipspro" = "true"; then
  CFLAGS="$CFLAGS -c99"
fi

AC_C_BIGENDIAN

AC_ARG_VAR([TOR_RUST_TARGET], [Rust target, must be specified when cross-compiling (HOST != BUILD). example: i686-pc-windows-gnu])

if test "x$enable_rust" = "xyes"; then
  AC_ARG_VAR([RUSTC], [path to the rustc binary])
  AC_CHECK_PROG([RUSTC], [rustc], [rustc],[no])
  if test "x$RUSTC" = "xno"; then
    AC_MSG_ERROR([rustc unavailable but rust integration requested.])
  fi

  AC_ARG_VAR([CARGO], [path to the cargo binary])
  AC_CHECK_PROG([CARGO], [cargo], [cargo],[no])
  if test "x$CARGO" = "xno"; then
    AC_MSG_ERROR([cargo unavailable but rust integration requested.])
  fi

  AC_DEFINE([HAVE_RUST], 1, [have Rust])
  if test "x$enable_fatal_warnings" = "xyes"; then
    RUST_WARN=
  else
    RUST_WARN=#
  fi
  if test "x$enable_cargo_online_mode" = "xyes"; then
    CARGO_ONLINE=
    RUST_DL=#
  else
    CARGO_ONLINE=--frozen
    RUST_DL=

    dnl When we're not allowed to touch the network, we need crate dependencies
    dnl locally available.
    AC_MSG_CHECKING([rust crate dependencies])
    AC_ARG_VAR([TOR_RUST_DEPENDENCIES], [path to directory with local crate mirror])
    if test "x$TOR_RUST_DEPENDENCIES" = "x"; then
      TOR_RUST_DEPENDENCIES="${srcdir}/src/ext/rust/crates"
    fi
    dnl Check whether the path exists before we try to cd into it.
    if test ! -d "$TOR_RUST_DEPENDENCIES"; then
      AC_MSG_ERROR([Rust dependency directory $TOR_RUST_DEPENDENCIES does not exist. Specify a dependency directory using the TOR_RUST_DEPENDENCIES variable or allow cargo to fetch crates using --enable-cargo-online-mode.])
      ERRORED=1
    fi
    dnl Make the path absolute, since we'll be using it from within a
    dnl subdirectory.
    TOR_RUST_DEPENDENCIES=$(cd "$TOR_RUST_DEPENDENCIES" ; pwd)

    for dep in $rust_crates; do
      if test ! -d "$TOR_RUST_DEPENDENCIES"/"$dep"; then
        AC_MSG_ERROR([Failure to find rust dependency $TOR_RUST_DEPENDENCIES/$dep. Specify a dependency directory using the TOR_RUST_DEPENDENCIES variable or allow cargo to fetch crates using --enable-cargo-online-mode.])
        ERRORED=1
      fi
    done
    if test "x$ERRORED" = "x"; then
      AC_MSG_RESULT([yes])
    fi
  fi

  dnl This is a workaround for #46797
  dnl (a.k.a https://github.com/rust-lang/rust/issues/46797 ).  Once the
  dnl upstream bug is fixed, we can remove this workaround.
  case "$host_os" in
      darwin*)
        TOR_RUST_EXTRA_LIBS="-lresolv"
	;;
  esac

  dnl For now both MSVC and MinGW rust libraries will output static libs with
  dnl the MSVC naming convention.
  if test "$bwin32" = "true"; then
    tor_rust_static_name=tor_rust.lib
  else
    tor_rust_static_name=libtor_rust.a
  fi

  AC_CANONICAL_BUILD

  if test -n "$TOR_RUST_TARGET"; then
    if test "$host" = "$build"; then
      AC_MSG_ERROR([HOST = BUILD is invalid if TOR_RUST_TARGET is specified, see configure --help for more information.])
    fi
    RUST_TARGET_PROP="target = '$TOR_RUST_TARGET'"
    TOR_RUST_LIB_PATH="src/rust/target/$TOR_RUST_TARGET/release/$tor_rust_static_name"
  else
    if test "$host" != "$build"; then
      AC_MSG_ERROR([TOR_RUST_TARGET must be specified when cross-compiling with Rust enabled.])
    fi
    RUST_TARGET_PROP=
    TOR_RUST_LIB_PATH="src/rust/target/release/$tor_rust_static_name"
  fi

  AC_SUBST(RUST_TARGET_PROP)
  AC_SUBST(TOR_RUST_LIB_PATH)
  AC_SUBST(CARGO_ONLINE)
  AC_SUBST(RUST_WARN)
  AC_SUBST(RUST_DL)

  dnl Let's check the rustc version, too
  AC_MSG_CHECKING([rust version])
  RUSTC_VERSION=`$RUSTC --version`
  RUSTC_VERSION_MAJOR=`$RUSTC --version | cut -d ' ' -f 2 | cut -d '.' -f 1`
  RUSTC_VERSION_MINOR=`$RUSTC --version | cut -d ' ' -f 2 | cut -d '.' -f 2`
  if test "x$RUSTC_VERSION_MAJOR" = "x" -o "x$RUSTC_VERSION_MINOR" = "x"; then
    AC_MSG_ERROR([rustc version couldn't be identified])
  fi
  if test "$RUSTC_VERSION_MAJOR" -lt 2 -a "$RUSTC_VERSION_MINOR" -lt 14; then
    AC_MSG_ERROR([rustc must be at least version 1.14])
  fi
  AC_MSG_RESULT([$RUSTC_VERSION])
fi

AC_SUBST(TOR_RUST_EXTRA_LIBS)

AC_SEARCH_LIBS(socket, [socket network])
AC_SEARCH_LIBS(gethostbyname, [nsl])
AC_SEARCH_LIBS(dlopen, [dl])
AC_SEARCH_LIBS(inet_aton, [resolv])
AC_SEARCH_LIBS(backtrace, [execinfo])
saved_LIBS="$LIBS"
AC_SEARCH_LIBS([clock_gettime], [rt])
if test "$LIBS" != "$saved_LIBS"; then
   # Looks like we need -lrt for clock_gettime().
   have_rt=yes
fi

if test "$bwin32" = "false"; then
  AC_SEARCH_LIBS(pthread_create, [pthread])
  AC_SEARCH_LIBS(pthread_detach, [pthread])
fi

AM_CONDITIONAL(THREADS_WIN32, test "$bwin32" = "true")
AM_CONDITIONAL(THREADS_PTHREADS, test "$bwin32" = "false")

AC_CHECK_FUNCS(
	_NSGetEnviron \
	RtlSecureZeroMemory \
	SecureZeroMemory \
	accept4 \
	backtrace \
	backtrace_symbols_fd \
	eventfd \
	explicit_bzero \
	timingsafe_memcmp \
	flock \
	ftime \
	get_current_dir_name \
	getaddrinfo \
	getdelim \
	getifaddrs \
	getline \
	getpass \
	getrlimit \
	gettimeofday \
	gmtime_r \
	gnu_get_libc_version \
	htonll \
	inet_aton \
	ioctl \
	issetugid \
	llround \
	localtime_r \
	lround \
	memmem \
	memset_s \
	mmap \
	pipe \
	pipe2 \
	prctl \
	readpassphrase \
	rint \
	sigaction \
	socketpair \
	statvfs \
	strncasecmp \
	strcasecmp \
	strlcat \
	strlcpy \
	strnlen \
	strptime \
	strtok_r \
	strtoull \
	sysconf \
	sysctl \
	truncate \
	uname \
	usleep \
	vasprintf \
	_vscprintf
)

# Apple messed up when they added some functions: they
# forgot to decorate them with appropriate AVAILABLE_MAC_OS_VERSION
# checks.

# We should only probe for these functions if we are sure that we
# are not targeting OS X 10.9 or earlier.
AC_MSG_CHECKING([for a pre-Yosemite OS X build target])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef __APPLE__
#  include <AvailabilityMacros.h>
#  ifndef MAC_OS_X_VERSION_10_10
#    define MAC_OS_X_VERSION_10_10 101000
#  endif
#  if defined(MAC_OS_X_VERSION_MIN_REQUIRED)
#    if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_10
#      error "Running on Mac OS X 10.9 or earlier"
#    endif
#  endif
#endif
]], [[]])],
   [on_macos_pre_10_10=no ; AC_MSG_RESULT([no])],
   [on_macos_pre_10_10=yes; AC_MSG_RESULT([yes])])

if test "$on_macos_pre_10_10" = "no"; then
  AC_CHECK_FUNCS(
        mach_approximate_time \
  )
fi

# We should only probe for these functions if we are sure that we
# are not targeting OSX 10.11 or earlier.
AC_MSG_CHECKING([for a pre-Sierra OSX build target])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef __APPLE__
#  include <AvailabilityMacros.h>
#  ifndef MAC_OS_X_VERSION_10_12
#    define MAC_OS_X_VERSION_10_12 101200
#  endif
#  if defined(MAC_OS_X_VERSION_MIN_REQUIRED)
#    if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12
#      error "Running on Mac OSX 10.11 or earlier"
#    endif
#  endif
#endif
]], [[]])],
   [on_macos_pre_10_12=no ; AC_MSG_RESULT([no])],
   [on_macos_pre_10_12=yes; AC_MSG_RESULT([yes])])

if test "$on_macos_pre_10_12" = "no"; then
  AC_CHECK_FUNCS(
        clock_gettime \
        getentropy \
  )
fi

if test "$bwin32" != "true"; then
  AC_CHECK_HEADERS(pthread.h)
  AC_CHECK_FUNCS(pthread_create)
  AC_CHECK_FUNCS(pthread_condattr_setclock)
fi

if test "$bwin32" = "true"; then
  AC_CHECK_DECLS([SecureZeroMemory, _getwch], , , [
#include <windows.h>
#include <conio.h>
#include <wchar.h>
                 ])
fi

AM_CONDITIONAL(BUILD_READPASSPHRASE_C,
  test "x$ac_cv_func_readpassphrase" = "xno" && test "$bwin32" = "false")

AC_MSG_CHECKING([whether free(NULL) works])
AC_RUN_IFELSE([AC_LANG_PROGRAM([
  #include <stdlib.h>
], [
char *p = NULL;
free(p);
])],
[free_null_ok=true; AC_MSG_RESULT(yes)],
[free_null_ok=false; AC_MSG_RESULT(no)],
[free_null_ok=cross; AC_MSG_RESULT(cross)])

if test "$free_null_ok" = "false"; then
   AC_MSG_ERROR([Your libc implementation doesn't allow free(NULL), as required by C99.])
fi

dnl ------------------------------------------------------
dnl Where do you live, libevent?  And how do we call you?

if test "$bwin32" = "true"; then
  TOR_LIB_WS32=-lws2_32
  TOR_LIB_IPHLPAPI=-liphlpapi
  # Some of the cargo-cults recommend -lwsock32 as well, but I don't
  # think it's actually necessary.
  TOR_LIB_GDI=-lgdi32
  TOR_LIB_USERENV=-luserenv
else
  TOR_LIB_WS32=
  TOR_LIB_GDI=
  TOR_LIB_USERENV=
fi
AC_SUBST(TOR_LIB_WS32)
AC_SUBST(TOR_LIB_GDI)
AC_SUBST(TOR_LIB_IPHLPAPI)
AC_SUBST(TOR_LIB_USERENV)

tor_libevent_pkg_redhat="libevent"
tor_libevent_pkg_debian="libevent-dev"
tor_libevent_devpkg_redhat="libevent-devel"
tor_libevent_devpkg_debian="libevent-dev"

dnl On Gnu/Linux or any place we require it, we'll add librt to the Libevent
dnl linking for static builds.
STATIC_LIBEVENT_FLAGS=""
if test "$enable_static_libevent" = "yes"; then
    if test "$have_rt" = "yes"; then
      STATIC_LIBEVENT_FLAGS=" -lrt "
    fi
fi

TOR_SEARCH_LIBRARY(libevent, $trylibeventdir, [-levent $STATIC_LIBEVENT_FLAGS $TOR_LIB_WS32], [
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <event2/event.h>], [
#ifdef _WIN32
#include <winsock2.h>
#endif
struct event_base;
struct event_base *event_base_new(void);
void event_base_free(struct event_base *);],
    [
#ifdef _WIN32
{WSADATA d; WSAStartup(0x101,&d); }
#endif
event_base_free(event_base_new());
], [--with-libevent-dir], [/opt/libevent])

dnl Determine the incantation needed to link libevent.
save_LIBS="$LIBS"
save_LDFLAGS="$LDFLAGS"
save_CPPFLAGS="$CPPFLAGS"

LIBS="$STATIC_LIBEVENT_FLAGS $TOR_LIB_WS32 $save_LIBS"
LDFLAGS="$TOR_LDFLAGS_libevent $LDFLAGS"
CPPFLAGS="$TOR_CPPFLAGS_libevent $CPPFLAGS"

AC_CHECK_HEADERS(event2/event.h event2/dns.h event2/bufferevent_ssl.h)

if test "$enable_static_libevent" = "yes"; then
   if test "$tor_cv_library_libevent_dir" = "(system)"; then
     AC_MSG_ERROR("You must specify an explicit --with-libevent-dir=x option when using --enable-static-libevent")
   else
     TOR_LIBEVENT_LIBS="$TOR_LIBDIR_libevent/libevent.a $STATIC_LIBEVENT_FLAGS"
   fi
else
     if test "x$ac_cv_header_event2_event_h" = "xyes"; then
       AC_SEARCH_LIBS(event_new, [event event_core], , AC_MSG_ERROR("libevent2 is installed but linking it failed while searching for event_new"))
       AC_SEARCH_LIBS(evdns_base_new, [event event_extra], , AC_MSG_ERROR("libevent2 is installed but linking it failed while searching for evdns_base_new"))

       if test "$ac_cv_search_event_new" != "none required"; then
         TOR_LIBEVENT_LIBS="$ac_cv_search_event_new"
       fi
       if test "$ac_cv_search_evdns_base_new" != "none required"; then
         TOR_LIBEVENT_LIBS="$ac_cv_search_evdns_base_new $TOR_LIBEVENT_LIBS"
       fi
     else
       AC_MSG_ERROR("libevent2 is required but the headers could not be found")
     fi
fi

dnl Now check for particular libevent functions.
AC_CHECK_FUNCS([evutil_secure_rng_set_urandom_device_file \
                evutil_secure_rng_add_bytes \
])

LIBS="$save_LIBS"
LDFLAGS="$save_LDFLAGS"
CPPFLAGS="$save_CPPFLAGS"

dnl Check that libevent is at least at version 2.0.10, the first stable
dnl release of its series
CPPFLAGS="$CPPFLAGS $TOR_CPPFLAGS_libevent"
AC_MSG_CHECKING([whether Libevent is new enough])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <event2/event.h>
#if !defined(LIBEVENT_VERSION_NUMBER) || LIBEVENT_VERSION_NUMBER < 0x02000a00
#error
int x = y(zz);
#else
int x = 1;
#endif
])], [ AC_MSG_RESULT([yes]) ],
   [ AC_MSG_RESULT([no])
     AC_MSG_ERROR([Libevent is not new enough.  We require 2.0.10-stable or later]) ] )

LIBS="$save_LIBS"
LDFLAGS="$save_LDFLAGS"
CPPFLAGS="$save_CPPFLAGS"

AC_SUBST(TOR_LIBEVENT_LIBS)

dnl ------------------------------------------------------
dnl Where do you live, libm?

dnl On some platforms (Haiku/BeOS) the math library is
dnl part of libroot. In which case don't link against lm
TOR_LIB_MATH=""
save_LIBS="$LIBS"
AC_SEARCH_LIBS(pow, [m], , AC_MSG_ERROR([Could not find pow in libm or libc.]))
if test "$ac_cv_search_pow" != "none required"; then
    TOR_LIB_MATH="$ac_cv_search_pow"
fi
LIBS="$save_LIBS"
AC_SUBST(TOR_LIB_MATH)

dnl ------------------------------------------------------
dnl Hello, NSS.  You're new around here.
if test "x$enable_nss" = "xyes"; then
  PKG_CHECK_MODULES(NSS,
     [nss],
     [have_nss=yes],
     [have_nss=no; AC_MSG_ERROR([You asked for NSS but I can't find it.])])
  AC_SUBST(NSS_CFLAGS)
  AC_SUBST(NSS_LIBS)
fi

dnl ------------------------------------------------------
dnl Where do you live, openssl?  And how do we call you?

if test "x$enable_nss" != "xyes"; then

tor_openssl_pkg_redhat="openssl"
tor_openssl_pkg_debian="libssl-dev"
tor_openssl_devpkg_redhat="openssl-devel"
tor_openssl_devpkg_debian="libssl-dev"

ALT_openssl_WITHVAL=""
AC_ARG_WITH(ssl-dir,
  AS_HELP_STRING(--with-ssl-dir=PATH, [obsolete alias for --with-openssl-dir]),
  [
      if test "x$withval" != "xno" && test "x$withval" != "x"; then
         ALT_openssl_WITHVAL="$withval"
      fi
  ])

AC_MSG_NOTICE([Now, we'll look for OpenSSL >= 1.0.1])
TOR_SEARCH_LIBRARY(openssl, $tryssldir, [-lssl -lcrypto $TOR_LIB_GDI $TOR_LIB_WS32],
    [#include <openssl/ssl.h>
     char *getenv(const char *);],
    [struct ssl_cipher_st;
     unsigned SSL_CIPHER_get_id(const struct ssl_cipher_st *);
     char *getenv(const char *);],
    dnl This funny-looking test program calls getenv, so that the compiler
    dnl will neither make code that call SSL_CIPHER_get_id(NULL) [producing
    dnl a crash], nor optimize out the call to SSL_CIPHER_get_id().
    dnl We look for SSL_cipher_get_id() because it is present in
    dnl OpenSSL >=1.0.1, because it is not deprecated, and because Tor
    dnl depends on it.
    [if (getenv("THIS_SHOULDNT_BE_SET_X201803")) SSL_CIPHER_get_id((void *)0);], [],
    [/usr/local/opt/openssl /usr/local/openssl /usr/lib/openssl /usr/local/ssl /usr/lib/ssl /usr/local /opt/openssl])

dnl XXXX check for OPENSSL_VERSION_NUMBER == SSLeay()

if test "$enable_static_openssl" = "yes"; then
   if test "$tor_cv_library_openssl_dir" = "(system)"; then
     AC_MSG_ERROR("You must specify an explicit --with-openssl-dir=x option when using --enable-static-openssl")
   else
     TOR_OPENSSL_LIBS="$TOR_LIBDIR_openssl/libssl.a $TOR_LIBDIR_openssl/libcrypto.a"
   fi
else
     TOR_OPENSSL_LIBS="-lssl -lcrypto"
fi
AC_SUBST(TOR_OPENSSL_LIBS)

dnl Now check for particular openssl functions.
save_LIBS="$LIBS"
save_LDFLAGS="$LDFLAGS"
save_CPPFLAGS="$CPPFLAGS"
LIBS="$TOR_OPENSSL_LIBS $LIBS"
LDFLAGS="$TOR_LDFLAGS_openssl $LDFLAGS"
CPPFLAGS="$TOR_CPPFLAGS_openssl $CPPFLAGS"

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <openssl/opensslv.h>
#if !defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER < 0x1000100fL
#error "too old"
#endif
   ]], [[]])],
   [ : ],
   [ AC_MSG_ERROR([OpenSSL is too old. We require 1.0.1 or later. You can specify a path to a newer one with --with-openssl-dir.]) ])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <openssl/opensslv.h>
#include <openssl/evp.h>
#if defined(OPENSSL_NO_EC) || defined(OPENSSL_NO_ECDH) || defined(OPENSSL_NO_ECDSA)
#error "no ECC"
#endif
#if !defined(NID_X9_62_prime256v1) || !defined(NID_secp224r1)
#error "curves unavailable"
#endif
   ]], [[]])],
   [ : ],
   [ AC_MSG_ERROR([OpenSSL is built without full ECC support, including curves P256 and P224. You can specify a path to one with ECC support with --with-openssl-dir.]) ])

AC_CHECK_MEMBERS([struct ssl_method_st.get_cipher_by_char], , ,
[#include <openssl/ssl.h>
])

AC_CHECK_FUNCS([ \
		ERR_load_KDF_strings \
		SSL_SESSION_get_master_key \
		SSL_get_server_random \
                SSL_get_client_ciphers \
                SSL_get_client_random \
                SSL_CTX_set1_groups_list \
		SSL_CIPHER_find \
                SSL_CTX_set_security_level \
		TLS_method
	       ])

dnl Check if OpenSSL has scrypt implementation.
AC_CHECK_FUNCS([ EVP_PBE_scrypt ])

dnl Check if OpenSSL structures are opaque
AC_CHECK_MEMBERS([SSL.state], , ,
[#include <openssl/ssl.h>
])

AC_CHECK_SIZEOF(SHA_CTX, , [AC_INCLUDES_DEFAULT()
#include <openssl/sha.h>
])

fi # enable_nss

dnl ======================================================================
dnl Can we use KIST?

dnl Define the set of checks for KIST scheduler support.
AC_DEFUN([CHECK_KIST_SUPPORT],[
  dnl KIST needs struct tcp_info and for certain members to exist.
  AC_CHECK_MEMBERS(
    [struct tcp_info.tcpi_unacked, struct tcp_info.tcpi_snd_mss],
    , ,[[#include <netinet/tcp.h>]])
  dnl KIST needs SIOCOUTQNSD to exist for an ioctl call.
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [
                     #include <linux/sockios.h>
                     #ifndef SIOCOUTQNSD
                     #error
                     #endif
                     ])], have_siocoutqnsd=yes, have_siocoutqnsd=no)
  if test "x$have_siocoutqnsd" = "xyes"; then
    if test "x$ac_cv_member_struct_tcp_info_tcpi_unacked" = "xyes"; then
      if test "x$ac_cv_member_struct_tcp_info_tcpi_snd_mss" = "xyes"; then
        have_kist_support=yes
      fi
    fi
  fi
])
dnl Now, trigger the check.
CHECK_KIST_SUPPORT
AS_IF([test "x$have_kist_support" = "xyes"],
      [AC_DEFINE(HAVE_KIST_SUPPORT, 1, [Defined if KIST scheduler is supported
                                        on this system])],
      [AC_MSG_NOTICE([KIST scheduler can't be used. Missing support.])])

LIBS="$save_LIBS"
LDFLAGS="$save_LDFLAGS"
CPPFLAGS="$save_CPPFLAGS"

dnl ------------------------------------------------------
dnl Where do you live, zlib?  And how do we call you?

tor_zlib_pkg_redhat="zlib"
tor_zlib_pkg_debian="zlib1g"
tor_zlib_devpkg_redhat="zlib-devel"
tor_zlib_devpkg_debian="zlib1g-dev"

TOR_SEARCH_LIBRARY(zlib, $tryzlibdir, [-lz],
    [#include <zlib.h>],
    [const char * zlibVersion(void);],
    [zlibVersion();], [--with-zlib-dir],
    [/opt/zlib])

if test "$enable_static_zlib" = "yes"; then
   if test "$tor_cv_library_zlib_dir" = "(system)"; then
     AC_MSG_ERROR("You must specify an explicit --with-zlib-dir=x option when
 using --enable-static-zlib")
   else
     TOR_ZLIB_LIBS="$TOR_LIBDIR_zlib/libz.a"
   fi
else
     TOR_ZLIB_LIBS="-lz"
fi
AC_SUBST(TOR_ZLIB_LIBS)

dnl ------------------------------------------------------
dnl Where we do we find lzma?

AC_ARG_ENABLE(lzma,
      AS_HELP_STRING(--enable-lzma, [enable support for the LZMA compression scheme.]),
      [case "${enableval}" in
        "yes") lzma=true ;;
        "no")  lzma=false ;;
        * ) AC_MSG_ERROR(bad value for --enable-lzma) ;;
      esac], [lzma=auto])

if test "x$enable_lzma" = "xno"; then
    have_lzma=no;
else
    PKG_CHECK_MODULES([LZMA],
                      [liblzma],
                      have_lzma=yes,
                      have_lzma=no)

    if test "x$have_lzma" = "xno" ; then
        AC_MSG_WARN([Unable to find liblzma.])
    fi
fi

if test "x$have_lzma" = "xyes"; then
    AC_DEFINE(HAVE_LZMA,1,[Have LZMA])
    TOR_LZMA_CFLAGS="${LZMA_CFLAGS}"
    TOR_LZMA_LIBS="${LZMA_LIBS}"
fi
AC_SUBST(TOR_LZMA_CFLAGS)
AC_SUBST(TOR_LZMA_LIBS)

dnl ------------------------------------------------------
dnl Where we do we find zstd?

AC_ARG_ENABLE(zstd,
      AS_HELP_STRING(--enable-zstd, [enable support for the Zstandard compression scheme.]),
      [case "${enableval}" in
        "yes") zstd=true ;;
        "no")  zstd=false ;;
        * ) AC_MSG_ERROR(bad value for --enable-zstd) ;;
      esac], [zstd=auto])

if test "x$enable_zstd" = "xno"; then
    have_zstd=no;
else
    PKG_CHECK_MODULES([ZSTD],
                      [libzstd >= 1.1],
                      have_zstd=yes,
                      have_zstd=no)

    if test "x$have_zstd" = "xno" ; then
        AC_MSG_WARN([Unable to find libzstd.])
    fi
fi

if test "x$have_zstd" = "xyes"; then
    AC_DEFINE(HAVE_ZSTD,1,[Have Zstd])
    TOR_ZSTD_CFLAGS="${ZSTD_CFLAGS}"
    TOR_ZSTD_LIBS="${ZSTD_LIBS}"

    dnl now check for zstd functions
    save_LIBS="$LIBS"
    save_CFLAGS="$CFLAGS"
    LIBS="$LIBS $ZSTD_LIBS"
    CFLAGS="$CFLAGS $ZSTD_CFLAGS"
    AC_CHECK_FUNCS(ZSTD_estimateCStreamSize \
                   ZSTD_estimateDCtxSize)
    LIBS="$save_LIBS"
    CFLAGS="$save_CFLAGS"
fi
AC_SUBST(TOR_ZSTD_CFLAGS)
AC_SUBST(TOR_ZSTD_LIBS)

dnl ----------------------------------------------------------------------
dnl Check if libcap is available for capabilities.

tor_cap_pkg_debian="libcap2"
tor_cap_pkg_redhat="libcap"
tor_cap_devpkg_debian="libcap-dev"
tor_cap_devpkg_redhat="libcap-devel"

AC_CHECK_LIB([cap], [cap_init], [],
  AC_MSG_NOTICE([Libcap was not found. Capabilities will not be usable.])
)
AC_CHECK_FUNCS(cap_set_proc)

dnl ---------------------------------------------------------------------
dnl Now that we know about our major libraries, we can check for compiler
dnl and linker hardening options.  We need to do this with the libraries known,
dnl since sometimes the linker will like an option but not be willing to
dnl use it with a build of a library.

all_ldflags_for_check="$TOR_LDFLAGS_zlib $TOR_LDFLAGS_openssl $TOR_LDFLAGS_libevent"
all_libs_for_check="$TOR_ZLIB_LIBS $TOR_LIB_MATH $TOR_LIBEVENT_LIBS $TOR_OPENSSL_LIBS $TOR_SYSTEMD_LIBS $TOR_LIB_WS32 $TOR_LIB_GDI $TOR_LIB_USERENV $TOR_CAP_LIBS"

CFLAGS_FTRAPV=
CFLAGS_FWRAPV=
CFLAGS_ASAN=
CFLAGS_UBSAN=


AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [
#if !defined(__clang__)
#error
#endif])], have_clang=yes, have_clang=no)

if test "x$enable_pic" = "xyes"; then
    TOR_CHECK_CFLAGS(-fPIC)
fi

if test "x$enable_gcc_hardening" != "xno"; then
    CFLAGS="$CFLAGS -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2"
    if test "x$have_clang" = "xyes"; then
        TOR_CHECK_CFLAGS(-Qunused-arguments)
    fi
    TOR_CHECK_CFLAGS(-fstack-protector-all, also_link)
    AS_VAR_PUSHDEF([can_compile], [tor_cv_cflags_-fstack-protector-all])
    AS_VAR_PUSHDEF([can_link], [tor_can_link_-fstack-protector-all])
m4_ifdef([AS_VAR_IF],[
    AS_VAR_IF(can_compile, [yes],
        AS_VAR_IF(can_link, [yes],
                  [],
                  AC_MSG_ERROR([We tried to build with stack protection; it looks like your compiler supports it but your libc does not provide it. Are you missing libssp? (You can --disable-gcc-hardening to ignore this error.)]))
        )])
    AS_VAR_POPDEF([can_link])
    AS_VAR_POPDEF([can_compile])
    TOR_CHECK_CFLAGS(-Wstack-protector)
    TOR_CHECK_CFLAGS(--param ssp-buffer-size=1)
    if test "$bwin32" = "false" && test "$enable_libfuzzer" != "yes" && test "$enable_oss_fuzz" != "yes"; then
       if test "$enable_pic" != "yes"; then
           # If we have already enabled -fPIC, then we don't also need to
           # compile with -fPIE...
           TOR_CHECK_CFLAGS(-fPIE)
       fi
       # ... but we want to link our executables with -pie in any case, since
       # they're executables, not a library.
       TOR_CHECK_LDFLAGS(-pie, "$all_ldflags_for_check", "$all_libs_for_check")
    fi
    TOR_TRY_COMPILE_WITH_CFLAGS(-fwrapv, also_link, CFLAGS_FWRAPV="-fwrapv", true)
fi

if test "$fragile_hardening" = "yes"; then
    TOR_TRY_COMPILE_WITH_CFLAGS(-ftrapv, also_link, CFLAGS_FTRAPV="-ftrapv", true)
   if test "$tor_cv_cflags__ftrapv" = "yes" && test "$tor_can_link__ftrapv" != "yes"; then
      AC_MSG_WARN([The compiler supports -ftrapv, but for some reason I was not able to link with -ftrapv. Are you missing run-time support? Run-time hardening will not work as well as it should.])
   fi

   if test "$tor_cv_cflags__ftrapv" != "yes"; then
     AC_MSG_ERROR([You requested fragile hardening, but the compiler does not seem to support -ftrapv.])
   fi

   TOR_TRY_COMPILE_WITH_CFLAGS([-fsanitize=address], also_link, CFLAGS_ASAN="-fsanitize=address", true)
    if test "$tor_cv_cflags__fsanitize_address" = "yes" && test "$tor_can_link__fsanitize_address" != "yes"; then
      AC_MSG_ERROR([The compiler supports -fsanitize=address, but for some reason I was not able to link when using it. Are you missing run-time support? With GCC you need libubsan.*, and with Clang you need libclang_rt.ubsan*])
    fi

   TOR_TRY_COMPILE_WITH_CFLAGS([-fsanitize=undefined], also_link, CFLAGS_UBSAN="-fsanitize=undefined", true)
    if test "$tor_cv_cflags__fsanitize_address" = "yes" && test "$tor_can_link__fsanitize_address" != "yes"; then
      AC_MSG_ERROR([The compiler supports -fsanitize=undefined, but for some reason I was not able to link when using it. Are you missing run-time support? With GCC you need libasan.*, and with Clang you need libclang_rt.ubsan*])
    fi

TOR_CHECK_CFLAGS([-fno-omit-frame-pointer])
fi

dnl Find the correct libraries to add in order to use the sanitizers.
dnl
dnl When building Rust, Cargo will run the linker with the -nodefaultlibs
dnl option, which will prevent the compiler from linking the sanitizer
dnl libraries it needs.  We need to specify them manually.
dnl
dnl What's more, we need to specify them in a linker script rather than
dnl from build.rs: these options aren't allowed in the cargo:rustc-flags
dnl variable.
RUST_LINKER_OPTIONS=""
if test "x$have_clang" = "xyes"; then
	if test "x$CFLAGS_ASAN" != "x"; then
		RUST_LINKER_OPTIONS="$RUST_LINKER_OPTIONS -Clink-arg=$CFLAGS_ASAN -Cdefault-linker-libraries"
	fi
	if test "x$CFLAGS_UBSAN" != "x"; then
        	RUST_LINKER_OPTIONS="$RUST_LINKER_OPTIONS -Clink-arg=$CFLAGS_UBSAN -Cdefault-linker-libraries"
	fi
else
	if test "x$CFLAGS_ASAN" != "x"; then
		RUST_LINKER_OPTIONS="$RUST_LINKER_OPTIONS -Clink-arg=-fsanitize=address -Cdefault-linker-libraries"
	fi
	if test "x$CFLAGS_UBSAN" != "x"; then
        	RUST_LINKER_OPTIONS="$RUST_LINKER_OPTIONS -Clink-arg=-fsanitize=undefined -Cdefault-linker-libraries"
	fi
fi
AC_SUBST(RUST_LINKER_OPTIONS)

CFLAGS_BUGTRAP="$CFLAGS_FTRAPV $CFLAGS_ASAN $CFLAGS_UBSAN"
CFLAGS_CONSTTIME="$CFLAGS_FWRAPV"

mulodi_fixes_ftrapv=no
if test "$have_clang" = "yes"; then
  saved_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $CFLAGS_FTRAPV"
  AC_MSG_CHECKING([whether clang -ftrapv can link a 64-bit int multiply])
  AC_LINK_IFELSE([
      AC_LANG_SOURCE([[
          #include <stdint.h>
          #include <stdlib.h>
	  int main(int argc, char **argv)
	  {
            int64_t x = ((int64_t)atoi(argv[1])) * (int64_t)atoi(argv[2])
	                * (int64_t)atoi(argv[3]);
	    return x == 9;
	  } ]])],
	  [ftrapv_can_link=yes; AC_MSG_RESULT([yes])],
	  [ftrapv_can_link=no; AC_MSG_RESULT([no])])
  if test "$ftrapv_can_link" = "no"; then
    AC_MSG_CHECKING([whether defining __mulodi4 fixes that])
    AC_LINK_IFELSE([
      AC_LANG_SOURCE([[
          #include <stdint.h>
          #include <stdlib.h>
	  int64_t __mulodi4(int64_t a, int64_t b, int *overflow) {
             *overflow=0;
	     return a;
          }
	  int main(int argc, char **argv)
	  {
            int64_t x = ((int64_t)atoi(argv[1])) * (int64_t)atoi(argv[2])
	                * (int64_t)atoi(argv[3]);
	    return x == 9;
	  } ]])],
	  [mulodi_fixes_ftrapv=yes; AC_MSG_RESULT([yes])],
	  [mulodi_fixes_ftrapv=no; AC_MSG_RESULT([no])])
  fi
  CFLAGS="$saved_CFLAGS"
fi

AM_CONDITIONAL(ADD_MULODI4, test "$mulodi_fixes_ftrapv" = "yes")

dnl These cflags add bunches of branches, and we haven't been able to
dnl persuade ourselves that they're suitable for code that needs to be
dnl constant time.
AC_SUBST(CFLAGS_BUGTRAP)
dnl These cflags are variant ones sutable for code that needs to be
dnl constant-time.
AC_SUBST(CFLAGS_CONSTTIME)

if test "x$enable_linker_hardening" != "xno"; then
    TOR_CHECK_LDFLAGS(-z relro -z now, "$all_ldflags_for_check", "$all_libs_for_check")
fi

# For backtrace support
TOR_CHECK_LDFLAGS(-rdynamic)

dnl ------------------------------------------------------
dnl Now see if we have a -fomit-frame-pointer compiler option.

saved_CFLAGS="$CFLAGS"
TOR_CHECK_CFLAGS(-fomit-frame-pointer)
F_OMIT_FRAME_POINTER=''
if test "$saved_CFLAGS" != "$CFLAGS"; then
  if test "$fragile_hardening" = "yes"; then
    F_OMIT_FRAME_POINTER='-fomit-frame-pointer'
  fi
fi
CFLAGS="$saved_CFLAGS"
AC_SUBST(F_OMIT_FRAME_POINTER)

dnl ------------------------------------------------------
dnl If we are adding -fomit-frame-pointer (or if the compiler's doing it
dnl for us, as GCC 4.6 and later do at many optimization levels), then
dnl we should try to add -fasynchronous-unwind-tables so that our backtrace
dnl code will work.
TOR_CHECK_CFLAGS(-fasynchronous-unwind-tables)

dnl ============================================================
dnl Check for libseccomp

if test "x$enable_seccomp" != "xno"; then
  AC_CHECK_HEADERS([seccomp.h])
  AC_SEARCH_LIBS(seccomp_init, [seccomp])
fi

dnl ============================================================
dnl Check for libscrypt

if test "x$enable_libscrypt" != "xno"; then
  AC_CHECK_HEADERS([libscrypt.h])
  AC_SEARCH_LIBS(libscrypt_scrypt, [scrypt])
  AC_CHECK_FUNCS([libscrypt_scrypt])
fi

dnl ============================================================
dnl We need an implementation of curve25519.

dnl set these defaults.
build_curve25519_donna=no
build_curve25519_donna_c64=no
use_curve25519_donna=no
use_curve25519_nacl=no
CURVE25519_LIBS=

dnl The best choice is using curve25519-donna-c64, but that requires
dnl that we
AC_CACHE_CHECK([whether we can use curve25519-donna-c64],
  tor_cv_can_use_curve25519_donna_c64,
  [AC_RUN_IFELSE(
    [AC_LANG_PROGRAM([dnl
      #include <stdint.h>
      typedef unsigned uint128_t __attribute__((mode(TI)));
  int func(uint64_t a, uint64_t b) {
           uint128_t c = ((uint128_t)a) * b;
           int ok = ((uint64_t)(c>>96)) == 522859 &&
             (((uint64_t)(c>>64))&0xffffffffL) == 3604448702L &&
                 (((uint64_t)(c>>32))&0xffffffffL) == 2351960064L &&
                 (((uint64_t)(c))&0xffffffffL) == 0;
           return ok;
      }
  ], [dnl
    int ok = func( ((uint64_t)2000000000) * 1000000000,
                   ((uint64_t)1234567890) << 24);
        return !ok;
      ])],
  [tor_cv_can_use_curve25519_donna_c64=yes],
      [tor_cv_can_use_curve25519_donna_c64=no],
  [AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([dnl
      #include <stdint.h>
      typedef unsigned uint128_t __attribute__((mode(TI)));
  int func(uint64_t a, uint64_t b) {
           uint128_t c = ((uint128_t)a) * b;
           int ok = ((uint64_t)(c>>96)) == 522859 &&
             (((uint64_t)(c>>64))&0xffffffffL) == 3604448702L &&
                 (((uint64_t)(c>>32))&0xffffffffL) == 2351960064L &&
                 (((uint64_t)(c))&0xffffffffL) == 0;
           return ok;
      }
  ], [dnl
    int ok = func( ((uint64_t)2000000000) * 1000000000,
    	         ((uint64_t)1234567890) << 24);
        return !ok;
      ])],
          [tor_cv_can_use_curve25519_donna_c64=cross],
      [tor_cv_can_use_curve25519_donna_c64=no])])])

AC_CHECK_HEADERS([crypto_scalarmult_curve25519.h \
                  nacl/crypto_scalarmult_curve25519.h])

AC_CACHE_CHECK([for nacl compiled with a fast curve25519 implementation],
  tor_cv_can_use_curve25519_nacl,
  [tor_saved_LIBS="$LIBS"
   LIBS="$LIBS -lnacl"
   AC_LINK_IFELSE(
     [AC_LANG_PROGRAM([dnl
       #ifdef HAVE_CRYPTO_SCALARMULT_CURVE25519_H
       #include <crypto_scalarmult_curve25519.h>
   #elif defined(HAVE_NACL_CRYPTO_SCALARMULT_CURVE25519_H)
   #include <nacl/crypto_scalarmult_curve25519.h>
   #endif
       #ifdef crypto_scalarmult_curve25519_ref_BYTES
   #error Hey, this is the reference implementation! That's not fast.
   #endif
     ], [
   unsigned char *a, *b, *c; crypto_scalarmult_curve25519(a,b,c);
     ])], [tor_cv_can_use_curve25519_nacl=yes],
     [tor_cv_can_use_curve25519_nacl=no])
   LIBS="$tor_saved_LIBS" ])

 dnl Okay, now we need to figure out which one to actually use. Fall back
 dnl to curve25519-donna.c

 if test "x$tor_cv_can_use_curve25519_donna_c64" != "xno"; then
   build_curve25519_donna_c64=yes
   use_curve25519_donna=yes
 elif test "x$tor_cv_can_use_curve25519_nacl" = "xyes"; then
   use_curve25519_nacl=yes
   CURVE25519_LIBS=-lnacl
 else
   build_curve25519_donna=yes
   use_curve25519_donna=yes
 fi

if test "x$use_curve25519_donna" = "xyes"; then
  AC_DEFINE(USE_CURVE25519_DONNA, 1,
            [Defined if we should use an internal curve25519_donna{,_c64} implementation])
fi
if test "x$use_curve25519_nacl" = "xyes"; then
  AC_DEFINE(USE_CURVE25519_NACL, 1,
            [Defined if we should use a curve25519 from nacl])
fi
AM_CONDITIONAL(BUILD_CURVE25519_DONNA,
  test "x$build_curve25519_donna" = "xyes")
AM_CONDITIONAL(BUILD_CURVE25519_DONNA_C64,
  test "x$build_curve25519_donna_c64" = "xyes")
AC_SUBST(CURVE25519_LIBS)

dnl Make sure to enable support for large off_t if available.
AC_SYS_LARGEFILE

AC_CHECK_HEADERS([errno.h \
		  fcntl.h \
		  signal.h \
		  string.h \
		  sys/capability.h \
		  sys/fcntl.h \
		  sys/stat.h \
		  sys/time.h \
		  sys/types.h \
		  time.h \
		  unistd.h \
		  arpa/inet.h \
		  crt_externs.h \
		  execinfo.h \
		  gnu/libc-version.h \
		  grp.h \
		  ifaddrs.h \
		  inttypes.h \
		  limits.h \
		  linux/types.h \
		  machine/limits.h \
		  malloc.h \
		  malloc/malloc.h \
		  malloc_np.h \
		  netdb.h \
		  netinet/in.h \
		  netinet/in6.h \
		  pwd.h \
		  readpassphrase.h \
		  stdatomic.h \
		  sys/eventfd.h \
		  sys/file.h \
		  sys/ioctl.h \
		  sys/limits.h \
		  sys/mman.h \
		  sys/param.h \
		  sys/prctl.h \
		  sys/random.h \
		  sys/resource.h \
		  sys/select.h \
		  sys/socket.h \
		  sys/statvfs.h \
		  sys/syscall.h \
		  sys/sysctl.h \
		  sys/syslimits.h \
		  sys/time.h \
		  sys/types.h \
		  sys/un.h \
		  sys/utime.h \
		  sys/wait.h \
		  syslog.h \
		  utime.h])

AC_CHECK_HEADERS(sys/param.h)

AC_CHECK_HEADERS(net/if.h, net_if_found=1, net_if_found=0,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif])
AC_CHECK_HEADERS(net/pfvar.h, net_pfvar_found=1, net_pfvar_found=0,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif])

AC_CHECK_HEADERS(linux/if.h,[],[],
[
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
])

AC_CHECK_HEADERS(linux/netfilter_ipv4.h,
        linux_netfilter_ipv4=1, linux_netfilter_ipv4=0,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif])

AC_CHECK_HEADERS(linux/netfilter_ipv6/ip6_tables.h,
        linux_netfilter_ipv6_ip6_tables=1, linux_netfilter_ipv6_ip6_tables=0,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_LINUX_IF_H
#include <linux/if.h>
#endif])

transparent_ok=0
if test "x$net_if_found" = "x1" && test "x$net_pfvar_found" = "x1"; then
  transparent_ok=1
fi
if test "x$linux_netfilter_ipv4" = "x1"; then
  transparent_ok=1
fi
if test "x$linux_netfilter_ipv6_ip6_tables" = "x1"; then
  transparent_ok=1
fi
if test "x$transparent_ok" = "x1"; then
  AC_DEFINE(USE_TRANSPARENT, 1, "Define to enable transparent proxy support")
else
  AC_MSG_NOTICE([Transparent proxy support enabled, but missing headers.])
fi

AC_CHECK_MEMBERS([struct timeval.tv_sec], , ,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif])

AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(__int64)
AC_CHECK_SIZEOF(void *)
AC_CHECK_SIZEOF(time_t)
AC_CHECK_SIZEOF(size_t)
AC_CHECK_SIZEOF(pid_t)

AC_CHECK_TYPES([uint, u_char, ssize_t])

AC_PC_FROM_UCONTEXT([:])

dnl used to include sockaddr_storage, but everybody has that.
AC_CHECK_TYPES([struct in6_addr, struct sockaddr_in6, sa_family_t], , ,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef _WIN32
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
])
AC_CHECK_MEMBERS([struct in6_addr.s6_addr32, struct in6_addr.s6_addr16, struct sockaddr_in.sin_len, struct sockaddr_in6.sin6_len], , ,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef _WIN32
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
])

AC_CHECK_TYPES([rlim_t], , ,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
])

AX_CHECK_SIGN([time_t],
       [ : ],
       [ : ], [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
])

if test "$ax_cv_decl_time_t_signed" = "no"; then
  AC_MSG_ERROR([You have an unsigned time_t; Tor does not support that. Please tell the Tor developers about your interesting platform.])
fi

AX_CHECK_SIGN([size_t],
       [ tor_cv_size_t_signed=yes ],
       [ tor_cv_size_t_signed=no ], [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
])

if test "$ax_cv_decl_size_t_signed" = "yes"; then
  AC_MSG_ERROR([You have a signed size_t; that's grossly nonconformant.])
fi

AX_CHECK_SIGN([enum always],
       [ AC_DEFINE(ENUM_VALS_ARE_SIGNED, 1, [Define if enum is always signed]) ],
       [ : ], [
 enum always { AAA, BBB, CCC };
])

AC_CHECK_SIZEOF(socklen_t, , [AC_INCLUDES_DEFAULT()
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
])

# We want to make sure that we _don't_ have a cell_t defined, like IRIX does.

AC_CHECK_SIZEOF(cell_t)

# Let's see if stdatomic works. (There are some debian clangs that screw it
# up; see Tor bug #26779 and debian bug 903709.)
AC_CACHE_CHECK([whether C11 stdatomic.h actually works],
               tor_cv_stdatomic_works,
[AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <stdatomic.h>
struct x { atomic_size_t y; };
void try_atomic_init(struct x *xx)
{
  atomic_init(&xx->y, 99);
  atomic_fetch_add(&xx->y, 1);
}
]])], [tor_cv_stdatomic_works=yes], [tor_cv_stdatomic_works=no])])

if test "$tor_cv_stdatomic_works" = "yes"; then
   AC_DEFINE(STDATOMIC_WORKS, 1, [Set to 1 if we can compile a simple stdatomic example.])
elif test "$ac_cv_header_stdatomic_h" = "yes"; then
   AC_MSG_WARN([Your compiler provides the stdatomic.h header, but it doesn't seem to work.  I'll pretend it isn't there. If you are using Clang on Debian, maybe this is because of https://bugs.debian.org/903709 ])
fi

# Now make sure that NULL can be represented as zero bytes.
AC_CACHE_CHECK([whether memset(0) sets pointers to NULL], tor_cv_null_is_zero,
[AC_RUN_IFELSE([AC_LANG_SOURCE(
[[#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
int main () { char *p1,*p2; p1=NULL; memset(&p2,0,sizeof(p2));
return memcmp(&p1,&p2,sizeof(char*))?1:0; }]])],
       [tor_cv_null_is_zero=yes],
       [tor_cv_null_is_zero=no],
       [tor_cv_null_is_zero=cross])])

if test "$tor_cv_null_is_zero" = "cross"; then
  # Cross-compiling; let's hope that the target isn't raving mad.
  AC_MSG_NOTICE([Cross-compiling: we'll assume that NULL is represented as a sequence of 0-valued bytes.])
fi

if test "$tor_cv_null_is_zero" != "no"; then
  AC_DEFINE([NULL_REP_IS_ZERO_BYTES], 1,
            [Define to 1 iff memset(0) sets pointers to NULL])
fi

AC_CACHE_CHECK([whether memset(0) sets doubles to 0.0], tor_cv_dbl0_is_zero,
[AC_RUN_IFELSE([AC_LANG_SOURCE(
[[#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
int main () { double d1,d2; d1=0; memset(&d2,0,sizeof(d2));
return memcmp(&d1,&d2,sizeof(d1))?1:0; }]])],
       [tor_cv_dbl0_is_zero=yes],
       [tor_cv_dbl0_is_zero=no],
       [tor_cv_dbl0_is_zero=cross])])

if test "$tor_cv_dbl0_is_zero" = "cross"; then
  # Cross-compiling; let's hope that the target isn't raving mad.
  AC_MSG_NOTICE([Cross-compiling: we'll assume that 0.0 can be represented as a sequence of 0-valued bytes.])
fi

if test "$tor_cv_dbl0_is_zero" != "no"; then
  AC_DEFINE([DOUBLE_0_REP_IS_ZERO_BYTES], 1,
            [Define to 1 iff memset(0) sets doubles to 0.0])
fi

# And what happens when we malloc zero?
AC_CACHE_CHECK([whether we can malloc(0) safely.], tor_cv_malloc_zero_works,
[AC_RUN_IFELSE([AC_LANG_SOURCE(
[[#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
int main () { return malloc(0)?0:1; }]])],
       [tor_cv_malloc_zero_works=yes],
       [tor_cv_malloc_zero_works=no],
       [tor_cv_malloc_zero_works=cross])])

if test "$tor_cv_malloc_zero_works" = "cross"; then
  # Cross-compiling; let's hope that the target isn't raving mad.
  AC_MSG_NOTICE([Cross-compiling: we'll assume that we need to check malloc() arguments for 0.])
fi

if test "$tor_cv_malloc_zero_works" = "yes"; then
  AC_DEFINE([MALLOC_ZERO_WORKS], 1,
            [Define to 1 iff malloc(0) returns a pointer])
fi

# whether we seem to be in a 2s-complement world.
AC_CACHE_CHECK([whether we are using 2s-complement arithmetic], tor_cv_twos_complement,
[AC_RUN_IFELSE([AC_LANG_SOURCE(
[[int main () { int problem = ((-99) != (~99)+1);
return problem ? 1 : 0; }]])],
       [tor_cv_twos_complement=yes],
       [tor_cv_twos_complement=no],
       [tor_cv_twos_complement=cross])])

if test "$tor_cv_twos_complement" = "cross"; then
  # Cross-compiling; let's hope that the target isn't raving mad.
  AC_MSG_NOTICE([Cross-compiling: we'll assume that negative integers are represented with two's complement.])
fi

if test "$tor_cv_twos_complement" != "no"; then
  AC_DEFINE([USING_TWOS_COMPLEMENT], 1,
            [Define to 1 iff we represent negative integers with
             two's complement])
fi

# What does shifting a negative value do?
AC_CACHE_CHECK([whether right-shift on negative values does sign-extension], tor_cv_sign_extend,
[AC_RUN_IFELSE([AC_LANG_SOURCE(
[[int main () { int okay = (-60 >> 8) == -1; return okay ? 0 : 1; }]])],
       [tor_cv_sign_extend=yes],
       [tor_cv_sign_extend=no],
       [tor_cv_sign_extend=cross])])

if test "$tor_cv_sign_extend" = "cross"; then
  # Cross-compiling; let's hope that the target isn't raving mad.
  AC_MSG_NOTICE([Cross-compiling: we'll assume that right-shifting negative integers causes sign-extension])
fi

if test "$tor_cv_sign_extend" != "no"; then
  AC_DEFINE([RSHIFT_DOES_SIGN_EXTEND], 1,
            [Define to 1 iff right-shifting a negative value performs sign-extension])
fi

# Is uint8_t the same type as unsigned char?
AC_CACHE_CHECK([whether uint8_t is the same type as unsigned char], tor_cv_uint8_uchar,
[AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <stdint.h>
extern uint8_t c;
unsigned char c;]])],
       [tor_cv_uint8_uchar=yes],
       [tor_cv_uint8_uchar=no],
       [tor_cv_uint8_uchar=cross])])

if test "$tor_cv_uint8_uchar" = "cross"; then
  AC_MSG_NOTICE([Cross-compiling: we'll assume that uint8_t is the same type as unsigned char])
fi

if test "$tor_cv_uint8_uchar" = "no"; then
  AC_MSG_ERROR([We assume that uint8_t is the same type as unsigned char, but your compiler disagrees.])
fi

AC_ARG_WITH(tcmalloc,
AS_HELP_STRING(--with-tcmalloc, [use tcmalloc memory allocation library. Deprecated; see --with-malloc]),
[ tcmalloc=yes ], [ tcmalloc=no ])

default_malloc=system

if test "x$enable_openbsd_malloc" = "xyes" ; then
  AC_MSG_NOTICE([The --enable-openbsd-malloc argument is deprecated; use --with-malloc=openbsd instead.])
  default_malloc=openbsd
fi

if test "x$tcmalloc" = "xyes"; then
  AC_MSG_NOTICE([The --with-tcmalloc argument is deprecated; use --with-malloc=tcmalloc instead.])
  default_malloc=tcmalloc
fi

AC_ARG_WITH(malloc,
   AS_HELP_STRING([--with-malloc=[system,jemalloc,tcmalloc,openbsd]],
                  [select special malloc implementation [system]]),
   [ malloc="$with_malloc" ], [ malloc="$default_malloc" ])

AS_CASE([$malloc],
  [tcmalloc], [
      PKG_CHECK_MODULES([TCMALLOC],
			[libtcmalloc],
			have_tcmalloc=yes,
			have_tcmalloc=no)

      if test "x$have_tcmalloc" = "xno" ; then
          AC_MSG_ERROR([Unable to find tcmalloc requested by --with-malloc.])
      fi

      CFLAGS="$CFLAGS $TCMALLOC_CFLAGS"
      LIBS="$TCMALLOC_LIBS $LIBS"
  ],

  [jemalloc], [
      PKG_CHECK_MODULES([JEMALLOC],
		        [jemalloc],
			have_jemalloc=yes,
			have_jemalloc=no)

      if test "x$have_tcmalloc" = "xno" ; then
          AC_MSG_ERROR([Unable to find jemalloc requested by --with-malloc.])
      fi

      CFLAGS="$CFLAGS $JEMALLOC_CFLAGS"
      LIBS="$JEMALLOC_LIBS $LIBS"
      using_custom_malloc=yes
  ],

  [openbsd], [
    AC_MSG_WARN([The openbsd malloc port is deprecated in Tor 0.3.5 and will be removed in a future version.])
    enable_openbsd_malloc=yes
  ],

  [system], [
     # handle this later, including the jemalloc fallback
    AC_CHECK_FUNCS(mallinfo)
  ],

  [AC_MSG_ERROR([--with-malloc=`$with_malloc' not supported, see --help])
])

AM_CONDITIONAL(USE_OPENBSD_MALLOC, test "x$enable_openbsd_malloc" = "xyes")

if test "$malloc" != "system"; then
  # Tell the C compiler not to use the system allocator functions.
  TOR_CHECK_CFLAGS([-fno-builtin-malloc -fno-builtin-realloc -fno-builtin-calloc -fno-builtin-free])
fi
if test "$using_custom_malloc" = "yes"; then
  # Tell the C compiler not to use the system allocator functions.
  TOR_CHECK_CFLAGS([-fno-builtin-malloc -fno-builtin-realloc -fno-builtin-calloc -fno-builtin-free])
fi

# By default, we're going to assume we don't have mlockall()
# bionic and other platforms have various broken mlockall subsystems.
# Some systems don't have a working mlockall, some aren't linkable,
# and some have it but don't declare it.
AC_CHECK_FUNCS(mlockall)
AC_CHECK_DECLS([mlockall], , , [
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif])

# Allow user to specify an alternate syslog facility
AC_ARG_WITH(syslog-facility,
AS_HELP_STRING(--with-syslog-facility=LOG, [syslog facility to use (default=LOG_DAEMON)]),
syslog_facility="$withval", syslog_facility="LOG_DAEMON")
AC_DEFINE_UNQUOTED(LOGFACILITY,$syslog_facility,[name of the syslog facility])
AC_SUBST(LOGFACILITY)

# Check if we have getresuid and getresgid
AC_CHECK_FUNCS(getresuid getresgid)

# Check for gethostbyname_r in all its glorious incompatible versions.
#   (This logic is based on that in Python's configure.in)
AH_TEMPLATE(HAVE_GETHOSTBYNAME_R,
  [Define this if you have any gethostbyname_r()])

AC_CHECK_FUNC(gethostbyname_r, [
  AC_MSG_CHECKING([how many arguments gethostbyname_r() wants])
  OLD_CFLAGS=$CFLAGS
  CFLAGS="$CFLAGS $MY_CPPFLAGS $MY_THREAD_CPPFLAGS $MY_CFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <netdb.h>
  ]], [[
    char *cp1, *cp2;
    struct hostent *h1, *h2;
    int i1, i2;
    (void)gethostbyname_r(cp1,h1,cp2,i1,&h2,&i2);
  ]])],[
    AC_DEFINE(HAVE_GETHOSTBYNAME_R)
    AC_DEFINE(HAVE_GETHOSTBYNAME_R_6_ARG, 1,
     [Define this if gethostbyname_r takes 6 arguments])
    AC_MSG_RESULT(6)
  ], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <netdb.h>
    ]], [[
      char *cp1, *cp2;
      struct hostent *h1;
      int i1, i2;
      (void)gethostbyname_r(cp1,h1,cp2,i1,&i2);
    ]])], [
      AC_DEFINE(HAVE_GETHOSTBYNAME_R)
      AC_DEFINE(HAVE_GETHOSTBYNAME_R_5_ARG, 1,
        [Define this if gethostbyname_r takes 5 arguments])
      AC_MSG_RESULT(5)
   ], [
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <netdb.h>
     ]], [[
       char *cp1;
       struct hostent *h1;
       struct hostent_data hd;
       (void) gethostbyname_r(cp1,h1,&hd);
     ]])], [
       AC_DEFINE(HAVE_GETHOSTBYNAME_R)
       AC_DEFINE(HAVE_GETHOSTBYNAME_R_3_ARG, 1,
         [Define this if gethostbyname_r takes 3 arguments])
       AC_MSG_RESULT(3)
     ], [
       AC_MSG_RESULT(0)
     ])
  ])
 ])
 CFLAGS=$OLD_CFLAGS
])

AC_CACHE_CHECK([whether the C compiler supports __func__],
  tor_cv_have_func_macro,
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
int main(int c, char **v) { puts(__func__); }])],
  tor_cv_have_func_macro=yes,
  tor_cv_have_func_macro=no))

AC_CACHE_CHECK([whether the C compiler supports __FUNC__],
  tor_cv_have_FUNC_macro,
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
int main(int c, char **v) { puts(__FUNC__); }])],
  tor_cv_have_FUNC_macro=yes,
  tor_cv_have_FUNC_macro=no))

AC_CACHE_CHECK([whether the C compiler supports __FUNCTION__],
  tor_cv_have_FUNCTION_macro,
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
int main(int c, char **v) { puts(__FUNCTION__); }])],
  tor_cv_have_FUNCTION_macro=yes,
  tor_cv_have_FUNCTION_macro=no))

AC_CACHE_CHECK([whether we have extern char **environ already declared],
  tor_cv_have_environ_declared,
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
int main(int c, char **v) { char **t = environ; }])],
  tor_cv_have_environ_declared=yes,
  tor_cv_have_environ_declared=no))

if test "$tor_cv_have_func_macro" = "yes"; then
  AC_DEFINE(HAVE_MACRO__func__, 1, [Defined if the compiler supports __func__])
fi

if test "$tor_cv_have_FUNC_macro" = "yes"; then
  AC_DEFINE(HAVE_MACRO__FUNC__, 1, [Defined if the compiler supports __FUNC__])
fi

if test "$tor_cv_have_FUNCTION_macro" = "yes"; then
  AC_DEFINE(HAVE_MACRO__FUNCTION__, 1,
           [Defined if the compiler supports __FUNCTION__])
fi

if test "$tor_cv_have_environ_declared" = "yes"; then
  AC_DEFINE(HAVE_EXTERN_ENVIRON_DECLARED, 1,
           [Defined if we have extern char **environ already declared])
fi

# $prefix stores the value of the --prefix command line option, or
# NONE if the option wasn't set.  In the case that it wasn't set, make
# it be the default, so that we can use it to expand directories now.
if test "x$prefix" = "xNONE"; then
  prefix=$ac_default_prefix
fi

# and similarly for $exec_prefix
if test "x$exec_prefix" = "xNONE"; then
  exec_prefix=$prefix
fi

if test "x$BUILDDIR" = "x"; then
  BUILDDIR=`pwd`
fi
AC_SUBST(BUILDDIR)
AH_TEMPLATE([BUILDDIR],[tor's build directory])
AC_DEFINE_UNQUOTED(BUILDDIR,"$BUILDDIR")

if test "x$SRCDIR" = "x"; then
  SRCDIR=$(cd "$srcdir"; pwd)
fi
AH_TEMPLATE([SRCDIR],[tor's sourcedir directory])
AC_DEFINE_UNQUOTED(SRCDIR,"$SRCDIR")

if test "x$CONFDIR" = "x"; then
  CONFDIR=`eval echo $sysconfdir/tor`
fi
AC_SUBST(CONFDIR)
AH_TEMPLATE([CONFDIR],[tor's configuration directory])
AC_DEFINE_UNQUOTED(CONFDIR,"$CONFDIR")

BINDIR=`eval echo $bindir`
AC_SUBST(BINDIR)
LOCALSTATEDIR=`eval echo $localstatedir`
AC_SUBST(LOCALSTATEDIR)

if test "$bwin32" = "true"; then
  # Test if the linker supports the --nxcompat and --dynamicbase options
  # for Windows
  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="-Wl,--nxcompat -Wl,--dynamicbase"
  AC_MSG_CHECKING([whether the linker supports DllCharacteristics])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([])],
    [AC_MSG_RESULT([yes])]
    [save_LDFLAGS="$save_LDFLAGS $LDFLAGS"],
    [AC_MSG_RESULT([no])]
  )
  LDFLAGS="$save_LDFLAGS"
fi

# Set CFLAGS _after_ all the above checks, since our warnings are stricter
# than autoconf's macros like.
if test "$GCC" = "yes"; then
  # Disable GCC's strict aliasing checks.  They are an hours-to-debug
  # accident waiting to happen.
  CFLAGS="$CFLAGS -Wall -fno-strict-aliasing"
else
  # Override optimization level for non-gcc compilers
  CFLAGS="$CFLAGS -O"
  enable_gcc_warnings=no
  enable_gcc_warnings_advisory=no
fi

# Warnings implies advisory-warnings and -Werror.
if test "$enable_gcc_warnings" = "yes"; then
  enable_gcc_warnings_advisory=yes
  enable_fatal_warnings=yes
fi

# OS X Lion started deprecating the system openssl. Let's just disable
# all deprecation warnings on OS X. Also, to potentially make the binary
# a little smaller, let's enable dead_strip.
case "$host_os" in

 darwin*)
    CFLAGS="$CFLAGS -Wno-deprecated-declarations"
    LDFLAGS="$LDFLAGS -dead_strip" ;;
esac

TOR_WARNING_FLAGS=""

# Add some more warnings which we use in development but not in the
# released versions.  (Some relevant gcc versions can't handle these.)
#
# Note that we have to do this near the end  of the autoconf process, or
# else we may run into problems when these warnings hit on the testing C
# programs that autoconf wants to build.
if test "x$enable_gcc_warnings_advisory" != "xno"; then

  case "$host" in
    *-*-openbsd* | *-*-bitrig*)
      # Some OpenBSD versions (like 4.8) have -Wsystem-headers by default.
      # That's fine, except that the headers don't pass -Wredundant-decls.
      # Therefore, let's disable -Wsystem-headers when we're building
      # with maximal warnings on OpenBSD.
      CFLAGS="$CFLAGS -Wno-system-headers" ;;
  esac

  CFLAGS_NOWARNINGS="$CFLAGS"

  # GCC4.3 users once report trouble with -Wstrict-overflow=5.  GCC5 users
  # have it work better.
  # CFLAGS="$CFLAGS -Wstrict-overflow=1"

  # This warning was added in gcc 4.3, but it appears to generate
  # spurious warnings in gcc 4.4.  I don't know if it works in 4.5.
  #CFLAGS="$CFLAGS -Wlogical-op"

  m4_foreach_w([warning_flag], [
     -Waddress
     -Waddress-of-array-temporary
     -Waddress-of-temporary
     -Wambiguous-macro
     -Wanonymous-pack-parens
     -Warc
     -Warc-abi
     -Warc-bridge-casts-disallowed-in-nonarc
     -Warc-maybe-repeated-use-of-weak
     -Warc-performSelector-leaks
     -Warc-repeated-use-of-weak
     -Warray-bounds
     -Warray-bounds-pointer-arithmetic
     -Wasm
     -Wasm-operand-widths
     -Watomic-properties
     -Watomic-property-with-user-defined-accessor
     -Wauto-import
     -Wauto-storage-class
     -Wauto-var-id
     -Wavailability
     -Wbackslash-newline-escape
     -Wbad-array-new-length
     -Wbind-to-temporary-copy
     -Wbitfield-constant-conversion
     -Wbool-conversion
     -Wbool-conversions
     -Wbuiltin-requires-header
     -Wchar-align
     -Wcompare-distinct-pointer-types
     -Wcomplex-component-init
     -Wconditional-type-mismatch
     -Wconfig-macros
     -Wconstant-conversion
     -Wconstant-logical-operand
     -Wconstexpr-not-const
     -Wcustom-atomic-properties
     -Wdangling-field
     -Wdangling-initializer-list
     -Wdate-time
     -Wdelegating-ctor-cycles
     -Wdeprecated-implementations
     -Wdeprecated-register
     -Wdirect-ivar-access
     -Wdiscard-qual
     -Wdistributed-object-modifiers
     -Wdivision-by-zero
     -Wdollar-in-identifier-extension
     -Wdouble-promotion
     -Wduplicate-decl-specifier
     -Wduplicate-enum
     -Wduplicate-method-arg
     -Wduplicate-method-match
     -Wduplicated-cond
     -Wdynamic-class-memaccess
     -Wembedded-directive
     -Wempty-translation-unit
     -Wenum-conversion
     -Wexit-time-destructors
     -Wexplicit-ownership-type
     -Wextern-initializer
     -Wextra
     -Wextra-semi
     -Wextra-tokens
     -Wflexible-array-extensions
     -Wfloat-conversion
     -Wformat-non-iso
     -Wfour-char-constants
     -Wgcc-compat
     -Wglobal-constructors
     -Wgnu-array-member-paren-init
     -Wgnu-designator
     -Wgnu-static-float-init
     -Wheader-guard
     -Wheader-hygiene
     -Widiomatic-parentheses
     -Wignored-attributes
     -Wimplicit-atomic-properties
     -Wimplicit-conversion-floating-point-to-bool
     -Wimplicit-exception-spec-mismatch
     -Wimplicit-fallthrough
     -Wimplicit-fallthrough-per-function
     -Wimplicit-retain-self
     -Wimport-preprocessor-directive-pedantic
     -Wincompatible-library-redeclaration
     -Wincompatible-pointer-types-discards-qualifiers
     -Wincomplete-implementation
     -Wincomplete-module
     -Wincomplete-umbrella
     -Winit-self
     -Wint-conversions
     -Wint-to-void-pointer-cast
     -Winteger-overflow
     -Winvalid-constexpr
     -Winvalid-iboutlet
     -Winvalid-noreturn
     -Winvalid-pp-token
     -Winvalid-source-encoding
     -Winvalid-token-paste
     -Wknr-promoted-parameter
     -Wlarge-by-value-copy
     -Wliteral-conversion
     -Wliteral-range
     -Wlocal-type-template-args
     -Wlogical-op
     -Wloop-analysis
     -Wmain-return-type
     -Wmalformed-warning-check
     -Wmethod-signatures
     -Wmicrosoft
     -Wmicrosoft-exists
     -Wmismatched-parameter-types
     -Wmismatched-return-types
     -Wmissing-field-initializers
     -Wmissing-format-attribute
     -Wmissing-noreturn
     -Wmissing-selector-name
     -Wmissing-sysroot
     -Wmissing-variable-declarations
     -Wmodule-conflict
     -Wnested-anon-types
     -Wnewline-eof
     -Wnon-literal-null-conversion
     -Wnon-pod-varargs
     -Wnonportable-cfstrings
     -Wnormalized=nfkc
     -Wnull-arithmetic
     -Wnull-character
     -Wnull-conversion
     -Wnull-dereference
     -Wout-of-line-declaration
     -Wover-aligned
     -Woverlength-strings
     -Woverride-init
     -Woverriding-method-mismatch
     -Wpointer-type-mismatch
     -Wpredefined-identifier-outside-function
     -Wprotocol-property-synthesis-ambiguity
     -Wreadonly-iboutlet-property
     -Wreadonly-setter-attrs
     -Wreceiver-expr
     -Wreceiver-forward-class
     -Wreceiver-is-weak
     -Wreinterpret-base-class
     -Wrequires-super-attribute
     -Wreserved-user-defined-literal
     -Wreturn-stack-address
     -Wsection
     -Wselector-type-mismatch
     -Wsentinel
     -Wserialized-diagnostics
     -Wshadow
     -Wshift-count-negative
     -Wshift-count-overflow
     -Wshift-negative-value
     -Wshift-overflow=2
     -Wshift-sign-overflow
     -Wshorten-64-to-32
     -Wsizeof-array-argument
     -Wsource-uses-openmp
     -Wstatic-float-init
     -Wstatic-in-inline
     -Wstatic-local-in-inline
     -Wstrict-overflow=1
     -Wstring-compare
     -Wstring-conversion
     -Wstrlcpy-strlcat-size
     -Wstrncat-size
     -Wsuggest-attribute=format
     -Wsuggest-attribute=noreturn
     -Wsuper-class-method-mismatch
     -Wswitch-bool
     -Wsync-nand
     -Wtautological-constant-out-of-range-compare
     -Wtentative-definition-incomplete-type
     -Wtrampolines
     -Wtype-safety
     -Wtypedef-redefinition
     -Wtypename-missing
     -Wundefined-inline
     -Wundefined-internal
     -Wundefined-reinterpret-cast
     -Wunicode
     -Wunicode-whitespace
     -Wunknown-warning-option
     -Wunnamed-type-template-args
     -Wunneeded-member-function
     -Wunsequenced
     -Wunsupported-visibility
     -Wunused-but-set-parameter
     -Wunused-but-set-variable
     -Wunused-command-line-argument
     -Wunused-const-variable=2
     -Wunused-exception-parameter
     -Wunused-local-typedefs
     -Wunused-member-function
     -Wunused-sanitize-argument
     -Wunused-volatile-lvalue
     -Wuser-defined-literals
     -Wvariadic-macros
     -Wvector-conversion
     -Wvector-conversions
     -Wvexing-parse
     -Wvisibility
     -Wvla-extension
     -Wzero-length-array
  ], [ TOR_TRY_COMPILE_WITH_CFLAGS(warning_flag, [],
              [TOR_WARNING_FLAGS="$TOR_WARNING_FLAGS warning_flag" CFLAGS="$CFLAGS warning_flag"], true)
     ])

dnl    We should re-enable this in some later version.  Clang doesn't
dnl    mind, but it causes trouble with GCC.
dnl     -Wstrict-overflow=2

dnl    These seem to require annotations that we don't currently use,
dnl    and they give false positives in our pthreads wrappers. (Clang 4)
dnl     -Wthread-safety
dnl     -Wthread-safety-analysis
dnl     -Wthread-safety-attributes
dnl     -Wthread-safety-beta
dnl     -Wthread-safety-precise

  W_FLAGS="$W_FLAGS -W -Wfloat-equal -Wundef -Wpointer-arith"
  W_FLAGS="$W_FLAGS -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings"
  W_FLAGS="$W_FLAGS -Wredundant-decls -Wchar-subscripts -Wcomment -Wformat=2"
  W_FLAGS="$W_FLAGS -Wwrite-strings"
  W_FLAGS="$W_FLAGS -Wnested-externs -Wbad-function-cast -Wswitch-enum"
  W_FLAGS="$W_FLAGS -Waggregate-return -Wpacked -Wunused"
  W_FLAGS="$W_FLAGS -Wunused-parameter "
  # These interfere with building main() { return 0; }, which autoconf
  # likes to use as its default program.
  W_FLAGS="$W_FLAGS -Wold-style-definition -Wmissing-declarations"

  TOR_WARNING_FLAGS="$TOR_WARNING_FLAGS $W_FLAGS"
  CFLAGS="$CFLAGS $W_FLAGS"

  if test "$tor_cv_cflags__Wnull_dereference" = "yes"; then
    AC_DEFINE([HAVE_CFLAG_WNULL_DEREFERENCE], 1, [True if we have -Wnull-dereference])
  fi
  if test "$tor_cv_cflags__Woverlength_strings" = "yes"; then
    AC_DEFINE([HAVE_CFLAG_WOVERLENGTH_STRINGS], 1, [True if we have -Woverlength-strings])
  fi
  if test "$tor_cv_cflags__warn_unused_const_variable_2" = "yes"; then
    AC_DEFINE([HAVE_CFLAG_WUNUSED_CONST_VARIABLE], 1, [True if we have -Wunused-const-variable])
  fi

  CFLAGS="$CFLAGS_NOWARNINGS"

  if test "x$enable_fatal_warnings" = "xyes"; then
    # I'd like to use TOR_CHECK_CFLAGS here, but I can't, since the
    # default autoconf programs are full of errors.
    CFLAGS="$CFLAGS -Werror"
  fi

fi

AC_SUBST(TOR_WARNING_FLAGS)

echo "$TOR_WARNING_FLAGS">warning_flags

TOR_TRY_COMPILE_WITH_CFLAGS([@warning_flags], [],
                            CFLAGS="$CFLAGS @warning_flags",
                            CFLAGS="$CFLAGS $TOR_WARNING_FLAGS")

if test "$enable_coverage" = "yes" && test "$have_clang" = "no"; then
   case "$host_os" in
    darwin*)
      AC_MSG_WARN([Tried to enable coverage on OSX without using the clang compiler. This might not work! If coverage fails, use CC=clang when configuring with --enable-coverage.])
   esac
fi

CPPFLAGS="$CPPFLAGS $TOR_CPPFLAGS_libevent $TOR_CPPFLAGS_openssl $TOR_CPPFLAGS_zlib"

AC_CONFIG_FILES([
	Doxyfile
	Makefile
	config.rust
	contrib/dist/suse/tor.sh
	contrib/operator-tools/tor.logrotate
	contrib/dist/tor.sh
	contrib/dist/torctl
	contrib/dist/tor.service
	src/config/torrc.sample
	src/config/torrc.minimal
	src/rust/.cargo/config
	scripts/maint/checkOptionDocs.pl
	scripts/maint/updateVersions.pl
        warning_flags
])

if test "x$asciidoc" = "xtrue" && test "$ASCIIDOC" = "none"; then
  regular_mans="doc/tor doc/tor-gencert doc/tor-resolve doc/torify"
  for file in $regular_mans ; do
    if ! [[ -f "$srcdir/$file.1.in" ]] || ! [[ -f "$srcdir/$file.html.in" ]] ; then
      echo "==================================";
      echo;
      echo "Building Tor has failed since manpages cannot be built.";
      echo;
      echo "You need asciidoc installed to be able to build the manpages.";
      echo "To build without manpages, use the --disable-asciidoc argument";
      echo "when calling configure.";
      echo;
      echo "==================================";
      exit 1;
    fi
  done
fi

if test "$fragile_hardening" = "yes"; then
  AC_MSG_WARN([

============
Warning!  Building Tor with --enable-fragile-hardening (also known as
--enable-expensive-hardening) makes some kinds of attacks harder, but makes
other kinds of attacks easier. A Tor instance build with this option will be
somewhat less vulnerable to remote code execution, arithmetic overflow, or
out-of-bounds read/writes... but at the cost of becoming more vulnerable to
denial of service attacks. For more information, see
https://trac.torproject.org/projects/tor/wiki/doc/TorFragileHardening
============
  ])
fi

AC_OUTPUT
