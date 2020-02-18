/* Copyright (c) 2003, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file tortls.c
 * \brief Wrapper functions to present a consistent interface to
 * TLS, SSL, and X.509 functions from OpenSSL.
 **/

/* (Unlike other tor functions, these
 * are prefixed with tor_ in order to avoid conflicting with OpenSSL
 * functions and variables.)
 */

#include "orconfig.h"

#define TORTLS_PRIVATE
#define TORTLS_OPENSSL_PRIVATE
#define TOR_X509_PRIVATE

#ifdef _WIN32
  /* We need to include these here, or else the dtls1.h header will include
   * <winsock.h> and mess things up, in at least some openssl versions. */
  #include <winsock2.h>
  #include <ws2tcpip.h>
#endif

#include "lib/crypt_ops/crypto_cipher.h"
#include "lib/crypt_ops/crypto_rand.h"
#include "lib/crypt_ops/crypto_dh.h"
#include "lib/crypt_ops/crypto_util.h"
#include "lib/crypt_ops/compat_openssl.h"
#include "lib/tls/x509.h"
#include "lib/tls/x509_internal.h"

/* Some versions of OpenSSL declare SSL_get_selected_srtp_profile twice in
 * srtp.h. Suppress the GCC warning so we can build with -Wredundant-decl. */
DISABLE_GCC_WARNING(redundant-decls)

#include <openssl/opensslv.h>

#ifdef OPENSSL_NO_EC
#error "We require OpenSSL with ECC support"
#endif

#include <openssl/ssl.h>
#include <openssl/ssl3.h>
#include <openssl/err.h>
#include <openssl/tls1.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>

ENABLE_GCC_WARNING(redundant-decls)

#include "lib/tls/tortls.h"
#include "lib/tls/tortls_st.h"
#include "lib/tls/tortls_internal.h"
#include "lib/log/log.h"
#include "lib/log/util_bug.h"
#include "lib/container/smartlist.h"
#include "lib/string/compat_string.h"
#include "lib/string/printf.h"
#include "lib/net/socket.h"
#include "lib/intmath/cmp.h"
#include "lib/ctime/di_ops.h"
#include "lib/encoding/time_fmt.h"

#include <stdlib.h>
#include <string.h>

#include "lib/arch/bytes.h"

/* Copied from or.h */
#define LEGAL_NICKNAME_CHARACTERS \
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

#define ADDR(tls) (((tls) && (tls)->address) ? tls->address : "peer")

#if OPENSSL_VERSION_NUMBER <  OPENSSL_V(1,0,0,'f')
/* This is a version of OpenSSL before 1.0.0f. It does not have
 * the CVE-2011-4576 fix, and as such it can't use RELEASE_BUFFERS and
 * SSL3 safely at the same time.
 */
#define DISABLE_SSL3_HANDSHAKE
#endif /* OPENSSL_VERSION_NUMBER <  OPENSSL_V(1,0,0,'f') */

/* We redefine these so that we can run correctly even if the vendor gives us
 * a version of OpenSSL that does not match its header files.  (Apple: I am
 * looking at you.)
 */
#ifndef SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
#define SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION 0x00040000L
#endif
#ifndef SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
#define SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION 0x0010
#endif

/** Set to true iff openssl bug 7712 has been detected. */
static int openssl_bug_7712_is_present = 0;

/** Return values for tor_tls_classify_client_ciphers.
 *
 * @{
 */
/** An error occurred when examining the client ciphers */
#define CIPHERS_ERR -1
/** The client cipher list indicates that a v1 handshake was in use. */
#define CIPHERS_V1 1
/** The client cipher list indicates that the client is using the v2 or the
 * v3 handshake, but that it is (probably!) lying about what ciphers it
 * supports */
#define CIPHERS_V2 2
/** The client cipher list indicates that the client is using the v2 or the
 * v3 handshake, and that it is telling the truth about what ciphers it
 * supports */
#define CIPHERS_UNRESTRICTED 3
/** @} */

/** The ex_data index in which we store a pointer to an SSL object's
 * corresponding tor_tls_t object. */
STATIC int tor_tls_object_ex_data_index = -1;

/** Helper: Allocate tor_tls_object_ex_data_index. */
void
tor_tls_allocate_tor_tls_object_ex_data_index(void)
{
  if (tor_tls_object_ex_data_index == -1) {
    tor_tls_object_ex_data_index =
      SSL_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    tor_assert(tor_tls_object_ex_data_index != -1);
  }
}

/** Helper: given a SSL* pointer, return the tor_tls_t object using that
 * pointer. */
tor_tls_t *
tor_tls_get_by_ssl(const SSL *ssl)
{
  tor_tls_t *result = SSL_get_ex_data(ssl, tor_tls_object_ex_data_index);
  if (result)
    tor_assert(result->magic == TOR_TLS_MAGIC);
  return result;
}

/** True iff tor_tls_init() has been called. */
static int tls_library_is_initialized = 0;

/* Module-internal error codes. */
#define TOR_TLS_SYSCALL_    (MIN_TOR_TLS_ERROR_VAL_ - 2)
#define TOR_TLS_ZERORETURN_ (MIN_TOR_TLS_ERROR_VAL_ - 1)

/** Write a description of the current state of <b>tls</b> into the
 * <b>sz</b>-byte buffer at <b>buf</b>. */
void
tor_tls_get_state_description(tor_tls_t *tls, char *buf, size_t sz)
{
  const char *ssl_state;
  const char *tortls_state;

  if (PREDICT_UNLIKELY(!tls || !tls->ssl)) {
    strlcpy(buf, "(No SSL object)", sz);
    return;
  }

  ssl_state = SSL_state_string_long(tls->ssl);
  switch (tls->state) {
#define CASE(st) case TOR_TLS_ST_##st: tortls_state = " in "#st ; break
    CASE(HANDSHAKE);
    CASE(OPEN);
    CASE(GOTCLOSE);
    CASE(SENTCLOSE);
    CASE(CLOSED);
    CASE(RENEGOTIATE);
#undef CASE
  case TOR_TLS_ST_BUFFEREVENT:
    tortls_state = "";
    break;
  default:
    tortls_state = " in unknown TLS state";
    break;
  }

  tor_snprintf(buf, sz, "%s%s", ssl_state, tortls_state);
}

/** Log a single error <b>err</b> as returned by ERR_get_error(), which was
 * received while performing an operation <b>doing</b> on <b>tls</b>.  Log
 * the message at <b>severity</b>, in log domain <b>domain</b>. */
void
tor_tls_log_one_error(tor_tls_t *tls, unsigned long err,
                  int severity, int domain, const char *doing)
{
  const char *state = NULL, *addr;
  const char *msg, *lib, *func;

  state = (tls && tls->ssl)?SSL_state_string_long(tls->ssl):"---";

  addr = tls ? tls->address : NULL;

  /* Some errors are known-benign, meaning they are the fault of the other
   * side of the connection. The caller doesn't know this, so override the
   * priority for those cases. */
  switch (ERR_GET_REASON(err)) {
    case SSL_R_HTTP_REQUEST:
    case SSL_R_HTTPS_PROXY_REQUEST:
    case SSL_R_RECORD_LENGTH_MISMATCH:
#ifndef OPENSSL_1_1_API
    case SSL_R_RECORD_TOO_LARGE:
#endif
    case SSL_R_UNKNOWN_PROTOCOL:
    case SSL_R_UNSUPPORTED_PROTOCOL:
      severity = LOG_INFO;
      break;
    default:
      break;
  }

  msg = (const char*)ERR_reason_error_string(err);
  lib = (const char*)ERR_lib_error_string(err);
  func = (const char*)ERR_func_error_string(err);
  if (!msg) msg = "(null)";
  if (!lib) lib = "(null)";
  if (!func) func = "(null)";
  if (doing) {
    tor_log(severity, domain, "TLS error while %s%s%s: %s (in %s:%s:%s)",
        doing, addr?" with ":"", addr?addr:"",
        msg, lib, func, state);
  } else {
    tor_log(severity, domain, "TLS error%s%s: %s (in %s:%s:%s)",
        addr?" with ":"", addr?addr:"",
        msg, lib, func, state);
  }
}

/** Log all pending tls errors at level <b>severity</b> in log domain
 * <b>domain</b>.  Use <b>doing</b> to describe our current activities.
 */
void
tls_log_errors(tor_tls_t *tls, int severity, int domain, const char *doing)
{
  unsigned long err;

  while ((err = ERR_get_error()) != 0) {
    tor_tls_log_one_error(tls, err, severity, domain, doing);
  }
}

#define CATCH_SYSCALL 1
#define CATCH_ZERO    2

/** Given a TLS object and the result of an SSL_* call, use
 * SSL_get_error to determine whether an error has occurred, and if so
 * which one.  Return one of TOR_TLS_{DONE|WANTREAD|WANTWRITE|ERROR}.
 * If extra&CATCH_SYSCALL is true, return TOR_TLS_SYSCALL_ instead of
 * reporting syscall errors.  If extra&CATCH_ZERO is true, return
 * TOR_TLS_ZERORETURN_ instead of reporting zero-return errors.
 *
 * If an error has occurred, log it at level <b>severity</b> and describe the
 * current action as <b>doing</b>.
 */
int
tor_tls_get_error(tor_tls_t *tls, int r, int extra,
                  const char *doing, int severity, int domain)
{
  int err = SSL_get_error(tls->ssl, r);
  int tor_error = TOR_TLS_ERROR_MISC;
  switch (err) {
    case SSL_ERROR_NONE:
      return TOR_TLS_DONE;
    case SSL_ERROR_WANT_READ:
      return TOR_TLS_WANTREAD;
    case SSL_ERROR_WANT_WRITE:
      return TOR_TLS_WANTWRITE;
    case SSL_ERROR_SYSCALL:
      if (extra&CATCH_SYSCALL)
        return TOR_TLS_SYSCALL_;
      if (r == 0) {
        tor_log(severity, LD_NET, "TLS error: unexpected close while %s (%s)",
            doing, SSL_state_string_long(tls->ssl));
        tor_error = TOR_TLS_ERROR_IO;
      } else {
        int e = tor_socket_errno(tls->socket);
        tor_log(severity, LD_NET,
            "TLS error: <syscall error while %s> (errno=%d: %s; state=%s)",
            doing, e, tor_socket_strerror(e),
            SSL_state_string_long(tls->ssl));
        tor_error = tor_errno_to_tls_error(e);
      }
      tls_log_errors(tls, severity, domain, doing);
      return tor_error;
    case SSL_ERROR_ZERO_RETURN:
      if (extra&CATCH_ZERO)
        return TOR_TLS_ZERORETURN_;
      tor_log(severity, LD_NET, "TLS connection closed while %s in state %s",
          doing, SSL_state_string_long(tls->ssl));
      tls_log_errors(tls, severity, domain, doing);
      return TOR_TLS_CLOSE;
    default:
      tls_log_errors(tls, severity, domain, doing);
      return TOR_TLS_ERROR_MISC;
  }
}

/** Initialize OpenSSL, unless it has already been initialized.
 */
void
tor_tls_init(void)
{
  check_no_tls_errors();

  if (!tls_library_is_initialized) {
#ifdef OPENSSL_1_1_API
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS, NULL);
#else
    SSL_library_init();
    SSL_load_error_strings();
#endif

#if (SIZEOF_VOID_P >= 8 &&                              \
     OPENSSL_VERSION_NUMBER >= OPENSSL_V_SERIES(1,0,1))
    long version = OpenSSL_version_num();

    /* LCOV_EXCL_START : we can't test these lines on the same machine */
    if (version >= OPENSSL_V_SERIES(1,0,1)) {
      /* Warn if we could *almost* be running with much faster ECDH.
         If we're built for a 64-bit target, using OpenSSL 1.0.1, but we
         don't have one of the built-in __uint128-based speedups, we are
         just one build operation away from an accelerated handshake.

         (We could be looking at OPENSSL_NO_EC_NISTP_64_GCC_128 instead of
          doing this test, but that gives compile-time options, not runtime
          behavior.)
      */
      EC_KEY *key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
      const EC_GROUP *g = key ? EC_KEY_get0_group(key) : NULL;
      const EC_METHOD *m = g ? EC_GROUP_method_of(g) : NULL;
      const int warn = (m == EC_GFp_simple_method() ||
                        m == EC_GFp_mont_method() ||
                        m == EC_GFp_nist_method());
      EC_KEY_free(key);

      if (warn)
        log_notice(LD_GENERAL, "We were built to run on a 64-bit CPU, with "
                   "OpenSSL 1.0.1 or later, but with a version of OpenSSL "
                   "that apparently lacks accelerated support for the NIST "
                   "P-224 and P-256 groups. Building openssl with such "
                   "support (using the enable-ec_nistp_64_gcc_128 option "
                   "when configuring it) would make ECDH much faster.");
    }
    /* LCOV_EXCL_STOP */
#endif /* (SIZEOF_VOID_P >= 8 &&                              ... */

    tor_tls_allocate_tor_tls_object_ex_data_index();

    tls_library_is_initialized = 1;
  }
}

/** We need to give OpenSSL a callback to verify certificates. This is
 * it: We always accept peer certs and complete the handshake.  We
 * don't validate them until later.
 */
int
always_accept_verify_cb(int preverify_ok,
                        X509_STORE_CTX *x509_ctx)
{
  (void) preverify_ok;
  (void) x509_ctx;
  return 1;
}

/** List of ciphers that servers should select from when the client might be
 * claiming extra unsupported ciphers in order to avoid fingerprinting.  */
static const char SERVER_CIPHER_LIST[] =
#ifdef  TLS1_3_TXT_AES_128_GCM_SHA256
  /* This one can never actually get selected, since if the client lists it,
   * we will assume that the client is honest, and not use this list.
   * Nonetheless we list it if it's available, so that the server doesn't
   * conclude that it has no valid ciphers if it's running with TLS1.3.
   */
  TLS1_3_TXT_AES_128_GCM_SHA256 ":"
#endif
  TLS1_TXT_DHE_RSA_WITH_AES_256_SHA ":"
  TLS1_TXT_DHE_RSA_WITH_AES_128_SHA;

/** List of ciphers that servers should select from when we actually have
 * our choice of what cipher to use. */
static const char UNRESTRICTED_SERVER_CIPHER_LIST[] =
  /* Here are the TLS 1.3 ciphers we like, in the order we prefer. */
#ifdef TLS1_3_TXT_AES_256_GCM_SHA384
  TLS1_3_TXT_AES_256_GCM_SHA384 ":"
#endif
#ifdef TLS1_3_TXT_CHACHA20_POLY1305_SHA256
  TLS1_3_TXT_CHACHA20_POLY1305_SHA256 ":"
#endif
#ifdef TLS1_3_TXT_AES_128_GCM_SHA256
  TLS1_3_TXT_AES_128_GCM_SHA256 ":"
#endif
#ifdef TLS1_3_TXT_AES_128_CCM_SHA256
  TLS1_3_TXT_AES_128_CCM_SHA256 ":"
#endif

  /* This list is autogenerated with the gen_server_ciphers.py script;
   * don't hand-edit it. */
#ifdef TLS1_TXT_ECDHE_RSA_WITH_AES_256_GCM_SHA384
       TLS1_TXT_ECDHE_RSA_WITH_AES_256_GCM_SHA384 ":"
#endif
#ifdef TLS1_TXT_ECDHE_RSA_WITH_AES_128_GCM_SHA256
       TLS1_TXT_ECDHE_RSA_WITH_AES_128_GCM_SHA256 ":"
#endif
#ifdef TLS1_TXT_ECDHE_RSA_WITH_AES_256_SHA384
       TLS1_TXT_ECDHE_RSA_WITH_AES_256_SHA384 ":"
#endif
#ifdef TLS1_TXT_ECDHE_RSA_WITH_AES_128_SHA256
       TLS1_TXT_ECDHE_RSA_WITH_AES_128_SHA256 ":"
#endif
#ifdef TLS1_TXT_ECDHE_RSA_WITH_AES_256_CBC_SHA
       TLS1_TXT_ECDHE_RSA_WITH_AES_256_CBC_SHA ":"
#endif
#ifdef TLS1_TXT_ECDHE_RSA_WITH_AES_128_CBC_SHA
       TLS1_TXT_ECDHE_RSA_WITH_AES_128_CBC_SHA ":"
#endif
#ifdef TLS1_TXT_DHE_RSA_WITH_AES_256_GCM_SHA384
       TLS1_TXT_DHE_RSA_WITH_AES_256_GCM_SHA384 ":"
#endif
#ifdef TLS1_TXT_DHE_RSA_WITH_AES_128_GCM_SHA256
       TLS1_TXT_DHE_RSA_WITH_AES_128_GCM_SHA256 ":"
#endif
#ifdef TLS1_TXT_DHE_RSA_WITH_AES_256_CCM
       TLS1_TXT_DHE_RSA_WITH_AES_256_CCM ":"
#endif
#ifdef TLS1_TXT_DHE_RSA_WITH_AES_128_CCM
       TLS1_TXT_DHE_RSA_WITH_AES_128_CCM ":"
#endif
#ifdef TLS1_TXT_DHE_RSA_WITH_AES_256_SHA256
       TLS1_TXT_DHE_RSA_WITH_AES_256_SHA256 ":"
#endif
#ifdef TLS1_TXT_DHE_RSA_WITH_AES_128_SHA256
       TLS1_TXT_DHE_RSA_WITH_AES_128_SHA256 ":"
#endif
       /* Required */
       TLS1_TXT_DHE_RSA_WITH_AES_256_SHA ":"
       /* Required */
       TLS1_TXT_DHE_RSA_WITH_AES_128_SHA ":"
#ifdef TLS1_TXT_ECDHE_RSA_WITH_CHACHA20_POLY1305
       TLS1_TXT_ECDHE_RSA_WITH_CHACHA20_POLY1305 ":"
#endif
#ifdef TLS1_TXT_DHE_RSA_WITH_CHACHA20_POLY1305
       TLS1_TXT_DHE_RSA_WITH_CHACHA20_POLY1305
#endif
  ;

/* Note: to set up your own private testing network with link crypto
 * disabled, set your Tors' cipher list to
 * (SSL3_TXT_RSA_NULL_SHA).  If you do this, you won't be able to communicate
 * with any of the "real" Tors, though. */

#define CIPHER(id, name) name ":"
#define XCIPHER(id, name)
/** List of ciphers that clients should advertise, omitting items that
 * our OpenSSL doesn't know about. */
static const char CLIENT_CIPHER_LIST[] =
#include "ciphers.inc"
  /* Tell it not to use SSLv2 ciphers, so that it can select an SSLv3 version
   * of any cipher we say. */
  "!SSLv2"
  ;
#undef CIPHER
#undef XCIPHER

/** Return true iff the other side of <b>tls</b> has authenticated to us, and
 * the key certified in <b>cert</b> is the same as the key they used to do it.
 */
MOCK_IMPL(int,
tor_tls_cert_matches_key,(const tor_tls_t *tls, const tor_x509_cert_t *cert))
{
  tor_x509_cert_t *peer = tor_tls_get_peer_cert((tor_tls_t *)tls);
  if (!peer)
    return 0;

  X509 *peercert = peer->cert;
  EVP_PKEY *link_key = NULL, *cert_key = NULL;
  int result;

  link_key = X509_get_pubkey(peercert);
  cert_key = X509_get_pubkey(cert->cert);

  result = link_key && cert_key && EVP_PKEY_cmp(cert_key, link_key) == 1;

  tor_x509_cert_free(peer);
  if (link_key)
    EVP_PKEY_free(link_key);
  if (cert_key)
    EVP_PKEY_free(cert_key);

  return result;
}

void
tor_tls_context_impl_free_(struct ssl_ctx_st *ctx)
{
  if (!ctx)
    return;
  SSL_CTX_free(ctx);
}

/** The group we should use for ecdhe when none was selected. */
#define  NID_tor_default_ecdhe_group NID_X9_62_prime256v1

/** Create a new TLS context for use with Tor TLS handshakes.
 * <b>identity</b> should be set to the identity key used to sign the
 * certificate.
 */
tor_tls_context_t *
tor_tls_context_new(crypto_pk_t *identity, unsigned int key_lifetime,
                    unsigned flags, int is_client)
{
  EVP_PKEY *pkey = NULL;
  tor_tls_context_t *result = NULL;

  tor_tls_init();

  result = tor_malloc_zero(sizeof(tor_tls_context_t));
  result->refcnt = 1;

  if (! is_client) {
    if (tor_tls_context_init_certificates(result, identity, key_lifetime,
                                          flags) < 0) {
      goto error;
    }
  }

#if 0
  /* Tell OpenSSL to only use TLS1.  This may have subtly different results
   * from SSLv23_method() with SSLv2 and SSLv3 disabled, so we need to do some
   * investigation before we consider adjusting it. It should be compatible
   * with existing Tors. */
  if (!(result->ctx = SSL_CTX_new(TLSv1_method())))
    goto error;
#endif /* 0 */

  /* Tell OpenSSL to use TLS 1.0 or later but not SSL2 or SSL3. */
#ifdef HAVE_TLS_METHOD
  if (!(result->ctx = SSL_CTX_new(TLS_method())))
    goto error;
#else
  if (!(result->ctx = SSL_CTX_new(SSLv23_method())))
    goto error;
#endif /* defined(HAVE_TLS_METHOD) */

#ifdef HAVE_SSL_CTX_SET_SECURITY_LEVEL
  /* Level 1 re-enables RSA1024 and DH1024 for compatibility with old tors */
  SSL_CTX_set_security_level(result->ctx, 1);
#endif

  SSL_CTX_set_options(result->ctx, SSL_OP_NO_SSLv2);
  SSL_CTX_set_options(result->ctx, SSL_OP_NO_SSLv3);

  /* Prefer the server's ordering of ciphers: the client's ordering has
  * historically been chosen for fingerprinting resistance. */
  SSL_CTX_set_options(result->ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);

  /* Disable TLS tickets if they're supported.  We never want to use them;
   * using them can make our perfect forward secrecy a little worse, *and*
   * create an opportunity to fingerprint us (since it's unusual to use them
   * with TLS sessions turned off).
   *
   * In 0.2.4, clients advertise support for them though, to avoid a TLS
   * distinguishability vector.  This can give us worse PFS, though, if we
   * get a server that doesn't set SSL_OP_NO_TICKET.  With luck, there will
   * be few such servers by the time 0.2.4 is more stable.
   */
#ifdef SSL_OP_NO_TICKET
  if (! is_client) {
    SSL_CTX_set_options(result->ctx, SSL_OP_NO_TICKET);
  }
#endif

  SSL_CTX_set_options(result->ctx, SSL_OP_SINGLE_DH_USE);
  SSL_CTX_set_options(result->ctx, SSL_OP_SINGLE_ECDH_USE);

#ifdef SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
  SSL_CTX_set_options(result->ctx,
                      SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
#endif
  /* Yes, we know what we are doing here.  No, we do not treat a renegotiation
   * as authenticating any earlier-received data.
   */
  {
    SSL_CTX_set_options(result->ctx,
                        SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION);
  }

  /* Don't actually allow compression; it uses RAM and time, it makes TLS
   * vulnerable to CRIME-style attacks, and most of the data we transmit over
   * TLS is encrypted (and therefore uncompressible) anyway. */
#ifdef SSL_OP_NO_COMPRESSION
  SSL_CTX_set_options(result->ctx, SSL_OP_NO_COMPRESSION);
#endif
#if OPENSSL_VERSION_NUMBER < OPENSSL_V_SERIES(1,1,0)
#ifndef OPENSSL_NO_COMP
  if (result->ctx->comp_methods)
    result->ctx->comp_methods = NULL;
#endif
#endif /* OPENSSL_VERSION_NUMBER < OPENSSL_V_SERIES(1,1,0) */

#ifdef SSL_MODE_RELEASE_BUFFERS
  SSL_CTX_set_mode(result->ctx, SSL_MODE_RELEASE_BUFFERS);
#endif
  if (! is_client) {
    if (result->my_link_cert &&
        !SSL_CTX_use_certificate(result->ctx,
                                 result->my_link_cert->cert)) {
      goto error;
    }
    if (result->my_id_cert) {
      X509_STORE *s = SSL_CTX_get_cert_store(result->ctx);
      tor_assert(s);
      X509_STORE_add_cert(s, result->my_id_cert->cert);
    }
  }
  SSL_CTX_set_session_cache_mode(result->ctx, SSL_SESS_CACHE_OFF);
  if (!is_client) {
    tor_assert(result->link_key);
    if (!(pkey = crypto_pk_get_openssl_evp_pkey_(result->link_key,1)))
      goto error;
    if (!SSL_CTX_use_PrivateKey(result->ctx, pkey))
      goto error;
    EVP_PKEY_free(pkey);
    pkey = NULL;
    if (!SSL_CTX_check_private_key(result->ctx))
      goto error;
  }

  {
    DH *dh = crypto_dh_new_openssl_tls();
    tor_assert(dh);
    SSL_CTX_set_tmp_dh(result->ctx, dh);
    DH_free(dh);
  }
/* We check for this function in two ways, since it might be either a symbol
 * or a macro. */
#if defined(SSL_CTX_set1_groups_list) || defined(HAVE_SSL_CTX_SET1_GROUPS_LIST)
  {
    const char *list;
    if (flags & TOR_TLS_CTX_USE_ECDHE_P224)
      list = "P-224:P-256";
    else if (flags & TOR_TLS_CTX_USE_ECDHE_P256)
      list = "P-256:P-224";
    else
      list = "P-256:P-224";
    int r = (int) SSL_CTX_set1_groups_list(result->ctx, list);
    if (r < 0)
      goto error;
  }
#else
  if (! is_client) {
    int nid;
    EC_KEY *ec_key;
    if (flags & TOR_TLS_CTX_USE_ECDHE_P224)
      nid = NID_secp224r1;
    else if (flags & TOR_TLS_CTX_USE_ECDHE_P256)
      nid = NID_X9_62_prime256v1;
    else
      nid = NID_tor_default_ecdhe_group;
    /* Use P-256 for ECDHE. */
    ec_key = EC_KEY_new_by_curve_name(nid);
    if (ec_key != NULL) /*XXXX Handle errors? */
      SSL_CTX_set_tmp_ecdh(result->ctx, ec_key);
    EC_KEY_free(ec_key);
  }
#endif
  SSL_CTX_set_verify(result->ctx, SSL_VERIFY_PEER,
                     always_accept_verify_cb);
  /* let us realloc bufs that we're writing from */
  SSL_CTX_set_mode(result->ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

  return result;

 error:
  tls_log_errors(NULL, LOG_WARN, LD_NET, "creating TLS context");
  if (pkey)
    EVP_PKEY_free(pkey);
  tor_tls_context_decref(result);
  return NULL;
}

/** Invoked when a TLS state changes: log the change at severity 'debug' */
void
tor_tls_debug_state_callback(const SSL *ssl, int type, int val)
{
  /* LCOV_EXCL_START since this depends on whether debug is captured or not */
  log_debug(LD_HANDSHAKE, "SSL %p is now in state %s [type=%d,val=%d].",
            ssl, SSL_state_string_long(ssl), type, val);
  /* LCOV_EXCL_STOP */
}

/* Return the name of the negotiated ciphersuite in use on <b>tls</b> */
const char *
tor_tls_get_ciphersuite_name(tor_tls_t *tls)
{
  return SSL_get_cipher(tls->ssl);
}

/* Here's the old V2 cipher list we sent from 0.2.1.1-alpha up to
 * 0.2.3.17-beta. If a client is using this list, we can't believe the ciphers
 * that it claims to support.  We'll prune this list to remove the ciphers
 * *we* don't recognize. */
STATIC uint16_t v2_cipher_list[] = {
  0xc00a, /* TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_CBC_SHA */
  0xc014, /* TLS1_TXT_ECDHE_RSA_WITH_AES_256_CBC_SHA */
  0x0039, /* TLS1_TXT_DHE_RSA_WITH_AES_256_SHA */
  0x0038, /* TLS1_TXT_DHE_DSS_WITH_AES_256_SHA */
  0xc00f, /* TLS1_TXT_ECDH_RSA_WITH_AES_256_CBC_SHA */
  0xc005, /* TLS1_TXT_ECDH_ECDSA_WITH_AES_256_CBC_SHA */
  0x0035, /* TLS1_TXT_RSA_WITH_AES_256_SHA */
  0xc007, /* TLS1_TXT_ECDHE_ECDSA_WITH_RC4_128_SHA */
  0xc009, /* TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_CBC_SHA */
  0xc011, /* TLS1_TXT_ECDHE_RSA_WITH_RC4_128_SHA */
  0xc013, /* TLS1_TXT_ECDHE_RSA_WITH_AES_128_CBC_SHA */
  0x0033, /* TLS1_TXT_DHE_RSA_WITH_AES_128_SHA */
  0x0032, /* TLS1_TXT_DHE_DSS_WITH_AES_128_SHA */
  0xc00c, /* TLS1_TXT_ECDH_RSA_WITH_RC4_128_SHA */
  0xc00e, /* TLS1_TXT_ECDH_RSA_WITH_AES_128_CBC_SHA */
  0xc002, /* TLS1_TXT_ECDH_ECDSA_WITH_RC4_128_SHA */
  0xc004, /* TLS1_TXT_ECDH_ECDSA_WITH_AES_128_CBC_SHA */
  0x0004, /* SSL3_TXT_RSA_RC4_128_MD5 */
  0x0005, /* SSL3_TXT_RSA_RC4_128_SHA */
  0x002f, /* TLS1_TXT_RSA_WITH_AES_128_SHA */
  0xc008, /* TLS1_TXT_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA */
  0xc012, /* TLS1_TXT_ECDHE_RSA_WITH_DES_192_CBC3_SHA */
  0x0016, /* SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA */
  0x0013, /* SSL3_TXT_EDH_DSS_DES_192_CBC3_SHA */
  0xc00d, /* TLS1_TXT_ECDH_RSA_WITH_DES_192_CBC3_SHA */
  0xc003, /* TLS1_TXT_ECDH_ECDSA_WITH_DES_192_CBC3_SHA */
  0xfeff, /* SSL3_TXT_RSA_FIPS_WITH_3DES_EDE_CBC_SHA */
  0x000a, /* SSL3_TXT_RSA_DES_192_CBC3_SHA */
  0
};
/** Have we removed the unrecognized ciphers from v2_cipher_list yet? */
static int v2_cipher_list_pruned = 0;

/** Return 0 if <b>m</b> does not support the cipher with ID <b>cipher</b>;
 * return 1 if it does support it, or if we have no way to tell. */
int
find_cipher_by_id(const SSL *ssl, const SSL_METHOD *m, uint16_t cipher)
{
  const SSL_CIPHER *c;
#ifdef HAVE_SSL_CIPHER_FIND
  (void) m;
  {
    unsigned char cipherid[3];
    tor_assert(ssl);
    set_uint16(cipherid, tor_htons(cipher));
    cipherid[2] = 0; /* If ssl23_get_cipher_by_char finds no cipher starting
                      * with a two-byte 'cipherid', it may look for a v2
                      * cipher with the appropriate 3 bytes. */
    c = SSL_CIPHER_find((SSL*)ssl, cipherid);
    if (c)
      tor_assert((SSL_CIPHER_get_id(c) & 0xffff) == cipher);
    return c != NULL;
  }
#else /* !(defined(HAVE_SSL_CIPHER_FIND)) */

# if defined(HAVE_STRUCT_SSL_METHOD_ST_GET_CIPHER_BY_CHAR)
  if (m && m->get_cipher_by_char) {
    unsigned char cipherid[3];
    set_uint16(cipherid, tor_htons(cipher));
    cipherid[2] = 0; /* If ssl23_get_cipher_by_char finds no cipher starting
                      * with a two-byte 'cipherid', it may look for a v2
                      * cipher with the appropriate 3 bytes. */
    c = m->get_cipher_by_char(cipherid);
    if (c)
      tor_assert((c->id & 0xffff) == cipher);
    return c != NULL;
  }
#endif /* defined(HAVE_STRUCT_SSL_METHOD_ST_GET_CIPHER_BY_CHAR) */
# ifndef OPENSSL_1_1_API
  if (m && m->get_cipher && m->num_ciphers) {
    /* It would seem that some of the "let's-clean-up-openssl" forks have
     * removed the get_cipher_by_char function.  Okay, so now you get a
     * quadratic search.
     */
    int i;
    for (i = 0; i < m->num_ciphers(); ++i) {
      c = m->get_cipher(i);
      if (c && (c->id & 0xffff) == cipher) {
        return 1;
      }
    }
    return 0;
  }
#endif /* !defined(OPENSSL_1_1_API) */
  (void) ssl;
  (void) m;
  (void) cipher;
  return 1; /* No way to search */
#endif /* defined(HAVE_SSL_CIPHER_FIND) */
}

/** Remove from v2_cipher_list every cipher that we don't support, so that
 * comparing v2_cipher_list to a client's cipher list will give a sensible
 * result. */
static void
prune_v2_cipher_list(const SSL *ssl)
{
  uint16_t *inp, *outp;
#ifdef HAVE_TLS_METHOD
  const SSL_METHOD *m = TLS_method();
#else
  const SSL_METHOD *m = SSLv23_method();
#endif

  inp = outp = v2_cipher_list;
  while (*inp) {
    if (find_cipher_by_id(ssl, m, *inp)) {
      *outp++ = *inp++;
    } else {
      inp++;
    }
  }
  *outp = 0;

  v2_cipher_list_pruned = 1;
}

/** Examine the client cipher list in <b>ssl</b>, and determine what kind of
 * client it is.  Return one of CIPHERS_ERR, CIPHERS_V1, CIPHERS_V2,
 * CIPHERS_UNRESTRICTED.
 **/
int
tor_tls_classify_client_ciphers(const SSL *ssl,
                                STACK_OF(SSL_CIPHER) *peer_ciphers)
{
  int i, res;
  tor_tls_t *tor_tls;
  if (PREDICT_UNLIKELY(!v2_cipher_list_pruned))
    prune_v2_cipher_list(ssl);

  tor_tls = tor_tls_get_by_ssl(ssl);
  if (tor_tls && tor_tls->client_cipher_list_type)
    return tor_tls->client_cipher_list_type;

  /* If we reached this point, we just got a client hello.  See if there is
   * a cipher list. */
  if (!peer_ciphers) {
    log_info(LD_NET, "No ciphers on session");
    res = CIPHERS_ERR;
    goto done;
  }
  /* Now we need to see if there are any ciphers whose presence means we're
   * dealing with an updated Tor. */
  for (i = 0; i < sk_SSL_CIPHER_num(peer_ciphers); ++i) {
    const SSL_CIPHER *cipher = sk_SSL_CIPHER_value(peer_ciphers, i);
    const char *ciphername = SSL_CIPHER_get_name(cipher);
    if (strcmp(ciphername, TLS1_TXT_DHE_RSA_WITH_AES_128_SHA) &&
        strcmp(ciphername, TLS1_TXT_DHE_RSA_WITH_AES_256_SHA) &&
        strcmp(ciphername, SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA) &&
        strcmp(ciphername, "(NONE)")) {
      log_debug(LD_NET, "Got a non-version-1 cipher called '%s'", ciphername);
      // return 1;
      goto v2_or_higher;
    }
  }
  res = CIPHERS_V1;
  goto done;
 v2_or_higher:
  {
    const uint16_t *v2_cipher = v2_cipher_list;
    for (i = 0; i < sk_SSL_CIPHER_num(peer_ciphers); ++i) {
      const SSL_CIPHER *cipher = sk_SSL_CIPHER_value(peer_ciphers, i);
      uint16_t id = SSL_CIPHER_get_id(cipher) & 0xffff;
      if (id == 0x00ff) /* extended renegotiation indicator. */
        continue;
      if (!id || id != *v2_cipher) {
        res = CIPHERS_UNRESTRICTED;
        goto dump_ciphers;
      }
      ++v2_cipher;
    }
    if (*v2_cipher != 0) {
      res = CIPHERS_UNRESTRICTED;
      goto dump_ciphers;
    }
    res = CIPHERS_V2;
  }

 dump_ciphers:
  {
    smartlist_t *elts = smartlist_new();
    char *s;
    for (i = 0; i < sk_SSL_CIPHER_num(peer_ciphers); ++i) {
      const SSL_CIPHER *cipher = sk_SSL_CIPHER_value(peer_ciphers, i);
      const char *ciphername = SSL_CIPHER_get_name(cipher);
      smartlist_add(elts, (char*)ciphername);
    }
    s = smartlist_join_strings(elts, ":", 0, NULL);
    log_debug(LD_NET, "Got a %s V2/V3 cipher list from %s.  It is: '%s'",
              (res == CIPHERS_V2) ? "fictitious" : "real", ADDR(tor_tls), s);
    tor_free(s);
    smartlist_free(elts);
  }
 done:
  if (tor_tls && peer_ciphers)
    return tor_tls->client_cipher_list_type = res;

  return res;
}

/** Return true iff the cipher list suggested by the client for <b>ssl</b> is
 * a list that indicates that the client knows how to do the v2 TLS connection
 * handshake. */
int
tor_tls_client_is_using_v2_ciphers(const SSL *ssl)
{
  STACK_OF(SSL_CIPHER) *ciphers;
#ifdef HAVE_SSL_GET_CLIENT_CIPHERS
  ciphers = SSL_get_client_ciphers(ssl);
#else
  SSL_SESSION *session;
  if (!(session = SSL_get_session((SSL *)ssl))) {
    log_info(LD_NET, "No session on TLS?");
    return CIPHERS_ERR;
  }
  ciphers = session->ciphers;
#endif /* defined(HAVE_SSL_GET_CLIENT_CIPHERS) */

  return tor_tls_classify_client_ciphers(ssl, ciphers) >= CIPHERS_V2;
}

/** Invoked when we're accepting a connection on <b>ssl</b>, and the connection
 * changes state. We use this:
 * <ul><li>To alter the state of the handshake partway through, so we
 *         do not send or request extra certificates in v2 handshakes.</li>
 * <li>To detect renegotiation</li></ul>
 */
void
tor_tls_server_info_callback(const SSL *ssl, int type, int val)
{
  tor_tls_t *tls;
  (void) val;

  IF_BUG_ONCE(ssl == NULL) {
    return; // LCOV_EXCL_LINE
  }

  tor_tls_debug_state_callback(ssl, type, val);

  if (type != SSL_CB_ACCEPT_LOOP)
    return;

  OSSL_HANDSHAKE_STATE ssl_state = SSL_get_state(ssl);
  if (! STATE_IS_SW_SERVER_HELLO(ssl_state))
    return;
  tls = tor_tls_get_by_ssl(ssl);
  if (tls) {
    /* Check whether we're watching for renegotiates.  If so, this is one! */
    if (tls->negotiated_callback)
      tls->got_renegotiate = 1;
  } else {
    log_warn(LD_BUG, "Couldn't look up the tls for an SSL*. How odd!");
    return;
  }

  /* Now check the cipher list. */
  if (tor_tls_client_is_using_v2_ciphers(ssl)) {
    if (tls->wasV2Handshake)
      return; /* We already turned this stuff off for the first handshake;
               * This is a renegotiation. */

    /* Yes, we're casting away the const from ssl.  This is very naughty of us.
     * Let's hope openssl doesn't notice! */

    /* Set SSL_MODE_NO_AUTO_CHAIN to keep from sending back any extra certs. */
    SSL_set_mode((SSL*) ssl, SSL_MODE_NO_AUTO_CHAIN);
    /* Don't send a hello request. */
    SSL_set_verify((SSL*) ssl, SSL_VERIFY_NONE, NULL);

    if (tls) {
      tls->wasV2Handshake = 1;
    } else {
      /* LCOV_EXCL_START this line is not reachable */
      log_warn(LD_BUG, "Couldn't look up the tls for an SSL*. How odd!");
      /* LCOV_EXCL_STOP */
    }
  }
}

/** Callback to get invoked on a server after we've read the list of ciphers
 * the client supports, but before we pick our own ciphersuite.
 *
 * We can't abuse an info_cb for this, since by the time one of the
 * client_hello info_cbs is called, we've already picked which ciphersuite to
 * use.
 *
 * Technically, this function is an abuse of this callback, since the point of
 * a session_secret_cb is to try to set up and/or verify a shared-secret for
 * authentication on the fly.  But as long as we return 0, we won't actually be
 * setting up a shared secret, and all will be fine.
 */
int
tor_tls_session_secret_cb(SSL *ssl, void *secret, int *secret_len,
                          STACK_OF(SSL_CIPHER) *peer_ciphers,
                          CONST_IF_OPENSSL_1_1_API SSL_CIPHER **cipher,
                          void *arg)
{
  (void) secret;
  (void) secret_len;
  (void) peer_ciphers;
  (void) cipher;
  (void) arg;

  if (tor_tls_classify_client_ciphers(ssl, peer_ciphers) ==
       CIPHERS_UNRESTRICTED) {
    SSL_set_cipher_list(ssl, UNRESTRICTED_SERVER_CIPHER_LIST);
  }

  SSL_set_session_secret_cb(ssl, NULL, NULL);

  return 0;
}
static void
tor_tls_setup_session_secret_cb(tor_tls_t *tls)
{
  SSL_set_session_secret_cb(tls->ssl, tor_tls_session_secret_cb, NULL);
}

/** Create a new TLS object from a file descriptor, and a flag to
 * determine whether it is functioning as a server.
 */
tor_tls_t *
tor_tls_new(tor_socket_t sock, int isServer)
{
  BIO *bio = NULL;
  tor_tls_t *result = tor_malloc_zero(sizeof(tor_tls_t));
  tor_tls_context_t *context = tor_tls_context_get(isServer);
  result->magic = TOR_TLS_MAGIC;

  check_no_tls_errors();
  tor_assert(context); /* make sure somebody made it first */
  if (!(result->ssl = SSL_new(context->ctx))) {
    tls_log_errors(NULL, LOG_WARN, LD_NET, "creating SSL object");
    tor_free(result);
    goto err;
  }

#ifdef SSL_set_tlsext_host_name
  /* Browsers use the TLS hostname extension, so we should too. */
  if (!isServer) {
    char *fake_hostname = crypto_random_hostname(4,25, "www.",".com");
    SSL_set_tlsext_host_name(result->ssl, fake_hostname);
    tor_free(fake_hostname);
  }
#endif /* defined(SSL_set_tlsext_host_name) */

#ifdef SSL_CTRL_SET_MAX_PROTO_VERSION
  if (openssl_bug_7712_is_present) {
    /* We can't actually use TLS 1.3 until this bug is fixed. */
    SSL_set_max_proto_version(result->ssl, TLS1_2_VERSION);
  }
#endif

  if (!SSL_set_cipher_list(result->ssl,
                     isServer ? SERVER_CIPHER_LIST : CLIENT_CIPHER_LIST)) {
    tls_log_errors(NULL, LOG_WARN, LD_NET, "setting ciphers");
#ifdef SSL_set_tlsext_host_name
    SSL_set_tlsext_host_name(result->ssl, NULL);
#endif
    SSL_free(result->ssl);
    tor_free(result);
    goto err;
  }
  result->socket = sock;
  bio = BIO_new_socket(sock, BIO_CLOSE);
  if (! bio) {
    tls_log_errors(NULL, LOG_WARN, LD_NET, "opening BIO");
#ifdef SSL_set_tlsext_host_name
    SSL_set_tlsext_host_name(result->ssl, NULL);
#endif
    SSL_free(result->ssl);
    tor_free(result);
    goto err;
  }
  {
    int set_worked =
      SSL_set_ex_data(result->ssl, tor_tls_object_ex_data_index, result);
    if (!set_worked) {
      log_warn(LD_BUG,
               "Couldn't set the tls for an SSL*; connection will fail");
    }
  }
  SSL_set_bio(result->ssl, bio, bio);
  tor_tls_context_incref(context);
  result->context = context;
  result->state = TOR_TLS_ST_HANDSHAKE;
  result->isServer = isServer;
  result->wantwrite_n = 0;
  result->last_write_count = (unsigned long) BIO_number_written(bio);
  result->last_read_count = (unsigned long) BIO_number_read(bio);
  if (result->last_write_count || result->last_read_count) {
    log_warn(LD_NET, "Newly created BIO has read count %lu, write count %lu",
             result->last_read_count, result->last_write_count);
  }
  if (isServer) {
    SSL_set_info_callback(result->ssl, tor_tls_server_info_callback);
  } else {
    SSL_set_info_callback(result->ssl, tor_tls_debug_state_callback);
  }

  if (isServer)
    tor_tls_setup_session_secret_cb(result);

  goto done;
 err:
  result = NULL;
 done:
  /* Not expected to get called. */
  tls_log_errors(NULL, LOG_WARN, LD_NET, "creating tor_tls_t object");
  return result;
}

/** Set <b>cb</b> to be called with argument <b>arg</b> whenever <b>tls</b>
 * next gets a client-side renegotiate in the middle of a read.  Do not
 * invoke this function until <em>after</em> initial handshaking is done!
 */
void
tor_tls_set_renegotiate_callback(tor_tls_t *tls,
                                 void (*cb)(tor_tls_t *, void *arg),
                                 void *arg)
{
  tls->negotiated_callback = cb;
  tls->callback_arg = arg;
  tls->got_renegotiate = 0;
  if (cb) {
    SSL_set_info_callback(tls->ssl, tor_tls_server_info_callback);
  } else {
    SSL_set_info_callback(tls->ssl, tor_tls_debug_state_callback);
  }
}

/** If this version of openssl requires it, turn on renegotiation on
 * <b>tls</b>.
 */
void
tor_tls_unblock_renegotiation(tor_tls_t *tls)
{
  /* Yes, we know what we are doing here.  No, we do not treat a renegotiation
   * as authenticating any earlier-received data. */
  SSL_set_options(tls->ssl,
                  SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION);
}

/** If this version of openssl supports it, turn off renegotiation on
 * <b>tls</b>.  (Our protocol never requires this for security, but it's nice
 * to use belt-and-suspenders here.)
 */
void
tor_tls_block_renegotiation(tor_tls_t *tls)
{
#ifdef SUPPORT_UNSAFE_RENEGOTIATION_FLAG
  tls->ssl->s3->flags &= ~SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION;
#else
  (void) tls;
#endif
}

/** Assert that the flags that allow legacy renegotiation are still set */
void
tor_tls_assert_renegotiation_unblocked(tor_tls_t *tls)
{
#if defined(SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION) && \
  SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION != 0
  long options = SSL_get_options(tls->ssl);
  tor_assert(0 != (options & SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION));
#else
  (void) tls;
#endif /* defined(SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION) && ... */
}

/**
 * Tell the TLS library that the underlying socket for <b>tls</b> has been
 * closed, and the library should not attempt to free that socket itself.
 */
void
tor_tls_release_socket(tor_tls_t *tls)
{
  if (! tls)
    return;

  BIO *rbio, *wbio;
  rbio = SSL_get_rbio(tls->ssl);
  wbio = SSL_get_wbio(tls->ssl);

  if (rbio) {
    (void) BIO_set_close(rbio, BIO_NOCLOSE);
  }
  if (wbio && wbio != rbio) {
    (void) BIO_set_close(wbio, BIO_NOCLOSE);
  }
}

void
tor_tls_impl_free_(tor_tls_impl_t *ssl)
{
  if (!ssl)
    return;

#ifdef SSL_set_tlsext_host_name
  SSL_set_tlsext_host_name(ssl, NULL);
#endif
  SSL_free(ssl);
}

/** Underlying function for TLS reading.  Reads up to <b>len</b>
 * characters from <b>tls</b> into <b>cp</b>.  On success, returns the
 * number of characters read.  On failure, returns TOR_TLS_ERROR,
 * TOR_TLS_CLOSE, TOR_TLS_WANTREAD, or TOR_TLS_WANTWRITE.
 */
MOCK_IMPL(int,
tor_tls_read,(tor_tls_t *tls, char *cp, size_t len))
{
  int r, err;
  tor_assert(tls);
  tor_assert(tls->ssl);
  tor_assert(tls->state == TOR_TLS_ST_OPEN);
  tor_assert(len<INT_MAX);
  r = SSL_read(tls->ssl, cp, (int)len);
  if (r > 0) {
    if (tls->got_renegotiate) {
      /* Renegotiation happened! */
      log_info(LD_NET, "Got a TLS renegotiation from %s", ADDR(tls));
      if (tls->negotiated_callback)
        tls->negotiated_callback(tls, tls->callback_arg);
      tls->got_renegotiate = 0;
    }
    return r;
  }
  err = tor_tls_get_error(tls, r, CATCH_ZERO, "reading", LOG_DEBUG, LD_NET);
  if (err == TOR_TLS_ZERORETURN_ || err == TOR_TLS_CLOSE) {
    log_debug(LD_NET,"read returned r=%d; TLS is closed",r);
    tls->state = TOR_TLS_ST_CLOSED;
    return TOR_TLS_CLOSE;
  } else {
    tor_assert(err != TOR_TLS_DONE);
    log_debug(LD_NET,"read returned r=%d, err=%d",r,err);
    return err;
  }
}

/** Total number of bytes that we've used TLS to send.  Used to track TLS
 * overhead. */
STATIC uint64_t total_bytes_written_over_tls = 0;
/** Total number of bytes that TLS has put on the network for us. Used to
 * track TLS overhead. */
STATIC uint64_t total_bytes_written_by_tls = 0;

/** Underlying function for TLS writing.  Write up to <b>n</b>
 * characters from <b>cp</b> onto <b>tls</b>.  On success, returns the
 * number of characters written.  On failure, returns TOR_TLS_ERROR,
 * TOR_TLS_WANTREAD, or TOR_TLS_WANTWRITE.
 */
int
tor_tls_write(tor_tls_t *tls, const char *cp, size_t n)
{
  int r, err;
  tor_assert(tls);
  tor_assert(tls->ssl);
  tor_assert(tls->state == TOR_TLS_ST_OPEN);
  tor_assert(n < INT_MAX);
  if (n == 0)
    return 0;
  if (tls->wantwrite_n) {
    /* if WANTWRITE last time, we must use the _same_ n as before */
    tor_assert(n >= tls->wantwrite_n);
    log_debug(LD_NET,"resuming pending-write, (%d to flush, reusing %d)",
              (int)n, (int)tls->wantwrite_n);
    n = tls->wantwrite_n;
    tls->wantwrite_n = 0;
  }
  r = SSL_write(tls->ssl, cp, (int)n);
  err = tor_tls_get_error(tls, r, 0, "writing", LOG_INFO, LD_NET);
  if (err == TOR_TLS_DONE) {
    total_bytes_written_over_tls += r;
    return r;
  }
  if (err == TOR_TLS_WANTWRITE || err == TOR_TLS_WANTREAD) {
    tls->wantwrite_n = n;
  }
  return err;
}

/** Perform initial handshake on <b>tls</b>.  When finished, returns
 * TOR_TLS_DONE.  On failure, returns TOR_TLS_ERROR, TOR_TLS_WANTREAD,
 * or TOR_TLS_WANTWRITE.
 */
int
tor_tls_handshake(tor_tls_t *tls)
{
  int r;
  tor_assert(tls);
  tor_assert(tls->ssl);
  tor_assert(tls->state == TOR_TLS_ST_HANDSHAKE);

  check_no_tls_errors();

  OSSL_HANDSHAKE_STATE oldstate = SSL_get_state(tls->ssl);

  if (tls->isServer) {
    log_debug(LD_HANDSHAKE, "About to call SSL_accept on %p (%s)", tls,
              SSL_state_string_long(tls->ssl));
    r = SSL_accept(tls->ssl);
  } else {
    log_debug(LD_HANDSHAKE, "About to call SSL_connect on %p (%s)", tls,
              SSL_state_string_long(tls->ssl));
    r = SSL_connect(tls->ssl);
  }

  OSSL_HANDSHAKE_STATE newstate = SSL_get_state(tls->ssl);

  if (oldstate != newstate)
    log_debug(LD_HANDSHAKE, "After call, %p was in state %s",
              tls, SSL_state_string_long(tls->ssl));
  /* We need to call this here and not earlier, since OpenSSL has a penchant
   * for clearing its flags when you say accept or connect. */
  tor_tls_unblock_renegotiation(tls);
  r = tor_tls_get_error(tls,r,0, "handshaking", LOG_INFO, LD_HANDSHAKE);
  if (ERR_peek_error() != 0) {
    tls_log_errors(tls, tls->isServer ? LOG_INFO : LOG_WARN, LD_HANDSHAKE,
                   "handshaking");
    return TOR_TLS_ERROR_MISC;
  }
  if (r == TOR_TLS_DONE) {
    tls->state = TOR_TLS_ST_OPEN;
    return tor_tls_finish_handshake(tls);
  }
  return r;
}

/** Perform the final part of the initial TLS handshake on <b>tls</b>.  This
 * should be called for the first handshake only: it determines whether the v1
 * or the v2 handshake was used, and adjusts things for the renegotiation
 * handshake as appropriate.
 *
 * tor_tls_handshake() calls this on its own; you only need to call this if
 * bufferevent is doing the handshake for you.
 */
int
tor_tls_finish_handshake(tor_tls_t *tls)
{
  int r = TOR_TLS_DONE;
  check_no_tls_errors();
  if (tls->isServer) {
    SSL_set_info_callback(tls->ssl, NULL);
    SSL_set_verify(tls->ssl, SSL_VERIFY_PEER, always_accept_verify_cb);
    SSL_clear_mode(tls->ssl, SSL_MODE_NO_AUTO_CHAIN);
    if (tor_tls_client_is_using_v2_ciphers(tls->ssl)) {
      /* This check is redundant, but back when we did it in the callback,
       * we might have not been able to look up the tor_tls_t if the code
       * was buggy.  Fixing that. */
      if (!tls->wasV2Handshake) {
        log_warn(LD_BUG, "For some reason, wasV2Handshake didn't"
                 " get set. Fixing that.");
      }
      tls->wasV2Handshake = 1;
      log_debug(LD_HANDSHAKE, "Completed V2 TLS handshake with client; waiting"
                " for renegotiation.");
    } else {
      tls->wasV2Handshake = 0;
    }
  } else {
    /* Client-side */
    tls->wasV2Handshake = 1;
    /* XXXX this can move, probably? -NM */
    if (SSL_set_cipher_list(tls->ssl, SERVER_CIPHER_LIST) == 0) {
      tls_log_errors(NULL, LOG_WARN, LD_HANDSHAKE, "re-setting ciphers");
      r = TOR_TLS_ERROR_MISC;
    }
  }
  tls_log_errors(NULL, LOG_WARN, LD_NET, "finishing the handshake");
  return r;
}

/** Return true iff this TLS connection is authenticated.
 */
int
tor_tls_peer_has_cert(tor_tls_t *tls)
{
  X509 *cert;
  cert = SSL_get_peer_certificate(tls->ssl);
  tls_log_errors(tls, LOG_WARN, LD_HANDSHAKE, "getting peer certificate");
  if (!cert)
    return 0;
  X509_free(cert);
  return 1;
}

/** Return a newly allocated copy of the peer certificate, or NULL if there
 * isn't one. */
MOCK_IMPL(tor_x509_cert_t *,
tor_tls_get_peer_cert,(tor_tls_t *tls))
{
  X509 *cert;
  cert = SSL_get_peer_certificate(tls->ssl);
  tls_log_errors(tls, LOG_WARN, LD_HANDSHAKE, "getting peer certificate");
  if (!cert)
    return NULL;
  return tor_x509_cert_new(cert);
}

/** Return a newly allocated copy of the cerficate we used on the connection,
 * or NULL if somehow we didn't use one. */
MOCK_IMPL(tor_x509_cert_t *,
tor_tls_get_own_cert,(tor_tls_t *tls))
{
  X509 *cert = SSL_get_certificate(tls->ssl);
  tls_log_errors(tls, LOG_WARN, LD_HANDSHAKE,
                 "getting own-connection certificate");
  if (!cert)
    return NULL;
  /* Fun inconsistency: SSL_get_peer_certificate increments the reference
   * count, but SSL_get_certificate does not. */
  X509 *duplicate = X509_dup(cert);
  if (BUG(duplicate == NULL))
    return NULL;
  return tor_x509_cert_new(duplicate);
}

/** Helper function: try to extract a link certificate and an identity
 * certificate from <b>tls</b>, and store them in *<b>cert_out</b> and
 * *<b>id_cert_out</b> respectively.  Log all messages at level
 * <b>severity</b>.
 *
 * Note that a reference is added both of the returned certificates. */
MOCK_IMPL(void,
try_to_extract_certs_from_tls,(int severity, tor_tls_t *tls,
                               X509 **cert_out, X509 **id_cert_out))
{
  X509 *cert = NULL, *id_cert = NULL;
  STACK_OF(X509) *chain = NULL;
  int num_in_chain, i;
  *cert_out = *id_cert_out = NULL;
  if (!(cert = SSL_get_peer_certificate(tls->ssl)))
    return;
  *cert_out = cert;
  if (!(chain = SSL_get_peer_cert_chain(tls->ssl)))
    return;
  num_in_chain = sk_X509_num(chain);
  /* 1 means we're receiving (server-side), and it's just the id_cert.
   * 2 means we're connecting (client-side), and it's both the link
   * cert and the id_cert.
   */
  if (num_in_chain < 1) {
    log_fn(severity,LD_PROTOCOL,
           "Unexpected number of certificates in chain (%d)",
           num_in_chain);
    return;
  }
  for (i=0; i<num_in_chain; ++i) {
    id_cert = sk_X509_value(chain, i);
    if (X509_cmp(id_cert, cert) != 0)
      break;
  }
  *id_cert_out = id_cert ? X509_dup(id_cert) : NULL;
}

/** Return the number of bytes available for reading from <b>tls</b>.
 */
int
tor_tls_get_pending_bytes(tor_tls_t *tls)
{
  tor_assert(tls);
  return SSL_pending(tls->ssl);
}

/** If <b>tls</b> requires that the next write be of a particular size,
 * return that size.  Otherwise, return 0. */
size_t
tor_tls_get_forced_write_size(tor_tls_t *tls)
{
  return tls->wantwrite_n;
}

/** Sets n_read and n_written to the number of bytes read and written,
 * respectively, on the raw socket used by <b>tls</b> since the last time this
 * function was called on <b>tls</b>. */
void
tor_tls_get_n_raw_bytes(tor_tls_t *tls, size_t *n_read, size_t *n_written)
{
  BIO *wbio, *tmpbio;
  unsigned long r, w;
  r = (unsigned long) BIO_number_read(SSL_get_rbio(tls->ssl));
  /* We want the number of bytes actually for real written.  Unfortunately,
   * sometimes OpenSSL replaces the wbio on tls->ssl with a buffering bio,
   * which makes the answer turn out wrong.  Let's cope with that.  Note
   * that this approach will fail if we ever replace tls->ssl's BIOs with
   * buffering bios for reasons of our own.  As an alternative, we could
   * save the original BIO for  tls->ssl in the tor_tls_t structure, but
   * that would be tempting fate. */
  wbio = SSL_get_wbio(tls->ssl);
#if OPENSSL_VERSION_NUMBER >= OPENSSL_VER(1,1,0,0,5)
  /* BIO structure is opaque as of OpenSSL 1.1.0-pre5-dev.  Again, not
   * supposed to use this form of the version macro, but the OpenSSL developers
   * introduced major API changes in the pre-release stage.
   */
  if (BIO_method_type(wbio) == BIO_TYPE_BUFFER &&
        (tmpbio = BIO_next(wbio)) != NULL)
    wbio = tmpbio;
#else /* !(OPENSSL_VERSION_NUMBER >= OPENSSL_VER(1,1,0,0,5)) */
  if (wbio->method == BIO_f_buffer() && (tmpbio = BIO_next(wbio)) != NULL)
    wbio = tmpbio;
#endif /* OPENSSL_VERSION_NUMBER >= OPENSSL_VER(1,1,0,0,5) */
  w = (unsigned long) BIO_number_written(wbio);

  /* We are ok with letting these unsigned ints go "negative" here:
   * If we wrapped around, this should still give us the right answer, unless
   * we wrapped around by more than ULONG_MAX since the last time we called
   * this function.
   */
  *n_read = (size_t)(r - tls->last_read_count);
  *n_written = (size_t)(w - tls->last_write_count);
  if (*n_read > INT_MAX || *n_written > INT_MAX) {
    log_warn(LD_BUG, "Preposterously large value in tor_tls_get_n_raw_bytes. "
             "r=%lu, last_read=%lu, w=%lu, last_written=%lu",
             r, tls->last_read_count, w, tls->last_write_count);
  }
  total_bytes_written_by_tls += *n_written;
  tls->last_read_count = r;
  tls->last_write_count = w;
}

/** Return a ratio of the bytes that TLS has sent to the bytes that we've told
 * it to send. Used to track whether our TLS records are getting too tiny. */
MOCK_IMPL(double,
tls_get_write_overhead_ratio,(void))
{
  if (total_bytes_written_over_tls == 0)
    return 1.0;

  return ((double)total_bytes_written_by_tls) /
    ((double)total_bytes_written_over_tls);
}

/** Implement check_no_tls_errors: If there are any pending OpenSSL
 * errors, log an error message. */
void
check_no_tls_errors_(const char *fname, int line)
{
  if (ERR_peek_error() == 0)
    return;
  log_warn(LD_CRYPTO, "Unhandled OpenSSL errors found at %s:%d: ",
      tor_fix_source_file(fname), line);
  tls_log_errors(NULL, LOG_WARN, LD_NET, NULL);
}

/** Return true iff the initial TLS connection at <b>tls</b> did not use a v2
 * TLS handshake. Output is undefined if the handshake isn't finished. */
int
tor_tls_used_v1_handshake(tor_tls_t *tls)
{
  return ! tls->wasV2Handshake;
}

/** Return true iff the server TLS connection <b>tls</b> got the renegotiation
 * request it was waiting for. */
int
tor_tls_server_got_renegotiate(tor_tls_t *tls)
{
  return tls->got_renegotiate;
}

#ifndef HAVE_SSL_GET_CLIENT_RANDOM
static size_t
SSL_get_client_random(SSL *s, uint8_t *out, size_t len)
{
  if (len == 0)
    return SSL3_RANDOM_SIZE;
  tor_assert(len == SSL3_RANDOM_SIZE);
  tor_assert(s->s3);
  memcpy(out, s->s3->client_random, len);
  return len;
}
#endif /* !defined(HAVE_SSL_GET_CLIENT_RANDOM) */

#ifndef HAVE_SSL_GET_SERVER_RANDOM
static size_t
SSL_get_server_random(SSL *s, uint8_t *out, size_t len)
{
  if (len == 0)
    return SSL3_RANDOM_SIZE;
  tor_assert(len == SSL3_RANDOM_SIZE);
  tor_assert(s->s3);
  memcpy(out, s->s3->server_random, len);
  return len;
}
#endif /* !defined(HAVE_SSL_GET_SERVER_RANDOM) */

#ifndef HAVE_SSL_SESSION_GET_MASTER_KEY
size_t
SSL_SESSION_get_master_key(SSL_SESSION *s, uint8_t *out, size_t len)
{
  tor_assert(s);
  if (len == 0)
    return s->master_key_length;
  tor_assert(len == (size_t)s->master_key_length);
  tor_assert(out);
  memcpy(out, s->master_key, len);
  return len;
}
#endif /* !defined(HAVE_SSL_SESSION_GET_MASTER_KEY) */

/** Set the DIGEST256_LEN buffer at <b>secrets_out</b> to the value used in
 * the v3 handshake to prove that the client knows the TLS secrets for the
 * connection <b>tls</b>.  Return 0 on success, -1 on failure.
 */
MOCK_IMPL(int,
tor_tls_get_tlssecrets,(tor_tls_t *tls, uint8_t *secrets_out))
{
#define TLSSECRET_MAGIC "Tor V3 handshake TLS cross-certification"
  uint8_t buf[128];
  size_t len;
  tor_assert(tls);

  SSL *const ssl = tls->ssl;
  SSL_SESSION *const session = SSL_get_session(ssl);

  tor_assert(ssl);
  tor_assert(session);

  const size_t server_random_len = SSL_get_server_random(ssl, NULL, 0);
  const size_t client_random_len = SSL_get_client_random(ssl, NULL, 0);
  const size_t master_key_len = SSL_SESSION_get_master_key(session, NULL, 0);

  tor_assert(server_random_len);
  tor_assert(client_random_len);
  tor_assert(master_key_len);

  len = client_random_len + server_random_len + strlen(TLSSECRET_MAGIC) + 1;
  tor_assert(len <= sizeof(buf));

  {
    size_t r = SSL_get_client_random(ssl, buf, client_random_len);
    tor_assert(r == client_random_len);
  }

  {
    size_t r = SSL_get_server_random(ssl,
                                     buf+client_random_len,
                                     server_random_len);
    tor_assert(r == server_random_len);
  }

  uint8_t *master_key = tor_malloc_zero(master_key_len);
  {
    size_t r = SSL_SESSION_get_master_key(session, master_key, master_key_len);
    tor_assert(r == master_key_len);
  }

  uint8_t *nextbuf = buf + client_random_len + server_random_len;
  memcpy(nextbuf, TLSSECRET_MAGIC, strlen(TLSSECRET_MAGIC) + 1);

  /*
    The value is an HMAC, using the TLS master key as the HMAC key, of
    client_random | server_random | TLSSECRET_MAGIC
  */
  crypto_hmac_sha256((char*)secrets_out,
                     (char*)master_key,
                     master_key_len,
                     (char*)buf, len);
  memwipe(buf, 0, sizeof(buf));
  memwipe(master_key, 0, master_key_len);
  tor_free(master_key);

  return 0;
}

/** Using the RFC5705 key material exporting construction, and the
 * provided <b>context</b> (<b>context_len</b> bytes long) and
 * <b>label</b> (a NUL-terminated string), compute a 32-byte secret in
 * <b>secrets_out</b> that only the parties to this TLS session can
 * compute.  Return 0 on success; -1 on failure; and -2 on failure
 * caused by OpenSSL bug 7712.
 */
MOCK_IMPL(int,
tor_tls_export_key_material,(tor_tls_t *tls, uint8_t *secrets_out,
                             const uint8_t *context,
                             size_t context_len,
                             const char *label))
{
  tor_assert(tls);
  tor_assert(tls->ssl);

  int r = SSL_export_keying_material(tls->ssl,
                                     secrets_out, DIGEST256_LEN,
                                     label, strlen(label),
                                     context, context_len, 1);

  if (r != 1) {
    int severity = openssl_bug_7712_is_present ? LOG_WARN : LOG_DEBUG;
    tls_log_errors(tls, severity, LD_NET, "exporting keying material");
  }

#ifdef TLS1_3_VERSION
  if (r != 1 &&
      strlen(label) > 12 &&
      SSL_version(tls->ssl) >= TLS1_3_VERSION) {

    if (! openssl_bug_7712_is_present) {
      /* We might have run into OpenSSL issue 7712, which caused OpenSSL
       * 1.1.1a to not handle long labels.  Let's test to see if we have.
       */
      r = SSL_export_keying_material(tls->ssl, secrets_out, DIGEST256_LEN,
                                     "short", 5, context, context_len, 1);
      if (r == 1) {
        /* A short label succeeds, but a long label fails. This was openssl
         * issue 7712. */
        openssl_bug_7712_is_present = 1;
        log_warn(LD_GENERAL, "Detected OpenSSL bug 7712: disabling TLS 1.3 on "
                 "future connections. A fix is expected to appear in OpenSSL "
                 "1.1.1b.");
      }
    }
    if (openssl_bug_7712_is_present)
      return -2;
    else
      return -1;
  }
#endif

  return (r == 1) ? 0 : -1;
}

/** Examine the amount of memory used and available for buffers in <b>tls</b>.
 * Set *<b>rbuf_capacity</b> to the amount of storage allocated for the read
 * buffer and *<b>rbuf_bytes</b> to the amount actually used.
 * Set *<b>wbuf_capacity</b> to the amount of storage allocated for the write
 * buffer and *<b>wbuf_bytes</b> to the amount actually used.
 *
 * Return 0 on success, -1 on failure.*/
int
tor_tls_get_buffer_sizes(tor_tls_t *tls,
                         size_t *rbuf_capacity, size_t *rbuf_bytes,
                         size_t *wbuf_capacity, size_t *wbuf_bytes)
{
#if OPENSSL_VERSION_NUMBER >= OPENSSL_V_SERIES(1,1,0)
  (void)tls;
  (void)rbuf_capacity;
  (void)rbuf_bytes;
  (void)wbuf_capacity;
  (void)wbuf_bytes;

  return -1;
#else /* !(OPENSSL_VERSION_NUMBER >= OPENSSL_V_SERIES(1,1,0)) */
  if (tls->ssl->s3->rbuf.buf)
    *rbuf_capacity = tls->ssl->s3->rbuf.len;
  else
    *rbuf_capacity = 0;
  if (tls->ssl->s3->wbuf.buf)
    *wbuf_capacity = tls->ssl->s3->wbuf.len;
  else
    *wbuf_capacity = 0;
  *rbuf_bytes = tls->ssl->s3->rbuf.left;
  *wbuf_bytes = tls->ssl->s3->wbuf.left;
  return 0;
#endif /* OPENSSL_VERSION_NUMBER >= OPENSSL_V_SERIES(1,1,0) */
}

/** Check whether the ECC group requested is supported by the current OpenSSL
 * library instance.  Return 1 if the group is supported, and 0 if not.
 */
int
evaluate_ecgroup_for_tls(const char *ecgroup)
{
  EC_KEY *ec_key;
  int nid;
  int ret;

  if (!ecgroup)
    nid = NID_tor_default_ecdhe_group;
  else if (!strcasecmp(ecgroup, "P256"))
    nid = NID_X9_62_prime256v1;
  else if (!strcasecmp(ecgroup, "P224"))
    nid = NID_secp224r1;
  else
    return 0;

  ec_key = EC_KEY_new_by_curve_name(nid);
  ret = (ec_key != NULL);
  EC_KEY_free(ec_key);

  return ret;
}
