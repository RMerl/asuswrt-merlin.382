/* Copyright (c) 2003, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file tortls_nss.c
 * \brief Wrapper functions to present a consistent interface to
 * TLS and SSL X.509 functions from NSS.
 **/

#include "orconfig.h"

#define TORTLS_PRIVATE
#define TOR_X509_PRIVATE

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#endif

#include "lib/crypt_ops/crypto_cipher.h"
#include "lib/crypt_ops/crypto_rand.h"
#include "lib/crypt_ops/crypto_dh.h"
#include "lib/crypt_ops/crypto_util.h"
#include "lib/crypt_ops/crypto_nss_mgt.h"
#include "lib/string/printf.h"

#include "lib/tls/x509.h"
#include "lib/tls/x509_internal.h"
#include "lib/tls/tortls.h"
#include "lib/tls/tortls_st.h"
#include "lib/tls/tortls_internal.h"
#include "lib/tls/nss_countbytes.h"
#include "lib/log/util_bug.h"

DISABLE_GCC_WARNING(strict-prototypes)
#include <prio.h>
// For access to rar sockets.
#include <private/pprio.h>
#include <ssl.h>
#include <sslt.h>
#include <sslproto.h>
#include <certt.h>
ENABLE_GCC_WARNING(strict-prototypes)

static SECStatus always_accept_cert_cb(void *, PRFileDesc *, PRBool, PRBool);

MOCK_IMPL(void,
try_to_extract_certs_from_tls,(int severity, tor_tls_t *tls,
                               tor_x509_cert_impl_t **cert_out,
                               tor_x509_cert_impl_t **id_cert_out))
{
  tor_assert(tls);
  tor_assert(cert_out);
  tor_assert(id_cert_out);
  (void) severity;

  *cert_out = *id_cert_out = NULL;

  CERTCertificate *peer = SSL_PeerCertificate(tls->ssl);
  if (!peer)
    return;
  *cert_out = peer; /* Now owns pointer. */

  CERTCertList *chain = SSL_PeerCertificateChain(tls->ssl);
  CERTCertListNode *c = CERT_LIST_HEAD(chain);
  for (; !CERT_LIST_END(c, chain); c = CERT_LIST_NEXT(c)) {
    if (CERT_CompareCerts(c->cert, peer) == PR_FALSE) {
      *id_cert_out = CERT_DupCertificate(c->cert);
      break;
    }
  }
  CERT_DestroyCertList(chain);
}

static bool
we_like_ssl_cipher(SSLCipherAlgorithm ca)
{
  switch (ca) {
    case ssl_calg_null: return false;
    case ssl_calg_rc4: return false;
    case ssl_calg_rc2: return false;
    case ssl_calg_des: return false;
    case ssl_calg_3des: return false; /* ???? */
    case ssl_calg_idea: return false;
    case ssl_calg_fortezza: return false;
    case ssl_calg_camellia: return false;
    case ssl_calg_seed: return false;

    case ssl_calg_aes: return true;
    case ssl_calg_aes_gcm: return true;
    case ssl_calg_chacha20: return true;
    default: return true;
  }
}
static bool
we_like_ssl_kea(SSLKEAType kt)
{
  switch (kt) {
    case ssl_kea_null: return false;
    case ssl_kea_rsa: return false; /* ??? */
    case ssl_kea_fortezza: return false;
    case ssl_kea_ecdh_psk: return false;
    case ssl_kea_dh_psk: return false;

    case ssl_kea_dh: return true;
    case ssl_kea_ecdh: return true;
    case ssl_kea_tls13_any: return true;

    case ssl_kea_size: return true; /* prevent a warning. */
    default: return true;
  }
}

static bool
we_like_mac_algorithm(SSLMACAlgorithm ma)
{
  switch (ma) {
    case ssl_mac_null: return false;
    case ssl_mac_md5: return false;
    case ssl_hmac_md5: return false;

    case ssl_mac_sha: return true;
    case ssl_hmac_sha: return true;
    case ssl_hmac_sha256: return true;
    case ssl_mac_aead: return true;
    case ssl_hmac_sha384: return true;
    default: return true;
  }
}

static bool
we_like_auth_type(SSLAuthType at)
{
  switch (at) {
    case ssl_auth_null: return false;
    case ssl_auth_rsa_decrypt: return false;
    case ssl_auth_dsa: return false;
    case ssl_auth_kea: return false;

    case ssl_auth_ecdsa: return true;
    case ssl_auth_ecdh_rsa: return true;
    case ssl_auth_ecdh_ecdsa: return true;
    case ssl_auth_rsa_sign: return true;
    case ssl_auth_rsa_pss: return true;
    case ssl_auth_psk: return true;
    case ssl_auth_tls13_any: return true;

    case ssl_auth_size: return true; /* prevent a warning. */
    default: return true;
  }
}

/**
 * Return true iff this ciphersuite will be hit by a mozilla bug 1312976,
 * which makes TLS key exporters not work with TLS 1.2 non-SHA256
 * ciphersuites.
 **/
static bool
ciphersuite_has_nss_export_bug(const SSLCipherSuiteInfo *info)
{
  /* For more information on the bug, see
     https://bugzilla.mozilla.org/show_bug.cgi?id=1312976 */

  /* This bug only exists in TLS 1.2. */
  if (info->authType == ssl_auth_tls13_any)
    return false;

  /* Sadly, there's no way to get this information from the
   * CipherSuiteInfo object itself other than by looking at the
   * name.  */
  if (strstr(info->cipherSuiteName, "_SHA384") ||
      strstr(info->cipherSuiteName, "_SHA512")) {
    return true;
  }

  return false;
}

tor_tls_context_t *
tor_tls_context_new(crypto_pk_t *identity,
                    unsigned int key_lifetime, unsigned flags, int is_client)
{
  SECStatus s;
  tor_assert(identity);

  tor_tls_init();

  tor_tls_context_t *ctx = tor_malloc_zero(sizeof(tor_tls_context_t));
  ctx->refcnt = 1;

  if (! is_client) {
    if (tor_tls_context_init_certificates(ctx, identity,
                                          key_lifetime, flags) < 0) {
      goto err;
    }
  }

  {
    /* Create the "model" PRFileDesc that we will use to base others on. */
    PRFileDesc *tcp = PR_NewTCPSocket();
    if (!tcp)
      goto err;

    ctx->ctx = SSL_ImportFD(NULL, tcp);
    if (!ctx->ctx) {
      PR_Close(tcp);
      goto err;
    }
  }

  // Configure the certificate.
  if (!is_client) {
    s = SSL_ConfigServerCert(ctx->ctx,
                             ctx->my_link_cert->cert,
                             (SECKEYPrivateKey *)
                               crypto_pk_get_nss_privkey(ctx->link_key),
                             NULL, /* ExtraServerCertData */
                             0 /* DataLen */);
    if (s != SECSuccess)
      goto err;
  }

  // We need a certificate from the other side.
  if (is_client) {
    // XXXX does this do anything?
    s = SSL_OptionSet(ctx->ctx, SSL_REQUIRE_CERTIFICATE, PR_TRUE);
    if (s != SECSuccess)
      goto err;
  }

  // Always accept other side's cert; we'll check it ourselves in goofy
  // tor ways.
  s = SSL_AuthCertificateHook(ctx->ctx, always_accept_cert_cb, NULL);

  // We allow simultaneous read and write.
  s = SSL_OptionSet(ctx->ctx, SSL_ENABLE_FDX, PR_TRUE);
  if (s != SECSuccess)
    goto err;
  // XXXX SSL_ROLLBACK_DETECTION??
  // XXXX SSL_ENABLE_ALPN??

  // Force client-mode or server_mode.
  s = SSL_OptionSet(ctx->ctx,
                is_client ? SSL_HANDSHAKE_AS_CLIENT : SSL_HANDSHAKE_AS_SERVER,
                PR_TRUE);
  if (s != SECSuccess)
    goto err;

  // Disable everything before TLS 1.0; support everything else.
  {
    SSLVersionRange vrange;
    memset(&vrange, 0, sizeof(vrange));
    s = SSL_VersionRangeGetSupported(ssl_variant_stream, &vrange);
    if (s != SECSuccess)
      goto err;
    if (vrange.min < SSL_LIBRARY_VERSION_TLS_1_0)
      vrange.min = SSL_LIBRARY_VERSION_TLS_1_0;
    s = SSL_VersionRangeSet(ctx->ctx, &vrange);
    if (s != SECSuccess)
      goto err;
  }

  // Only support strong ciphers.
  {
    const PRUint16 *ciphers = SSL_GetImplementedCiphers();
    const PRUint16 n_ciphers = SSL_GetNumImplementedCiphers();
    PRUint16 i;
    for (i = 0; i < n_ciphers; ++i) {
      SSLCipherSuiteInfo info;
      memset(&info, 0, sizeof(info));
      s = SSL_GetCipherSuiteInfo(ciphers[i], &info, sizeof(info));
      if (s != SECSuccess)
        goto err;
      if (BUG(info.cipherSuite != ciphers[i]))
        goto err;
      int disable = info.effectiveKeyBits < 128 ||
        info.macBits < 128 ||
        !we_like_ssl_cipher(info.symCipher) ||
        !we_like_ssl_kea(info.keaType) ||
        !we_like_mac_algorithm(info.macAlgorithm) ||
        !we_like_auth_type(info.authType)/* Requires NSS 3.24 */;

      if (ciphersuite_has_nss_export_bug(&info)) {
        /* SSL_ExportKeyingMaterial will fail; we can't use this cipher.
         */
        disable = 1;
      }

      s = SSL_CipherPrefSet(ctx->ctx, ciphers[i],
                            disable ? PR_FALSE : PR_TRUE);
      if (s != SECSuccess)
        goto err;
    }
  }

  // Only use DH and ECDH keys once.
  s = SSL_OptionSet(ctx->ctx, SSL_REUSE_SERVER_ECDHE_KEY, PR_FALSE);
  if (s != SECSuccess)
    goto err;

  // don't cache sessions.
  s = SSL_OptionSet(ctx->ctx, SSL_NO_CACHE, PR_TRUE);
  if (s != SECSuccess)
    goto err;

  // Enable DH.
  s = SSL_OptionSet(ctx->ctx, SSL_ENABLE_SERVER_DHE, PR_TRUE);
  if (s != SECSuccess)
    goto err;

  // Set DH and ECDH groups.
  SSLNamedGroup groups[] = {
      ssl_grp_ec_curve25519,
      ssl_grp_ec_secp256r1,
      ssl_grp_ec_secp224r1,
      ssl_grp_ffdhe_2048,
  };
  s = SSL_NamedGroupConfig(ctx->ctx, groups, ARRAY_LENGTH(groups));
  if (s != SECSuccess)
    goto err;

  // These features are off by default, so we don't need to disable them:
  //   Session tickets
  //   Renegotiation
  //   Compression

  goto done;
 err:
  tor_tls_context_decref(ctx);
  ctx = NULL;
 done:
  return ctx;
}

void
tor_tls_context_impl_free_(tor_tls_context_impl_t *ctx)
{
  if (!ctx)
    return;
  PR_Close(ctx);
}

void
tor_tls_get_state_description(tor_tls_t *tls, char *buf, size_t sz)
{
  (void)tls;
  (void)buf;
  (void)sz;
  // AFAICT, NSS doesn't expose its internal state.
  buf[0]=0;
}

void
tor_tls_init(void)
{
  tor_nss_countbytes_init();
}

void
tls_log_errors(tor_tls_t *tls, int severity, int domain,
               const char *doing)
{
  /* This implementation is a little different for NSS than it is for OpenSSL
     -- it logs the last error whether anything actually failed or not. So we
     have to only call it when something has gone wrong and we have a real
     error to report. */

  (void)tls;
  PRErrorCode code = PORT_GetError();

  const char *addr = tls ? tls->address : NULL;
  const char *string = PORT_ErrorToString(code);
  const char *name = PORT_ErrorToName(code);
  char buf[16];
  if (!string)
    string = "<unrecognized>";
  if (!name) {
    tor_snprintf(buf, sizeof(buf), "%d", code);
    name = buf;
  }

  const char *with = addr ? " with " : "";
  addr = addr ? addr : "";
  if (doing) {
    log_fn(severity, domain, "TLS error %s while %s%s%s: %s",
           name, doing, with, addr, string);
  } else {
    log_fn(severity, domain, "TLS error %s%s%s: %s", name, string,
           with, addr);
  }
}

tor_tls_t *
tor_tls_new(tor_socket_t sock, int is_server)
{
  (void)sock;
  tor_tls_context_t *ctx = tor_tls_context_get(is_server);

  PRFileDesc *tcp = NULL;
  if (SOCKET_OK(sock)) {
    tcp = PR_ImportTCPSocket(sock);
  } else {
    tcp = PR_NewTCPSocket();
  }

  if (!tcp)
    return NULL;

  PRFileDesc *count = tor_wrap_prfiledesc_with_byte_counter(tcp);
  if (! count)
    return NULL;

  PRFileDesc *ssl = SSL_ImportFD(ctx->ctx, count);
  if (!ssl) {
    PR_Close(tcp);
    return NULL;
  }

  tor_tls_t *tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->magic = TOR_TLS_MAGIC;
  tls->context = ctx;
  tor_tls_context_incref(ctx);
  tls->ssl = ssl;
  tls->socket = sock;
  tls->state = TOR_TLS_ST_HANDSHAKE;
  tls->isServer = !!is_server;

  if (!is_server) {
    /* Set a random SNI */
    char *fake_hostname = crypto_random_hostname(4,25, "www.",".com");
    SSL_SetURL(tls->ssl, fake_hostname);
    tor_free(fake_hostname);
  }
  SECStatus s = SSL_ResetHandshake(ssl, is_server ? PR_TRUE : PR_FALSE);
  if (s != SECSuccess) {
    tls_log_errors(tls, LOG_WARN, LD_CRYPTO, "resetting handshake state");
  }

  return tls;
}

void
tor_tls_set_renegotiate_callback(tor_tls_t *tls,
                                 void (*cb)(tor_tls_t *, void *arg),
                                 void *arg)
{
  tor_assert(tls);
  (void)cb;
  (void)arg;

  /* We don't support renegotiation-based TLS with NSS. */
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

  /* NSS doesn't have the equivalent of BIO_NO_CLOSE.  If you replace the
   * fd with something that's invalid, it causes a memory leak in PR_Close.
   *
   * If there were a way to put the PRFileDesc into the CLOSED state, that
   * would prevent it from closing its fd -- but there doesn't seem to be a
   * supported way to do that either.
   *
   * So instead: we make a new sacrificial socket, and replace the original
   * socket with that one. This seems to be the best we can do, until we
   * redesign the mainloop code enough to make this function unnecessary.
   */
  tor_socket_t sock =
    tor_open_socket_nonblocking(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (! SOCKET_OK(sock)) {
    log_warn(LD_NET, "Out of sockets when trying to shut down an NSS "
             "connection");
    return;
  }

  PRFileDesc *tcp = PR_GetIdentitiesLayer(tls->ssl, PR_NSPR_IO_LAYER);
  if (BUG(! tcp)) {
    tor_close_socket(sock);
    return;
  }

  PR_ChangeFileDescNativeHandle(tcp, sock);
  /* Tell our socket accounting layer that we don't own this socket any more:
   * NSS is about to free it for us. */
  tor_release_socket_ownership(sock);
}

void
tor_tls_impl_free_(tor_tls_impl_t *tls)
{
  // XXXX This will close the underlying fd, which our OpenSSL version does
  // not do!
  if (!tls)
    return;

  PR_Close(tls);
}

int
tor_tls_peer_has_cert(tor_tls_t *tls)
{
  CERTCertificate *cert = SSL_PeerCertificate(tls->ssl);
  int result = (cert != NULL);
  CERT_DestroyCertificate(cert);
  return result;
}

MOCK_IMPL(tor_x509_cert_t *,
tor_tls_get_peer_cert,(tor_tls_t *tls))
{
  CERTCertificate *cert = SSL_PeerCertificate(tls->ssl);
  if (cert)
    return tor_x509_cert_new(cert);
  else
    return NULL;
}

MOCK_IMPL(tor_x509_cert_t *,
tor_tls_get_own_cert,(tor_tls_t *tls))
{
  tor_assert(tls);
  CERTCertificate *cert = SSL_LocalCertificate(tls->ssl);
  if (cert)
    return tor_x509_cert_new(cert);
  else
    return NULL;
}

MOCK_IMPL(int,
tor_tls_read, (tor_tls_t *tls, char *cp, size_t len))
{
  tor_assert(tls);
  tor_assert(cp);
  tor_assert(len < INT_MAX);

  PRInt32 rv = PR_Read(tls->ssl, cp, (int)len);
  // log_debug(LD_NET, "PR_Read(%zu) returned %d", n, (int)rv);
  if (rv > 0) {
    return rv;
  }
  if (rv == 0)
    return TOR_TLS_CLOSE;
  PRErrorCode err = PORT_GetError();
  if (err == PR_WOULD_BLOCK_ERROR) {
    return TOR_TLS_WANTREAD; // XXXX ????
  } else {
    tls_log_errors(tls, LOG_NOTICE, LD_CRYPTO, "reading"); // XXXX
    return TOR_TLS_ERROR_MISC; // ????
  }
}

int
tor_tls_write(tor_tls_t *tls, const char *cp, size_t n)
{
  tor_assert(tls);
  tor_assert(cp || n == 0);
  tor_assert(n < INT_MAX);

  PRInt32 rv = PR_Write(tls->ssl, cp, (int)n);
  // log_debug(LD_NET, "PR_Write(%zu) returned %d", n, (int)rv);
  if (rv > 0) {
    return rv;
  }
  if (rv == 0)
    return TOR_TLS_ERROR_MISC;
  PRErrorCode err = PORT_GetError();

  if (err == PR_WOULD_BLOCK_ERROR) {
    return TOR_TLS_WANTWRITE; // XXXX ????
  } else {
    tls_log_errors(tls, LOG_NOTICE, LD_CRYPTO, "writing"); // XXXX
    return TOR_TLS_ERROR_MISC; // ????
  }
}

int
tor_tls_handshake(tor_tls_t *tls)
{
  tor_assert(tls);
  tor_assert(tls->state == TOR_TLS_ST_HANDSHAKE);

  SECStatus s = SSL_ForceHandshake(tls->ssl);
  if (s == SECSuccess) {
    tls->state = TOR_TLS_ST_OPEN;
    log_debug(LD_NET, "SSL handshake is supposedly complete.");
    return tor_tls_finish_handshake(tls);
  }
  if (PORT_GetError() == PR_WOULD_BLOCK_ERROR)
    return TOR_TLS_WANTREAD; /* XXXX What about wantwrite? */

  return TOR_TLS_ERROR_MISC; // XXXX
}

int
tor_tls_finish_handshake(tor_tls_t *tls)
{
  tor_assert(tls);
  // We don't need to do any of the weird handshake nonsense stuff on NSS,
  // since we only support recent handshakes.
  return TOR_TLS_DONE;
}

void
tor_tls_unblock_renegotiation(tor_tls_t *tls)
{
  tor_assert(tls);
  /* We don't support renegotiation with NSS. */
}

void
tor_tls_block_renegotiation(tor_tls_t *tls)
{
  tor_assert(tls);
  /* We don't support renegotiation with NSS. */
}

void
tor_tls_assert_renegotiation_unblocked(tor_tls_t *tls)
{
  tor_assert(tls);
  /* We don't support renegotiation with NSS. */
}

int
tor_tls_get_pending_bytes(tor_tls_t *tls)
{
  tor_assert(tls);
  int n = SSL_DataPending(tls->ssl);
  if (n < 0) {
    tls_log_errors(tls, LOG_WARN, LD_CRYPTO, "looking up pending bytes");
    return 0;
  }
  return (int)n;
}

size_t
tor_tls_get_forced_write_size(tor_tls_t *tls)
{
  tor_assert(tls);
  /* NSS doesn't have the same "forced write" restriction as openssl. */
  return 0;
}

void
tor_tls_get_n_raw_bytes(tor_tls_t *tls,
                        size_t *n_read, size_t *n_written)
{
  tor_assert(tls);
  tor_assert(n_read);
  tor_assert(n_written);
  uint64_t r, w;
  if (tor_get_prfiledesc_byte_counts(tls->ssl, &r, &w) < 0) {
    *n_read = *n_written = 0;
    return;
  }

  *n_read = (size_t)(r - tls->last_read_count);
  *n_written = (size_t)(w - tls->last_write_count);

  tls->last_read_count = r;
  tls->last_write_count = w;
}

int
tor_tls_get_buffer_sizes(tor_tls_t *tls,
                         size_t *rbuf_capacity, size_t *rbuf_bytes,
                         size_t *wbuf_capacity, size_t *wbuf_bytes)
{
  tor_assert(tls);
  tor_assert(rbuf_capacity);
  tor_assert(rbuf_bytes);
  tor_assert(wbuf_capacity);
  tor_assert(wbuf_bytes);

  /* This is an acceptable way to say "we can't measure this." */
  return -1;
}

MOCK_IMPL(double,
tls_get_write_overhead_ratio, (void))
{
  /* XXX We don't currently have a way to measure this in NSS; we could do that
   * XXX with a PRIO layer, but it'll take a little coding. */
  return 0.95;
}

int
tor_tls_used_v1_handshake(tor_tls_t *tls)
{
  tor_assert(tls);
  /* We don't support or allow the V1 handshake with NSS.
   */
  return 0;
}

int
tor_tls_server_got_renegotiate(tor_tls_t *tls)
{
  tor_assert(tls);
  return 0; /* We don't support renegotiation with NSS */
}

MOCK_IMPL(int,
tor_tls_cert_matches_key,(const tor_tls_t *tls,
                          const struct tor_x509_cert_t *cert))
{
  tor_assert(tls);
  tor_assert(cert);
  int rv = 0;

  CERTCertificate *peercert = SSL_PeerCertificate(tls->ssl);
  if (!peercert)
    goto done;
  CERTSubjectPublicKeyInfo *peer_info = &peercert->subjectPublicKeyInfo;
  CERTSubjectPublicKeyInfo *cert_info = &cert->cert->subjectPublicKeyInfo;
  rv = SECOID_CompareAlgorithmID(&peer_info->algorithm,
                                 &cert_info->algorithm) == 0 &&
       SECITEM_ItemsAreEqual(&peer_info->subjectPublicKey,
                             &cert_info->subjectPublicKey);

 done:
  if (peercert)
    CERT_DestroyCertificate(peercert);
  return rv;
}

MOCK_IMPL(int,
tor_tls_get_tlssecrets,(tor_tls_t *tls, uint8_t *secrets_out))
{
  tor_assert(tls);
  tor_assert(secrets_out);

  /* There's no way to get this information out of NSS. */

  return -1;
}

MOCK_IMPL(int,
tor_tls_export_key_material,(tor_tls_t *tls, uint8_t *secrets_out,
                             const uint8_t *context,
                             size_t context_len,
                             const char *label))
{
  tor_assert(tls);
  tor_assert(secrets_out);
  tor_assert(context);
  tor_assert(label);
  tor_assert(strlen(label) <= UINT_MAX);
  tor_assert(context_len <= UINT_MAX);

  SECStatus s;
  /* Make sure that the error code is set here, so that we can be sure that
   * any error code set after a failure was in fact caused by
   * SSL_ExportKeyingMaterial. */
  PR_SetError(PR_UNKNOWN_ERROR, 0);
  s = SSL_ExportKeyingMaterial(tls->ssl,
                               label, (unsigned)strlen(label),
                               PR_TRUE, context, (unsigned)context_len,
                               secrets_out, DIGEST256_LEN);
  if (s != SECSuccess) {
    tls_log_errors(tls, LOG_WARN, LD_CRYPTO,
                   "exporting key material for a TLS handshake");
  }

  return (s == SECSuccess) ? 0 : -1;
}

const char *
tor_tls_get_ciphersuite_name(tor_tls_t *tls)
{
  tor_assert(tls);

  SSLChannelInfo channel_info;
  SSLCipherSuiteInfo cipher_info;

  memset(&channel_info, 0, sizeof(channel_info));
  memset(&cipher_info, 0, sizeof(cipher_info));

  SECStatus s = SSL_GetChannelInfo(tls->ssl,
                                   &channel_info, sizeof(channel_info));
  if (s != SECSuccess)
    return NULL;

  s = SSL_GetCipherSuiteInfo(channel_info.cipherSuite,
                             &cipher_info, sizeof(cipher_info));
  if (s != SECSuccess)
    return NULL;

  return cipher_info.cipherSuiteName;
}

/** The group we should use for ecdhe when none was selected. */
#define SEC_OID_TOR_DEFAULT_ECDHE_GROUP SEC_OID_ANSIX962_EC_PRIME256V1

int
evaluate_ecgroup_for_tls(const char *ecgroup)
{
  SECOidTag tag;

  if (!ecgroup)
    tag = SEC_OID_TOR_DEFAULT_ECDHE_GROUP;
  else if (!strcasecmp(ecgroup, "P256"))
    tag = SEC_OID_ANSIX962_EC_PRIME256V1;
  else if (!strcasecmp(ecgroup, "P224"))
    tag = SEC_OID_SECG_EC_SECP224R1;
  else
    return 0;

  /* I don't think we need any additional tests here for NSS */
  (void) tag;

  return 1;
}

static SECStatus
always_accept_cert_cb(void *arg, PRFileDesc *ssl, PRBool checkSig,
                      PRBool isServer)
{
  (void)arg;
  (void)ssl;
  (void)checkSig;
  (void)isServer;
  return SECSuccess;
}
