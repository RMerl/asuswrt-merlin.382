#!/bin/sh

set -x

UNAME_OS=$(uname -s | cut -d_ -f1)
if test "$UNAME_OS" = 'CYGWIN' || \
   test "$UNAME_OS" = 'MSYS' || \
   test "$UNAME_OS" = 'MINGW'; then
  if test "$APPVEYOR" = 'True'; then
    echo "This test is disabled on Windows CI, as it requires firewall examptions. Skipping." >&2
    exit 77
  fi
fi

exitcode=0

tmpdir=
clean () { test -n "$tmpdir" && test -d "$tmpdir" && rm -rf "$tmpdir" || :; }
trap clean EXIT HUP INT TERM

tmpdir="`mktemp -d -t tor_rebind_test.XXXXXX`"
if [ -z "$tmpdir" ]; then
  echo >&2 mktemp failed
  exit 2
elif [ ! -d "$tmpdir" ]; then
  echo >&2 mktemp failed to make a directory
  exit 3
fi

"${PYTHON:-python}" "${abs_top_srcdir:-.}/src/test/test_rebind.py" "${TESTING_TOR_BINARY}" "$tmpdir"

exit $?
