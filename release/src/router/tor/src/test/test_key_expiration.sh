#!/bin/sh

# Note: some of this code is lifted from zero_length_keys.sh and
# test_keygen.sh, and could be unified.

umask 077
set -e

if [ $# -eq 0 ] || [ ! -f ${1} ] || [ ! -x ${1} ]; then
  if [ "$TESTING_TOR_BINARY" = "" ] ; then
    echo "Usage: ${0} PATH_TO_TOR [case-number]"
    exit 1
  fi
fi

UNAME_OS=`uname -s | cut -d_ -f1`
if test "$UNAME_OS" = 'CYGWIN' || \
   test "$UNAME_OS" = 'MSYS' || \
   test "$UNAME_OS" = 'MINGW'; then
  echo "This test is unreliable on Windows. See trac #26076. Skipping." >&2
  exit 77
fi

if [ $# -ge 1 ]; then
  TOR_BINARY="${1}"
  shift
else
  TOR_BINARY="${TESTING_TOR_BINARY}"
fi

if [ $# -ge 1 ]; then
  dflt=0
else
  dflt=1
fi

CASE1=$dflt
CASE2=$dflt
CASE3=$dflt

if [ $# -ge 1 ]; then
  eval "CASE${1}"=1
fi


dump() { xxd -p "$1" | tr -d '\n '; }
die() { echo "$1" >&2 ; exit 5; }
check_dir() { [ -d "$1" ] || die "$1 did not exist"; }
check_file() { [ -e "$1" ] || die "$1 did not exist"; }
check_no_file() { [ -e "$1" ] && die "$1 was not supposed to exist" || true; }
check_files_eq() { cmp "$1" "$2" || die "$1 and $2 did not match: `dump $1` vs `dump $2`"; }
check_keys_eq() { check_files_eq "${SRC}/keys/${1}" "${ME}/keys/${1}"; }

DATA_DIR=`mktemp -d -t tor_key_expiration_tests.XXXXXX`
if [ -z "$DATA_DIR" ]; then
  echo "Failure: mktemp invocation returned empty string" >&2
  exit 3
fi
if [ ! -d "$DATA_DIR" ]; then
  echo "Failure: mktemp invocation result doesn't point to directory" >&2
  exit 3
fi
trap "rm -rf '$DATA_DIR'" 0

# Use an absolute path for this or Tor will complain
DATA_DIR=`cd "${DATA_DIR}" && pwd`

touch "${DATA_DIR}/empty_torrc"
touch "${DATA_DIR}/empty_defaults_torrc"

QUIETLY="--hush"
SILENTLY="--quiet"
TOR="${TOR_BINARY} --DisableNetwork 1 --ShutdownWaitLength 0 --ORPort 12345 --ExitRelay 0 --DataDirectory ${DATA_DIR} -f ${DATA_DIR}/empty_torrc --defaults-torrc ${DATA_DIR}/empty_defaults_torrc"

##### SETUP
#
# Here we create a set of keys.

# Step 1: Start Tor with --list-fingerprint --quiet.  Make sure everything is there.
echo "Setup step #1"
${TOR} --list-fingerprint ${SILENTLY} > /dev/null

check_dir "${DATA_DIR}/keys"
check_file "${DATA_DIR}/keys/ed25519_master_id_public_key"
check_file "${DATA_DIR}/keys/ed25519_master_id_secret_key"
check_file "${DATA_DIR}/keys/ed25519_signing_cert"
check_file "${DATA_DIR}/keys/ed25519_signing_secret_key"
check_file "${DATA_DIR}/keys/secret_id_key"
check_file "${DATA_DIR}/keys/secret_onion_key"
check_file "${DATA_DIR}/keys/secret_onion_key_ntor"

##### TEST CASES

echo "=== Starting key expiration tests."

FN="${DATA_DIR}/stderr"

if [ "$CASE1" = 1 ]; then
  echo "==== Case 1: Test --key-expiration without argument and ensure usage"
  echo "             instructions are printed."

  ${TOR} ${QUIETLY} --key-expiration 2>"$FN" || true
  grep "No valid argument to --key-expiration found!" "$FN" >/dev/null || \
    die "Tor didn't mention supported --key-expiration argmuents"

  echo "==== Case 1: ok"
fi

if [ "$CASE2" = 1 ]; then
  echo "==== Case 2: Start Tor with --key-expiration 'sign' and make sure it prints an expiration."

  ${TOR} ${QUIETLY} --key-expiration sign 2>"$FN"
  grep "signing-cert-expiry:" "$FN" >/dev/null || \
    die "Tor didn't print an expiration"

  echo "==== Case 2: ok"
fi

if [ "$CASE3" = 1 ]; then
  echo "==== Case 3: Start Tor with --key-expiration 'sign', when there is no"
  echo "             signing key, and make sure that Tor generates a new key"
  echo "             and prints its certificate's expiration."

  mv "${DATA_DIR}/keys/ed25519_signing_cert" \
     "${DATA_DIR}/keys/ed25519_signing_cert.bak"

  ${TOR} --key-expiration sign > "$FN" 2>&1
  grep "It looks like I need to generate and sign a new medium-term signing key" "$FN" >/dev/null || \
    die "Tor didn't create a new signing key"
  check_file "${DATA_DIR}/keys/ed25519_signing_cert"
  grep "signing-cert-expiry:" "$FN" >/dev/null || \
    die "Tor didn't print an expiration"

  mv "${DATA_DIR}/keys/ed25519_signing_cert.bak" \
     "${DATA_DIR}/keys/ed25519_signing_cert"

  echo "==== Case 3: ok"
fi
