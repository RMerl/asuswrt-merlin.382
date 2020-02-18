/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file routerparse.c
 * \brief Code to parse and validate router descriptors, consenus directories,
 *   and similar objects.
 *
 * The objects parsed by this module use a common text-based metaformat,
 * documented in dir-spec.txt in torspec.git.  This module is itself divided
 * into two major kinds of function: code to handle the metaformat, and code
 * to convert from particular instances of the metaformat into the
 * objects that Tor uses.
 *
 * The generic parsing code works by calling a table-based tokenizer on the
 * input string.  Each token corresponds to a single line with a token, plus
 * optional arguments on that line, plus an optional base-64 encoded object
 * after that line.  Each token has a definition in a table of token_rule_t
 * entries that describes how many arguments it can take, whether it takes an
 * object, how many times it may appear, whether it must appear first, and so
 * on.
 *
 * The tokenizer function tokenize_string() converts its string input into a
 * smartlist full of instances of directory_token_t, according to a provided
 * table of token_rule_t.
 *
 * The generic parts of this module additionally include functions for
 * finding the start and end of signed information inside a signed object, and
 * computing the digest that will be signed.
 *
 * There are also functions for saving objects to disk that have caused
 * parsing to fail.
 *
 * The specific parts of this module describe conversions between
 * particular lists of directory_token_t and particular objects.  The
 * kinds of objects that can be parsed here are:
 *  <ul>
 *  <li>router descriptors (managed from routerlist.c)
 *  <li>extra-info documents (managed from routerlist.c)
 *  <li>microdescriptors (managed from microdesc.c)
 *  <li>vote and consensus networkstatus documents, and the routerstatus_t
 *    objects that they comprise (managed from networkstatus.c)
 *  <li>detached-signature objects used by authorities for gathering
 *    signatures on the networkstatus consensus (managed from dirvote.c)
 *  <li>authority key certificates (managed from routerlist.c)
 *  <li>hidden service descriptors (managed from rendcommon.c and rendcache.c)
 * </ul>
 **/

#define EXPOSE_ROUTERDESC_TOKEN_TABLE

#include "core/or/or.h"
#include "app/config/config.h"
#include "core/or/policies.h"
#include "core/or/versions.h"
#include "feature/dirparse/parsecommon.h"
#include "feature/dirparse/policy_parse.h"
#include "feature/dirparse/routerparse.h"
#include "feature/dirparse/sigcommon.h"
#include "feature/dirparse/unparseable.h"
#include "feature/nodelist/describe.h"
#include "feature/nodelist/nickname.h"
#include "feature/nodelist/routerinfo.h"
#include "feature/nodelist/routerlist.h"
#include "feature/nodelist/torcert.h"
#include "feature/relay/router.h"
#include "lib/crypt_ops/crypto_curve25519.h"
#include "lib/crypt_ops/crypto_ed25519.h"
#include "lib/crypt_ops/crypto_format.h"
#include "lib/memarea/memarea.h"
#include "lib/sandbox/sandbox.h"

#include "core/or/addr_policy_st.h"
#include "feature/nodelist/extrainfo_st.h"
#include "feature/nodelist/routerinfo_st.h"
#include "feature/nodelist/routerlist_st.h"

/****************************************************************************/

/** List of tokens recognized in router descriptors */
const token_rule_t routerdesc_token_table[] = {
  T0N("reject",              K_REJECT,              ARGS,    NO_OBJ ),
  T0N("accept",              K_ACCEPT,              ARGS,    NO_OBJ ),
  T0N("reject6",             K_REJECT6,             ARGS,    NO_OBJ ),
  T0N("accept6",             K_ACCEPT6,             ARGS,    NO_OBJ ),
  T1_START( "router",        K_ROUTER,              GE(5),   NO_OBJ ),
  T01("ipv6-policy",         K_IPV6_POLICY,         CONCAT_ARGS, NO_OBJ),
  T1( "signing-key",         K_SIGNING_KEY,         NO_ARGS, NEED_KEY_1024 ),
  T1( "onion-key",           K_ONION_KEY,           NO_ARGS, NEED_KEY_1024 ),
  T01("ntor-onion-key",      K_ONION_KEY_NTOR,      GE(1), NO_OBJ ),
  T1_END( "router-signature",    K_ROUTER_SIGNATURE,    NO_ARGS, NEED_OBJ ),
  T1( "published",           K_PUBLISHED,       CONCAT_ARGS, NO_OBJ ),
  T01("uptime",              K_UPTIME,              GE(1),   NO_OBJ ),
  T01("fingerprint",         K_FINGERPRINT,     CONCAT_ARGS, NO_OBJ ),
  T01("hibernating",         K_HIBERNATING,         GE(1),   NO_OBJ ),
  T01("platform",            K_PLATFORM,        CONCAT_ARGS, NO_OBJ ),
  T01("proto",               K_PROTO,           CONCAT_ARGS, NO_OBJ ),
  T01("contact",             K_CONTACT,         CONCAT_ARGS, NO_OBJ ),
  T01("read-history",        K_READ_HISTORY,        ARGS,    NO_OBJ ),
  T01("write-history",       K_WRITE_HISTORY,       ARGS,    NO_OBJ ),
  T01("extra-info-digest",   K_EXTRA_INFO_DIGEST,   GE(1),   NO_OBJ ),
  T01("hidden-service-dir",  K_HIDDEN_SERVICE_DIR,  NO_ARGS, NO_OBJ ),
  T01("identity-ed25519",    K_IDENTITY_ED25519,    NO_ARGS, NEED_OBJ ),
  T01("master-key-ed25519",  K_MASTER_KEY_ED25519,  GE(1),   NO_OBJ ),
  T01("router-sig-ed25519",  K_ROUTER_SIG_ED25519,  GE(1),   NO_OBJ ),
  T01("onion-key-crosscert", K_ONION_KEY_CROSSCERT, NO_ARGS, NEED_OBJ ),
  T01("ntor-onion-key-crosscert", K_NTOR_ONION_KEY_CROSSCERT,
                                                    EQ(1),   NEED_OBJ ),

  T01("allow-single-hop-exits",K_ALLOW_SINGLE_HOP_EXITS,    NO_ARGS, NO_OBJ ),

  T01("family",              K_FAMILY,              ARGS,    NO_OBJ ),
  T01("caches-extra-info",   K_CACHES_EXTRA_INFO,   NO_ARGS, NO_OBJ ),
  T0N("or-address",          K_OR_ADDRESS,          GE(1),   NO_OBJ ),

  T0N("opt",                 K_OPT,             CONCAT_ARGS, OBJ_OK ),
  T1( "bandwidth",           K_BANDWIDTH,           GE(3),   NO_OBJ ),
  A01("@purpose",            A_PURPOSE,             GE(1),   NO_OBJ ),
  T01("tunnelled-dir-server",K_DIR_TUNNELLED,       NO_ARGS, NO_OBJ ),

  END_OF_TABLE
};

/** List of tokens recognized in extra-info documents. */
static token_rule_t extrainfo_token_table[] = {
  T1_END( "router-signature",    K_ROUTER_SIGNATURE,    NO_ARGS, NEED_OBJ ),
  T1( "published",           K_PUBLISHED,       CONCAT_ARGS, NO_OBJ ),
  T01("identity-ed25519",    K_IDENTITY_ED25519,    NO_ARGS, NEED_OBJ ),
  T01("router-sig-ed25519",  K_ROUTER_SIG_ED25519,  GE(1),   NO_OBJ ),
  T0N("opt",                 K_OPT,             CONCAT_ARGS, OBJ_OK ),
  T01("read-history",        K_READ_HISTORY,        ARGS,    NO_OBJ ),
  T01("write-history",       K_WRITE_HISTORY,       ARGS,    NO_OBJ ),
  T01("dirreq-stats-end",    K_DIRREQ_END,          ARGS,    NO_OBJ ),
  T01("dirreq-v2-ips",       K_DIRREQ_V2_IPS,       ARGS,    NO_OBJ ),
  T01("dirreq-v3-ips",       K_DIRREQ_V3_IPS,       ARGS,    NO_OBJ ),
  T01("dirreq-v2-reqs",      K_DIRREQ_V2_REQS,      ARGS,    NO_OBJ ),
  T01("dirreq-v3-reqs",      K_DIRREQ_V3_REQS,      ARGS,    NO_OBJ ),
  T01("dirreq-v2-share",     K_DIRREQ_V2_SHARE,     ARGS,    NO_OBJ ),
  T01("dirreq-v3-share",     K_DIRREQ_V3_SHARE,     ARGS,    NO_OBJ ),
  T01("dirreq-v2-resp",      K_DIRREQ_V2_RESP,      ARGS,    NO_OBJ ),
  T01("dirreq-v3-resp",      K_DIRREQ_V3_RESP,      ARGS,    NO_OBJ ),
  T01("dirreq-v2-direct-dl", K_DIRREQ_V2_DIR,       ARGS,    NO_OBJ ),
  T01("dirreq-v3-direct-dl", K_DIRREQ_V3_DIR,       ARGS,    NO_OBJ ),
  T01("dirreq-v2-tunneled-dl", K_DIRREQ_V2_TUN,     ARGS,    NO_OBJ ),
  T01("dirreq-v3-tunneled-dl", K_DIRREQ_V3_TUN,     ARGS,    NO_OBJ ),
  T01("entry-stats-end",     K_ENTRY_END,           ARGS,    NO_OBJ ),
  T01("entry-ips",           K_ENTRY_IPS,           ARGS,    NO_OBJ ),
  T01("cell-stats-end",      K_CELL_END,            ARGS,    NO_OBJ ),
  T01("cell-processed-cells", K_CELL_PROCESSED,     ARGS,    NO_OBJ ),
  T01("cell-queued-cells",   K_CELL_QUEUED,         ARGS,    NO_OBJ ),
  T01("cell-time-in-queue",  K_CELL_TIME,           ARGS,    NO_OBJ ),
  T01("cell-circuits-per-decile", K_CELL_CIRCS,     ARGS,    NO_OBJ ),
  T01("exit-stats-end",      K_EXIT_END,            ARGS,    NO_OBJ ),
  T01("exit-kibibytes-written", K_EXIT_WRITTEN,     ARGS,    NO_OBJ ),
  T01("exit-kibibytes-read", K_EXIT_READ,           ARGS,    NO_OBJ ),
  T01("exit-streams-opened", K_EXIT_OPENED,         ARGS,    NO_OBJ ),

  T1_START( "extra-info",          K_EXTRA_INFO,          GE(2),   NO_OBJ ),

  END_OF_TABLE
};

#undef T

/* static function prototypes */
static int router_add_exit_policy(routerinfo_t *router,directory_token_t *tok);
static smartlist_t *find_all_exitpolicy(smartlist_t *s);

/** Set <b>digest</b> to the SHA-1 digest of the hash of the first router in
 * <b>s</b>. Return 0 on success, -1 on failure.
 */
int
router_get_router_hash(const char *s, size_t s_len, char *digest)
{
  return router_get_hash_impl(s, s_len, digest,
                              "router ","\nrouter-signature", '\n',
                              DIGEST_SHA1);
}

/** Set <b>digest</b> to the SHA-1 digest of the hash of the <b>s_len</b>-byte
 * extrainfo string at <b>s</b>.  Return 0 on success, -1 on failure. */
int
router_get_extrainfo_hash(const char *s, size_t s_len, char *digest)
{
  return router_get_hash_impl(s, s_len, digest, "extra-info",
                              "\nrouter-signature",'\n', DIGEST_SHA1);
}

/** Helper: move *<b>s_ptr</b> ahead to the next router, the next extra-info,
 * or to the first of the annotations proceeding the next router or
 * extra-info---whichever comes first.  Set <b>is_extrainfo_out</b> to true if
 * we found an extrainfo, or false if found a router. Do not scan beyond
 * <b>eos</b>.  Return -1 if we found nothing; 0 if we found something. */
static int
find_start_of_next_router_or_extrainfo(const char **s_ptr,
                                       const char *eos,
                                       int *is_extrainfo_out)
{
  const char *annotations = NULL;
  const char *s = *s_ptr;

  s = eat_whitespace_eos(s, eos);

  while (s < eos-32) {  /* 32 gives enough room for a the first keyword. */
    /* We're at the start of a line. */
    tor_assert(*s != '\n');

    if (*s == '@' && !annotations) {
      annotations = s;
    } else if (*s == 'r' && !strcmpstart(s, "router ")) {
      *s_ptr = annotations ? annotations : s;
      *is_extrainfo_out = 0;
      return 0;
    } else if (*s == 'e' && !strcmpstart(s, "extra-info ")) {
      *s_ptr = annotations ? annotations : s;
      *is_extrainfo_out = 1;
      return 0;
    }

    if (!(s = memchr(s+1, '\n', eos-(s+1))))
      break;
    s = eat_whitespace_eos(s, eos);
  }
  return -1;
}

/** Given a string *<b>s</b> containing a concatenated sequence of router
 * descriptors (or extra-info documents if <b>want_extrainfo</b> is set),
 * parses them and stores the result in <b>dest</b>. All routers are marked
 * running and valid. Advances *s to a point immediately following the last
 * router entry. Ignore any trailing router entries that are not complete.
 *
 * If <b>saved_location</b> isn't SAVED_IN_CACHE, make a local copy of each
 * descriptor in the signed_descriptor_body field of each routerinfo_t.  If it
 * isn't SAVED_NOWHERE, remember the offset of each descriptor.
 *
 * Returns 0 on success and -1 on failure.  Adds a digest to
 * <b>invalid_digests_out</b> for every entry that was unparseable or
 * invalid. (This may cause duplicate entries.)
 */
int
router_parse_list_from_string(const char **s, const char *eos,
                              smartlist_t *dest,
                              saved_location_t saved_location,
                              int want_extrainfo,
                              int allow_annotations,
                              const char *prepend_annotations,
                              smartlist_t *invalid_digests_out)
{
  routerinfo_t *router;
  extrainfo_t *extrainfo;
  signed_descriptor_t *signed_desc = NULL;
  void *elt;
  const char *end, *start;
  int have_extrainfo;

  tor_assert(s);
  tor_assert(*s);
  tor_assert(dest);

  start = *s;
  if (!eos)
    eos = *s + strlen(*s);

  tor_assert(eos >= *s);

  while (1) {
    char raw_digest[DIGEST_LEN];
    int have_raw_digest = 0;
    int dl_again = 0;
    if (find_start_of_next_router_or_extrainfo(s, eos, &have_extrainfo) < 0)
      break;

    end = tor_memstr(*s, eos-*s, "\nrouter-signature");
    if (end)
      end = tor_memstr(end, eos-end, "\n-----END SIGNATURE-----\n");
    if (end)
      end += strlen("\n-----END SIGNATURE-----\n");

    if (!end)
      break;

    elt = NULL;

    if (have_extrainfo && want_extrainfo) {
      routerlist_t *rl = router_get_routerlist();
      have_raw_digest = router_get_extrainfo_hash(*s, end-*s, raw_digest) == 0;
      extrainfo = extrainfo_parse_entry_from_string(*s, end,
                                       saved_location != SAVED_IN_CACHE,
                                       rl->identity_map, &dl_again);
      if (extrainfo) {
        signed_desc = &extrainfo->cache_info;
        elt = extrainfo;
      }
    } else if (!have_extrainfo && !want_extrainfo) {
      have_raw_digest = router_get_router_hash(*s, end-*s, raw_digest) == 0;
      router = router_parse_entry_from_string(*s, end,
                                              saved_location != SAVED_IN_CACHE,
                                              allow_annotations,
                                              prepend_annotations, &dl_again);
      if (router) {
        log_debug(LD_DIR, "Read router '%s', purpose '%s'",
                  router_describe(router),
                  router_purpose_to_string(router->purpose));
        signed_desc = &router->cache_info;
        elt = router;
      }
    }
    if (! elt && ! dl_again && have_raw_digest && invalid_digests_out) {
      smartlist_add(invalid_digests_out, tor_memdup(raw_digest, DIGEST_LEN));
    }
    if (!elt) {
      *s = end;
      continue;
    }
    if (saved_location != SAVED_NOWHERE) {
      tor_assert(signed_desc);
      signed_desc->saved_location = saved_location;
      signed_desc->saved_offset = *s - start;
    }
    *s = end;
    smartlist_add(dest, elt);
  }

  return 0;
}

/** Try to find an IPv6 OR port in <b>list</b> of directory_token_t's
 * with at least one argument (use GE(1) in setup). If found, store
 * address and port number to <b>addr_out</b> and
 * <b>port_out</b>. Return number of OR ports found. */
int
find_single_ipv6_orport(const smartlist_t *list,
                        tor_addr_t *addr_out,
                        uint16_t *port_out)
{
  int ret = 0;
  tor_assert(list != NULL);
  tor_assert(addr_out != NULL);
  tor_assert(port_out != NULL);

  SMARTLIST_FOREACH_BEGIN(list, directory_token_t *, t) {
    tor_addr_t a;
    maskbits_t bits;
    uint16_t port_min, port_max;
    tor_assert(t->n_args >= 1);
    /* XXXX Prop186 the full spec allows much more than this. */
    if (tor_addr_parse_mask_ports(t->args[0], 0,
                                  &a, &bits, &port_min,
                                  &port_max) == AF_INET6 &&
        bits == 128 &&
        port_min == port_max) {
      /* Okay, this is one we can understand. Use it and ignore
         any potential more addresses in list. */
      tor_addr_copy(addr_out, &a);
      *port_out = port_min;
      ret = 1;
      break;
    }
  } SMARTLIST_FOREACH_END(t);

  return ret;
}

/** Helper function: reads a single router entry from *<b>s</b> ...
 * *<b>end</b>.  Mallocs a new router and returns it if all goes well, else
 * returns NULL.  If <b>cache_copy</b> is true, duplicate the contents of
 * s through end into the signed_descriptor_body of the resulting
 * routerinfo_t.
 *
 * If <b>end</b> is NULL, <b>s</b> must be properly NUL-terminated.
 *
 * If <b>allow_annotations</b>, it's okay to encounter annotations in <b>s</b>
 * before the router; if it's false, reject the router if it's annotated.  If
 * <b>prepend_annotations</b> is set, it should contain some annotations:
 * append them to the front of the router before parsing it, and keep them
 * around when caching the router.
 *
 * Only one of allow_annotations and prepend_annotations may be set.
 *
 * If <b>can_dl_again_out</b> is provided, set *<b>can_dl_again_out</b> to 1
 * if it's okay to try to download a descriptor with this same digest again,
 * and 0 if it isn't.  (It might not be okay to download it again if part of
 * the part covered by the digest is invalid.)
 */
routerinfo_t *
router_parse_entry_from_string(const char *s, const char *end,
                               int cache_copy, int allow_annotations,
                               const char *prepend_annotations,
                               int *can_dl_again_out)
{
  routerinfo_t *router = NULL;
  char digest[128];
  smartlist_t *tokens = NULL, *exit_policy_tokens = NULL;
  directory_token_t *tok;
  struct in_addr in;
  const char *start_of_annotations, *cp, *s_dup = s;
  size_t prepend_len = prepend_annotations ? strlen(prepend_annotations) : 0;
  int ok = 1;
  memarea_t *area = NULL;
  tor_cert_t *ntor_cc_cert = NULL;
  /* Do not set this to '1' until we have parsed everything that we intend to
   * parse that's covered by the hash. */
  int can_dl_again = 0;
  crypto_pk_t *rsa_pubkey = NULL;

  tor_assert(!allow_annotations || !prepend_annotations);

  if (!end) {
    end = s + strlen(s);
  }

  /* point 'end' to a point immediately after the final newline. */
  while (end > s+2 && *(end-1) == '\n' && *(end-2) == '\n')
    --end;

  area = memarea_new();
  tokens = smartlist_new();
  if (prepend_annotations) {
    if (tokenize_string(area,prepend_annotations,NULL,tokens,
                        routerdesc_token_table,TS_NOCHECK)) {
      log_warn(LD_DIR, "Error tokenizing router descriptor (annotations).");
      goto err;
    }
  }

  start_of_annotations = s;
  cp = tor_memstr(s, end-s, "\nrouter ");
  if (!cp) {
    if (end-s < 7 || strcmpstart(s, "router ")) {
      log_warn(LD_DIR, "No router keyword found.");
      goto err;
    }
  } else {
    s = cp+1;
  }

  if (start_of_annotations != s) { /* We have annotations */
    if (allow_annotations) {
      if (tokenize_string(area,start_of_annotations,s,tokens,
                          routerdesc_token_table,TS_NOCHECK)) {
        log_warn(LD_DIR, "Error tokenizing router descriptor (annotations).");
        goto err;
      }
    } else {
      log_warn(LD_DIR, "Found unexpected annotations on router descriptor not "
               "loaded from disk.  Dropping it.");
      goto err;
    }
  }

  if (router_get_router_hash(s, end - s, digest) < 0) {
    log_warn(LD_DIR, "Couldn't compute router hash.");
    goto err;
  }
  {
    int flags = 0;
    if (allow_annotations)
      flags |= TS_ANNOTATIONS_OK;
    if (prepend_annotations)
      flags |= TS_ANNOTATIONS_OK|TS_NO_NEW_ANNOTATIONS;

    if (tokenize_string(area,s,end,tokens,routerdesc_token_table, flags)) {
      log_warn(LD_DIR, "Error tokenizing router descriptor.");
      goto err;
    }
  }

  if (smartlist_len(tokens) < 2) {
    log_warn(LD_DIR, "Impossibly short router descriptor.");
    goto err;
  }

  tok = find_by_keyword(tokens, K_ROUTER);
  const int router_token_pos = smartlist_pos(tokens, tok);
  tor_assert(tok->n_args >= 5);

  router = tor_malloc_zero(sizeof(routerinfo_t));
  router->cert_expiration_time = TIME_MAX;
  router->cache_info.routerlist_index = -1;
  router->cache_info.annotations_len = s-start_of_annotations + prepend_len;
  router->cache_info.signed_descriptor_len = end-s;
  if (cache_copy) {
    size_t len = router->cache_info.signed_descriptor_len +
                 router->cache_info.annotations_len;
    char *signed_body =
      router->cache_info.signed_descriptor_body = tor_malloc(len+1);
    if (prepend_annotations) {
      memcpy(signed_body, prepend_annotations, prepend_len);
      signed_body += prepend_len;
    }
    /* This assertion will always succeed.
     * len == signed_desc_len + annotations_len
     *     == end-s + s-start_of_annotations + prepend_len
     *     == end-start_of_annotations + prepend_len
     * We already wrote prepend_len bytes into the buffer; now we're
     * writing end-start_of_annotations -NM. */
    tor_assert(signed_body+(end-start_of_annotations) ==
               router->cache_info.signed_descriptor_body+len);
    memcpy(signed_body, start_of_annotations, end-start_of_annotations);
    router->cache_info.signed_descriptor_body[len] = '\0';
    tor_assert(strlen(router->cache_info.signed_descriptor_body) == len);
  }
  memcpy(router->cache_info.signed_descriptor_digest, digest, DIGEST_LEN);

  router->nickname = tor_strdup(tok->args[0]);
  if (!is_legal_nickname(router->nickname)) {
    log_warn(LD_DIR,"Router nickname is invalid");
    goto err;
  }
  if (!tor_inet_aton(tok->args[1], &in)) {
    log_warn(LD_DIR,"Router address is not an IP address.");
    goto err;
  }
  router->addr = ntohl(in.s_addr);

  router->or_port =
    (uint16_t) tor_parse_long(tok->args[2],10,0,65535,&ok,NULL);
  if (!ok) {
    log_warn(LD_DIR,"Invalid OR port %s", escaped(tok->args[2]));
    goto err;
  }
  router->dir_port =
    (uint16_t) tor_parse_long(tok->args[4],10,0,65535,&ok,NULL);
  if (!ok) {
    log_warn(LD_DIR,"Invalid dir port %s", escaped(tok->args[4]));
    goto err;
  }

  tok = find_by_keyword(tokens, K_BANDWIDTH);
  tor_assert(tok->n_args >= 3);
  router->bandwidthrate = (int)
    tor_parse_long(tok->args[0],10,1,INT_MAX,&ok,NULL);

  if (!ok) {
    log_warn(LD_DIR, "bandwidthrate %s unreadable or 0. Failing.",
             escaped(tok->args[0]));
    goto err;
  }
  router->bandwidthburst =
    (int) tor_parse_long(tok->args[1],10,0,INT_MAX,&ok,NULL);
  if (!ok) {
    log_warn(LD_DIR, "Invalid bandwidthburst %s", escaped(tok->args[1]));
    goto err;
  }
  router->bandwidthcapacity = (int)
    tor_parse_long(tok->args[2],10,0,INT_MAX,&ok,NULL);
  if (!ok) {
    log_warn(LD_DIR, "Invalid bandwidthcapacity %s", escaped(tok->args[1]));
    goto err;
  }

  if ((tok = find_opt_by_keyword(tokens, A_PURPOSE))) {
    tor_assert(tok->n_args);
    router->purpose = router_purpose_from_string(tok->args[0]);
    if (router->purpose == ROUTER_PURPOSE_UNKNOWN) {
      goto err;
    }
  } else {
    router->purpose = ROUTER_PURPOSE_GENERAL;
  }
  router->cache_info.send_unencrypted =
    (router->purpose == ROUTER_PURPOSE_GENERAL) ? 1 : 0;

  if ((tok = find_opt_by_keyword(tokens, K_UPTIME))) {
    tor_assert(tok->n_args >= 1);
    router->uptime = tor_parse_long(tok->args[0],10,0,LONG_MAX,&ok,NULL);
    if (!ok) {
      log_warn(LD_DIR, "Invalid uptime %s", escaped(tok->args[0]));
      goto err;
    }
  }

  if ((tok = find_opt_by_keyword(tokens, K_HIBERNATING))) {
    tor_assert(tok->n_args >= 1);
    router->is_hibernating
      = (tor_parse_long(tok->args[0],10,0,LONG_MAX,NULL,NULL) != 0);
  }

  tok = find_by_keyword(tokens, K_PUBLISHED);
  tor_assert(tok->n_args == 1);
  if (parse_iso_time(tok->args[0], &router->cache_info.published_on) < 0)
    goto err;

  tok = find_by_keyword(tokens, K_ONION_KEY);
  if (!crypto_pk_public_exponent_ok(tok->key)) {
    log_warn(LD_DIR,
             "Relay's onion key had invalid exponent.");
    goto err;
  }
  router_set_rsa_onion_pkey(tok->key, &router->onion_pkey,
                            &router->onion_pkey_len);
  crypto_pk_free(tok->key);

  if ((tok = find_opt_by_keyword(tokens, K_ONION_KEY_NTOR))) {
    curve25519_public_key_t k;
    tor_assert(tok->n_args >= 1);
    if (curve25519_public_from_base64(&k, tok->args[0]) < 0) {
      log_warn(LD_DIR, "Bogus ntor-onion-key in routerinfo");
      goto err;
    }
    router->onion_curve25519_pkey =
      tor_memdup(&k, sizeof(curve25519_public_key_t));
  }

  tok = find_by_keyword(tokens, K_SIGNING_KEY);
  router->identity_pkey = tok->key;
  tok->key = NULL; /* Prevent free */
  if (crypto_pk_get_digest(router->identity_pkey,
                           router->cache_info.identity_digest)) {
    log_warn(LD_DIR, "Couldn't calculate key digest"); goto err;
  }

  {
    directory_token_t *ed_sig_tok, *ed_cert_tok, *cc_tap_tok, *cc_ntor_tok,
      *master_key_tok;
    ed_sig_tok = find_opt_by_keyword(tokens, K_ROUTER_SIG_ED25519);
    ed_cert_tok = find_opt_by_keyword(tokens, K_IDENTITY_ED25519);
    master_key_tok = find_opt_by_keyword(tokens, K_MASTER_KEY_ED25519);
    cc_tap_tok = find_opt_by_keyword(tokens, K_ONION_KEY_CROSSCERT);
    cc_ntor_tok = find_opt_by_keyword(tokens, K_NTOR_ONION_KEY_CROSSCERT);
    int n_ed_toks = !!ed_sig_tok + !!ed_cert_tok +
      !!cc_tap_tok + !!cc_ntor_tok;
    if ((n_ed_toks != 0 && n_ed_toks != 4) ||
        (n_ed_toks == 4 && !router->onion_curve25519_pkey)) {
      log_warn(LD_DIR, "Router descriptor with only partial ed25519/"
               "cross-certification support");
      goto err;
    }
    if (master_key_tok && !ed_sig_tok) {
      log_warn(LD_DIR, "Router descriptor has ed25519 master key but no "
               "certificate");
      goto err;
    }
    if (ed_sig_tok) {
      tor_assert(ed_cert_tok && cc_tap_tok && cc_ntor_tok);
      const int ed_cert_token_pos = smartlist_pos(tokens, ed_cert_tok);
      if (ed_cert_token_pos == -1 || router_token_pos == -1 ||
          (ed_cert_token_pos != router_token_pos + 1 &&
           ed_cert_token_pos != router_token_pos - 1)) {
        log_warn(LD_DIR, "Ed25519 certificate in wrong position");
        goto err;
      }
      if (ed_sig_tok != smartlist_get(tokens, smartlist_len(tokens)-2)) {
        log_warn(LD_DIR, "Ed25519 signature in wrong position");
        goto err;
      }
      if (strcmp(ed_cert_tok->object_type, "ED25519 CERT")) {
        log_warn(LD_DIR, "Wrong object type on identity-ed25519 in decriptor");
        goto err;
      }
      if (strcmp(cc_ntor_tok->object_type, "ED25519 CERT")) {
        log_warn(LD_DIR, "Wrong object type on ntor-onion-key-crosscert "
                 "in decriptor");
        goto err;
      }
      if (strcmp(cc_tap_tok->object_type, "CROSSCERT")) {
        log_warn(LD_DIR, "Wrong object type on onion-key-crosscert "
                 "in decriptor");
        goto err;
      }
      if (strcmp(cc_ntor_tok->args[0], "0") &&
          strcmp(cc_ntor_tok->args[0], "1")) {
        log_warn(LD_DIR, "Bad sign bit on ntor-onion-key-crosscert");
        goto err;
      }
      int ntor_cc_sign_bit = !strcmp(cc_ntor_tok->args[0], "1");

      uint8_t d256[DIGEST256_LEN];
      const char *signed_start, *signed_end;
      tor_cert_t *cert = tor_cert_parse(
                       (const uint8_t*)ed_cert_tok->object_body,
                       ed_cert_tok->object_size);
      if (! cert) {
        log_warn(LD_DIR, "Couldn't parse ed25519 cert");
        goto err;
      }
      /* makes sure it gets freed. */
      router->cache_info.signing_key_cert = cert;

      if (cert->cert_type != CERT_TYPE_ID_SIGNING ||
          ! cert->signing_key_included) {
        log_warn(LD_DIR, "Invalid form for ed25519 cert");
        goto err;
      }

      if (master_key_tok) {
        /* This token is optional, but if it's present, it must match
         * the signature in the signing cert, or supplant it. */
        tor_assert(master_key_tok->n_args >= 1);
        ed25519_public_key_t pkey;
        if (ed25519_public_from_base64(&pkey, master_key_tok->args[0])<0) {
          log_warn(LD_DIR, "Can't parse ed25519 master key");
          goto err;
        }

        if (fast_memneq(&cert->signing_key.pubkey,
                        pkey.pubkey, ED25519_PUBKEY_LEN)) {
          log_warn(LD_DIR, "Ed25519 master key does not match "
                   "key in certificate");
          goto err;
        }
      }
      ntor_cc_cert = tor_cert_parse((const uint8_t*)cc_ntor_tok->object_body,
                                    cc_ntor_tok->object_size);
      if (!ntor_cc_cert) {
        log_warn(LD_DIR, "Couldn't parse ntor-onion-key-crosscert cert");
        goto err;
      }
      if (ntor_cc_cert->cert_type != CERT_TYPE_ONION_ID ||
          ! ed25519_pubkey_eq(&ntor_cc_cert->signed_key, &cert->signing_key)) {
        log_warn(LD_DIR, "Invalid contents for ntor-onion-key-crosscert cert");
        goto err;
      }

      ed25519_public_key_t ntor_cc_pk;
      if (ed25519_public_key_from_curve25519_public_key(&ntor_cc_pk,
                                            router->onion_curve25519_pkey,
                                            ntor_cc_sign_bit)<0) {
        log_warn(LD_DIR, "Error converting onion key to ed25519");
        goto err;
      }

      if (router_get_hash_impl_helper(s, end-s, "router ",
                                      "\nrouter-sig-ed25519",
                                      ' ', LOG_WARN,
                                      &signed_start, &signed_end) < 0) {
        log_warn(LD_DIR, "Can't find ed25519-signed portion of descriptor");
        goto err;
      }
      crypto_digest_t *d = crypto_digest256_new(DIGEST_SHA256);
      crypto_digest_add_bytes(d, ED_DESC_SIGNATURE_PREFIX,
        strlen(ED_DESC_SIGNATURE_PREFIX));
      crypto_digest_add_bytes(d, signed_start, signed_end-signed_start);
      crypto_digest_get_digest(d, (char*)d256, sizeof(d256));
      crypto_digest_free(d);

      ed25519_checkable_t check[3];
      int check_ok[3];
      time_t expires = TIME_MAX;
      if (tor_cert_get_checkable_sig(&check[0], cert, NULL, &expires) < 0) {
        log_err(LD_BUG, "Couldn't create 'checkable' for cert.");
        goto err;
      }
      if (tor_cert_get_checkable_sig(&check[1],
                               ntor_cc_cert, &ntor_cc_pk, &expires) < 0) {
        log_err(LD_BUG, "Couldn't create 'checkable' for ntor_cc_cert.");
        goto err;
      }

      if (ed25519_signature_from_base64(&check[2].signature,
                                        ed_sig_tok->args[0])<0) {
        log_warn(LD_DIR, "Couldn't decode ed25519 signature");
        goto err;
      }
      check[2].pubkey = &cert->signed_key;
      check[2].msg = d256;
      check[2].len = DIGEST256_LEN;

      if (ed25519_checksig_batch(check_ok, check, 3) < 0) {
        log_warn(LD_DIR, "Incorrect ed25519 signature(s)");
        goto err;
      }

      rsa_pubkey = router_get_rsa_onion_pkey(router->onion_pkey,
                                             router->onion_pkey_len);
      if (check_tap_onion_key_crosscert(
                      (const uint8_t*)cc_tap_tok->object_body,
                      (int)cc_tap_tok->object_size,
                      rsa_pubkey,
                      &cert->signing_key,
                      (const uint8_t*)router->cache_info.identity_digest)<0) {
        log_warn(LD_DIR, "Incorrect TAP cross-verification");
        goto err;
      }

      /* We check this before adding it to the routerlist. */
      router->cert_expiration_time = expires;
    }
  }

  if ((tok = find_opt_by_keyword(tokens, K_FINGERPRINT))) {
    /* If there's a fingerprint line, it must match the identity digest. */
    char d[DIGEST_LEN];
    tor_assert(tok->n_args == 1);
    tor_strstrip(tok->args[0], " ");
    if (base16_decode(d, DIGEST_LEN,
                      tok->args[0], strlen(tok->args[0])) != DIGEST_LEN) {
      log_warn(LD_DIR, "Couldn't decode router fingerprint %s",
               escaped(tok->args[0]));
      goto err;
    }
    if (tor_memneq(d,router->cache_info.identity_digest, DIGEST_LEN)) {
      log_warn(LD_DIR, "Fingerprint '%s' does not match identity digest.",
               tok->args[0]);
      goto err;
    }
  }

  {
    const char *version = NULL, *protocols = NULL;
    if ((tok = find_opt_by_keyword(tokens, K_PLATFORM))) {
      router->platform = tor_strdup(tok->args[0]);
      version = tok->args[0];
    }

    if ((tok = find_opt_by_keyword(tokens, K_PROTO))) {
      router->protocol_list = tor_strdup(tok->args[0]);
      protocols = tok->args[0];
    }

    summarize_protover_flags(&router->pv, protocols, version);
  }

  if ((tok = find_opt_by_keyword(tokens, K_CONTACT))) {
    router->contact_info = tor_strdup(tok->args[0]);
  }

  if (find_opt_by_keyword(tokens, K_REJECT6) ||
      find_opt_by_keyword(tokens, K_ACCEPT6)) {
    log_warn(LD_DIR, "Rejecting router with reject6/accept6 line: they crash "
             "older Tors.");
    goto err;
  }
  {
    smartlist_t *or_addresses = find_all_by_keyword(tokens, K_OR_ADDRESS);
    if (or_addresses) {
      find_single_ipv6_orport(or_addresses, &router->ipv6_addr,
                              &router->ipv6_orport);
      smartlist_free(or_addresses);
    }
  }
  exit_policy_tokens = find_all_exitpolicy(tokens);
  if (!smartlist_len(exit_policy_tokens)) {
    log_warn(LD_DIR, "No exit policy tokens in descriptor.");
    goto err;
  }
  SMARTLIST_FOREACH(exit_policy_tokens, directory_token_t *, t,
                    if (router_add_exit_policy(router,t)<0) {
                      log_warn(LD_DIR,"Error in exit policy");
                      goto err;
                    });
  policy_expand_private(&router->exit_policy);

  if ((tok = find_opt_by_keyword(tokens, K_IPV6_POLICY)) && tok->n_args) {
    router->ipv6_exit_policy = parse_short_policy(tok->args[0]);
    if (! router->ipv6_exit_policy) {
      log_warn(LD_DIR , "Error in ipv6-policy %s", escaped(tok->args[0]));
      goto err;
    }
  }

  if (policy_is_reject_star(router->exit_policy, AF_INET, 1) &&
      (!router->ipv6_exit_policy ||
       short_policy_is_reject_star(router->ipv6_exit_policy)))
    router->policy_is_reject_star = 1;

  if ((tok = find_opt_by_keyword(tokens, K_FAMILY)) && tok->n_args) {
    int i;
    router->declared_family = smartlist_new();
    for (i=0;i<tok->n_args;++i) {
      if (!is_legal_nickname_or_hexdigest(tok->args[i])) {
        log_warn(LD_DIR, "Illegal nickname %s in family line",
                 escaped(tok->args[i]));
        goto err;
      }
      smartlist_add_strdup(router->declared_family, tok->args[i]);
    }
  }

  if (find_opt_by_keyword(tokens, K_CACHES_EXTRA_INFO))
    router->caches_extra_info = 1;

  if (find_opt_by_keyword(tokens, K_ALLOW_SINGLE_HOP_EXITS))
    router->allow_single_hop_exits = 1;

  if ((tok = find_opt_by_keyword(tokens, K_EXTRA_INFO_DIGEST))) {
    tor_assert(tok->n_args >= 1);
    if (strlen(tok->args[0]) == HEX_DIGEST_LEN) {
      if (base16_decode(router->cache_info.extra_info_digest, DIGEST_LEN,
                        tok->args[0], HEX_DIGEST_LEN) != DIGEST_LEN) {
          log_warn(LD_DIR,"Invalid extra info digest");
      }
    } else {
      log_warn(LD_DIR, "Invalid extra info digest %s", escaped(tok->args[0]));
    }

    if (tok->n_args >= 2) {
      if (digest256_from_base64(router->cache_info.extra_info_digest256,
                                tok->args[1]) < 0) {
        log_warn(LD_DIR, "Invalid extra info digest256 %s",
                 escaped(tok->args[1]));
      }
    }
  }

  if (find_opt_by_keyword(tokens, K_HIDDEN_SERVICE_DIR)) {
    router->wants_to_be_hs_dir = 1;
  }

  /* This router accepts tunnelled directory requests via begindir if it has
   * an open dirport or it included "tunnelled-dir-server". */
  if (find_opt_by_keyword(tokens, K_DIR_TUNNELLED) || router->dir_port > 0) {
    router->supports_tunnelled_dir_requests = 1;
  }

  tok = find_by_keyword(tokens, K_ROUTER_SIGNATURE);

  if (!router->or_port) {
    log_warn(LD_DIR,"or_port unreadable or 0. Failing.");
    goto err;
  }

  /* We've checked everything that's covered by the hash. */
  can_dl_again = 1;
  if (check_signature_token(digest, DIGEST_LEN, tok, router->identity_pkey, 0,
                            "router descriptor") < 0)
    goto err;

  if (!router->platform) {
    router->platform = tor_strdup("<unknown>");
  }
  goto done;

 err:
  dump_desc(s_dup, "router descriptor");
  routerinfo_free(router);
  router = NULL;
 done:
  crypto_pk_free(rsa_pubkey);
  tor_cert_free(ntor_cc_cert);
  if (tokens) {
    SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
    smartlist_free(tokens);
  }
  smartlist_free(exit_policy_tokens);
  if (area) {
    DUMP_AREA(area, "routerinfo");
    memarea_drop_all(area);
  }
  if (can_dl_again_out)
    *can_dl_again_out = can_dl_again;
  return router;
}

/** Parse a single extrainfo entry from the string <b>s</b>, ending at
 * <b>end</b>.  (If <b>end</b> is NULL, parse up to the end of <b>s</b>.)  If
 * <b>cache_copy</b> is true, make a copy of the extra-info document in the
 * cache_info fields of the result.  If <b>routermap</b> is provided, use it
 * as a map from router identity to routerinfo_t when looking up signing keys.
 *
 * If <b>can_dl_again_out</b> is provided, set *<b>can_dl_again_out</b> to 1
 * if it's okay to try to download an extrainfo with this same digest again,
 * and 0 if it isn't.  (It might not be okay to download it again if part of
 * the part covered by the digest is invalid.)
 */
extrainfo_t *
extrainfo_parse_entry_from_string(const char *s, const char *end,
                            int cache_copy, struct digest_ri_map_t *routermap,
                            int *can_dl_again_out)
{
  extrainfo_t *extrainfo = NULL;
  char digest[128];
  smartlist_t *tokens = NULL;
  directory_token_t *tok;
  crypto_pk_t *key = NULL;
  routerinfo_t *router = NULL;
  memarea_t *area = NULL;
  const char *s_dup = s;
  /* Do not set this to '1' until we have parsed everything that we intend to
   * parse that's covered by the hash. */
  int can_dl_again = 0;

  if (BUG(s == NULL))
    return NULL;

  if (!end) {
    end = s + strlen(s);
  }

  /* point 'end' to a point immediately after the final newline. */
  while (end > s+2 && *(end-1) == '\n' && *(end-2) == '\n')
    --end;

  if (router_get_extrainfo_hash(s, end-s, digest) < 0) {
    log_warn(LD_DIR, "Couldn't compute router hash.");
    goto err;
  }
  tokens = smartlist_new();
  area = memarea_new();
  if (tokenize_string(area,s,end,tokens,extrainfo_token_table,0)) {
    log_warn(LD_DIR, "Error tokenizing extra-info document.");
    goto err;
  }

  if (smartlist_len(tokens) < 2) {
    log_warn(LD_DIR, "Impossibly short extra-info document.");
    goto err;
  }

  /* XXXX Accept this in position 1 too, and ed identity in position 0. */
  tok = smartlist_get(tokens,0);
  if (tok->tp != K_EXTRA_INFO) {
    log_warn(LD_DIR,"Entry does not start with \"extra-info\"");
    goto err;
  }

  extrainfo = tor_malloc_zero(sizeof(extrainfo_t));
  extrainfo->cache_info.is_extrainfo = 1;
  if (cache_copy)
    extrainfo->cache_info.signed_descriptor_body = tor_memdup_nulterm(s,end-s);
  extrainfo->cache_info.signed_descriptor_len = end-s;
  memcpy(extrainfo->cache_info.signed_descriptor_digest, digest, DIGEST_LEN);
  crypto_digest256((char*)extrainfo->digest256, s, end-s, DIGEST_SHA256);

  tor_assert(tok->n_args >= 2);
  if (!is_legal_nickname(tok->args[0])) {
    log_warn(LD_DIR,"Bad nickname %s on \"extra-info\"",escaped(tok->args[0]));
    goto err;
  }
  strlcpy(extrainfo->nickname, tok->args[0], sizeof(extrainfo->nickname));
  if (strlen(tok->args[1]) != HEX_DIGEST_LEN ||
      base16_decode(extrainfo->cache_info.identity_digest, DIGEST_LEN,
                    tok->args[1], HEX_DIGEST_LEN) != DIGEST_LEN) {
    log_warn(LD_DIR,"Invalid fingerprint %s on \"extra-info\"",
             escaped(tok->args[1]));
    goto err;
  }

  tok = find_by_keyword(tokens, K_PUBLISHED);
  if (parse_iso_time(tok->args[0], &extrainfo->cache_info.published_on)) {
    log_warn(LD_DIR,"Invalid published time %s on \"extra-info\"",
             escaped(tok->args[0]));
    goto err;
  }

  {
    directory_token_t *ed_sig_tok, *ed_cert_tok;
    ed_sig_tok = find_opt_by_keyword(tokens, K_ROUTER_SIG_ED25519);
    ed_cert_tok = find_opt_by_keyword(tokens, K_IDENTITY_ED25519);
    int n_ed_toks = !!ed_sig_tok + !!ed_cert_tok;
    if (n_ed_toks != 0 && n_ed_toks != 2) {
      log_warn(LD_DIR, "Router descriptor with only partial ed25519/"
               "cross-certification support");
      goto err;
    }
    if (ed_sig_tok) {
      tor_assert(ed_cert_tok);
      const int ed_cert_token_pos = smartlist_pos(tokens, ed_cert_tok);
      if (ed_cert_token_pos != 1) {
        /* Accept this in position 0 XXXX */
        log_warn(LD_DIR, "Ed25519 certificate in wrong position");
        goto err;
      }
      if (ed_sig_tok != smartlist_get(tokens, smartlist_len(tokens)-2)) {
        log_warn(LD_DIR, "Ed25519 signature in wrong position");
        goto err;
      }
      if (strcmp(ed_cert_tok->object_type, "ED25519 CERT")) {
        log_warn(LD_DIR, "Wrong object type on identity-ed25519 in decriptor");
        goto err;
      }

      uint8_t d256[DIGEST256_LEN];
      const char *signed_start, *signed_end;
      tor_cert_t *cert = tor_cert_parse(
                       (const uint8_t*)ed_cert_tok->object_body,
                       ed_cert_tok->object_size);
      if (! cert) {
        log_warn(LD_DIR, "Couldn't parse ed25519 cert");
        goto err;
      }
      /* makes sure it gets freed. */
      extrainfo->cache_info.signing_key_cert = cert;

      if (cert->cert_type != CERT_TYPE_ID_SIGNING ||
          ! cert->signing_key_included) {
        log_warn(LD_DIR, "Invalid form for ed25519 cert");
        goto err;
      }

      if (router_get_hash_impl_helper(s, end-s, "extra-info ",
                                      "\nrouter-sig-ed25519",
                                      ' ', LOG_WARN,
                                      &signed_start, &signed_end) < 0) {
        log_warn(LD_DIR, "Can't find ed25519-signed portion of extrainfo");
        goto err;
      }
      crypto_digest_t *d = crypto_digest256_new(DIGEST_SHA256);
      crypto_digest_add_bytes(d, ED_DESC_SIGNATURE_PREFIX,
        strlen(ED_DESC_SIGNATURE_PREFIX));
      crypto_digest_add_bytes(d, signed_start, signed_end-signed_start);
      crypto_digest_get_digest(d, (char*)d256, sizeof(d256));
      crypto_digest_free(d);

      ed25519_checkable_t check[2];
      int check_ok[2];
      if (tor_cert_get_checkable_sig(&check[0], cert, NULL, NULL) < 0) {
        log_err(LD_BUG, "Couldn't create 'checkable' for cert.");
        goto err;
      }

      if (ed25519_signature_from_base64(&check[1].signature,
                                        ed_sig_tok->args[0])<0) {
        log_warn(LD_DIR, "Couldn't decode ed25519 signature");
        goto err;
      }
      check[1].pubkey = &cert->signed_key;
      check[1].msg = d256;
      check[1].len = DIGEST256_LEN;

      if (ed25519_checksig_batch(check_ok, check, 2) < 0) {
        log_warn(LD_DIR, "Incorrect ed25519 signature(s)");
        goto err;
      }
      /* We don't check the certificate expiration time: checking that it
       * matches the cert in the router descriptor is adequate. */
    }
  }

  /* We've checked everything that's covered by the hash. */
  can_dl_again = 1;

  if (routermap &&
      (router = digestmap_get((digestmap_t*)routermap,
                              extrainfo->cache_info.identity_digest))) {
    key = router->identity_pkey;
  }

  tok = find_by_keyword(tokens, K_ROUTER_SIGNATURE);
  if (strcmp(tok->object_type, "SIGNATURE") ||
      tok->object_size < 128 || tok->object_size > 512) {
    log_warn(LD_DIR, "Bad object type or length on extra-info signature");
    goto err;
  }

  if (key) {
    if (check_signature_token(digest, DIGEST_LEN, tok, key, 0,
                              "extra-info") < 0)
      goto err;

    if (router)
      extrainfo->cache_info.send_unencrypted =
        router->cache_info.send_unencrypted;
  } else {
    extrainfo->pending_sig = tor_memdup(tok->object_body,
                                        tok->object_size);
    extrainfo->pending_sig_len = tok->object_size;
  }

  goto done;
 err:
  dump_desc(s_dup, "extra-info descriptor");
  extrainfo_free(extrainfo);
  extrainfo = NULL;
 done:
  if (tokens) {
    SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
    smartlist_free(tokens);
  }
  if (area) {
    DUMP_AREA(area, "extrainfo");
    memarea_drop_all(area);
  }
  if (can_dl_again_out)
    *can_dl_again_out = can_dl_again;
  return extrainfo;
}

/** Add an exit policy stored in the token <b>tok</b> to the router info in
 * <b>router</b>.  Return 0 on success, -1 on failure. */
static int
router_add_exit_policy(routerinfo_t *router, directory_token_t *tok)
{
  addr_policy_t *newe;
  /* Use the standard interpretation of accept/reject *, an IPv4 wildcard. */
  newe = router_parse_addr_policy(tok, 0);
  if (!newe)
    return -1;
  if (! router->exit_policy)
    router->exit_policy = smartlist_new();

  /* Ensure that in descriptors, accept/reject fields are followed by
   * IPv4 addresses, and accept6/reject6 fields are followed by
   * IPv6 addresses. Unlike torrcs, descriptor exit policies do not permit
   * accept/reject followed by IPv6. */
  if (((tok->tp == K_ACCEPT6 || tok->tp == K_REJECT6) &&
       tor_addr_family(&newe->addr) == AF_INET)
      ||
      ((tok->tp == K_ACCEPT || tok->tp == K_REJECT) &&
       tor_addr_family(&newe->addr) == AF_INET6)) {
    /* There's nothing the user can do about other relays' descriptors,
     * so we don't provide usage advice here. */
    log_warn(LD_DIR, "Mismatch between field type and address type in exit "
             "policy '%s'. Discarding entire router descriptor.",
             tok->n_args == 1 ? tok->args[0] : "");
    addr_policy_free(newe);
    return -1;
  }

  smartlist_add(router->exit_policy, newe);

  return 0;
}

/** Return a newly allocated smartlist of all accept or reject tokens in
 * <b>s</b>.
 */
static smartlist_t *
find_all_exitpolicy(smartlist_t *s)
{
  smartlist_t *out = smartlist_new();
  SMARTLIST_FOREACH(s, directory_token_t *, t,
      if (t->tp == K_ACCEPT || t->tp == K_ACCEPT6 ||
          t->tp == K_REJECT || t->tp == K_REJECT6)
        smartlist_add(out,t));
  return out;
}

/** Called on startup; right now we just handle scanning the unparseable
 * descriptor dumps, but hang anything else we might need to do in the
 * future here as well.
 */
void
routerparse_init(void)
{
  /*
   * Check both if the sandbox is active and whether it's configured; no
   * point in loading all that if we won't be able to use it after the
   * sandbox becomes active.
   */
  if (!(sandbox_is_active() || get_options()->Sandbox)) {
    dump_desc_init();
  }
}

/** Clean up all data structures used by routerparse.c at exit */
void
routerparse_free_all(void)
{
  dump_desc_fifo_cleanup();
}
