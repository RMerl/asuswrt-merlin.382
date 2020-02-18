/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file bwauth.c
 * \brief Code to read and apply bandwidth authority data.
 **/

#define BWAUTH_PRIVATE
#include "core/or/or.h"
#include "feature/dirauth/bwauth.h"

#include "app/config/config.h"
#include "feature/nodelist/networkstatus.h"
#include "feature/nodelist/routerlist.h"
#include "feature/dirparse/ns_parse.h"

#include "feature/nodelist/routerinfo_st.h"
#include "feature/nodelist/vote_routerstatus_st.h"

#include "lib/encoding/keyval.h"

/** Total number of routers with measured bandwidth; this is set by
 * dirserv_count_measured_bs() before the loop in
 * dirserv_generate_networkstatus_vote_obj() and checked by
 * dirserv_get_credible_bandwidth() and
 * dirserv_compute_performance_thresholds() */
static int routers_with_measured_bw = 0;

/** Look through the routerlist, and using the measured bandwidth cache count
 * how many measured bandwidths we know.  This is used to decide whether we
 * ever trust advertised bandwidths for purposes of assigning flags. */
void
dirserv_count_measured_bws(const smartlist_t *routers)
{
  /* Initialize this first */
  routers_with_measured_bw = 0;

  /* Iterate over the routerlist and count measured bandwidths */
  SMARTLIST_FOREACH_BEGIN(routers, const routerinfo_t *, ri) {
    /* Check if we know a measured bandwidth for this one */
    if (dirserv_has_measured_bw(ri->cache_info.identity_digest)) {
      ++routers_with_measured_bw;
    }
  } SMARTLIST_FOREACH_END(ri);
}

/** Return the last-computed result from dirserv_count_mesured_bws(). */
int
dirserv_get_last_n_measured_bws(void)
{
  return routers_with_measured_bw;
}

/** Measured bandwidth cache entry */
typedef struct mbw_cache_entry_s {
  long mbw_kb;
  time_t as_of;
} mbw_cache_entry_t;

/** Measured bandwidth cache - keys are identity_digests, values are
 * mbw_cache_entry_t *. */
static digestmap_t *mbw_cache = NULL;

/** Store a measured bandwidth cache entry when reading the measured
 * bandwidths file. */
STATIC void
dirserv_cache_measured_bw(const measured_bw_line_t *parsed_line,
                          time_t as_of)
{
  mbw_cache_entry_t *e = NULL;

  tor_assert(parsed_line);

  /* Allocate a cache if we need */
  if (!mbw_cache) mbw_cache = digestmap_new();

  /* Check if we have an existing entry */
  e = digestmap_get(mbw_cache, parsed_line->node_id);
  /* If we do, we can re-use it */
  if (e) {
    /* Check that we really are newer, and update */
    if (as_of > e->as_of) {
      e->mbw_kb = parsed_line->bw_kb;
      e->as_of = as_of;
    }
  } else {
    /* We'll have to insert a new entry */
    e = tor_malloc(sizeof(*e));
    e->mbw_kb = parsed_line->bw_kb;
    e->as_of = as_of;
    digestmap_set(mbw_cache, parsed_line->node_id, e);
  }
}

/** Clear and free the measured bandwidth cache */
void
dirserv_clear_measured_bw_cache(void)
{
  if (mbw_cache) {
    /* Free the map and all entries */
    digestmap_free(mbw_cache, tor_free_);
    mbw_cache = NULL;
  }
}

/** Scan the measured bandwidth cache and remove expired entries */
STATIC void
dirserv_expire_measured_bw_cache(time_t now)
{

  if (mbw_cache) {
    /* Iterate through the cache and check each entry */
    DIGESTMAP_FOREACH_MODIFY(mbw_cache, k, mbw_cache_entry_t *, e) {
      if (now > e->as_of + MAX_MEASUREMENT_AGE) {
        tor_free(e);
        MAP_DEL_CURRENT(k);
      }
    } DIGESTMAP_FOREACH_END;

    /* Check if we cleared the whole thing and free if so */
    if (digestmap_size(mbw_cache) == 0) {
      digestmap_free(mbw_cache, tor_free_);
      mbw_cache = 0;
    }
  }
}

/** Query the cache by identity digest, return value indicates whether
 * we found it. The bw_out and as_of_out pointers receive the cached
 * bandwidth value and the time it was cached if not NULL. */
int
dirserv_query_measured_bw_cache_kb(const char *node_id, long *bw_kb_out,
                                   time_t *as_of_out)
{
  mbw_cache_entry_t *v = NULL;
  int rv = 0;

  if (mbw_cache && node_id) {
    v = digestmap_get(mbw_cache, node_id);
    if (v) {
      /* Found something */
      rv = 1;
      if (bw_kb_out) *bw_kb_out = v->mbw_kb;
      if (as_of_out) *as_of_out = v->as_of;
    }
  }

  return rv;
}

/** Predicate wrapper for dirserv_query_measured_bw_cache() */
int
dirserv_has_measured_bw(const char *node_id)
{
  return dirserv_query_measured_bw_cache_kb(node_id, NULL, NULL);
}

/** Get the current size of the measured bandwidth cache */
int
dirserv_get_measured_bw_cache_size(void)
{
  if (mbw_cache) return digestmap_size(mbw_cache);
  else return 0;
}

/** Return the bandwidth we believe for assigning flags; prefer measured
 * over advertised, and if we have above a threshold quantity of measured
 * bandwidths, we don't want to ever give flags to unmeasured routers, so
 * return 0. */
uint32_t
dirserv_get_credible_bandwidth_kb(const routerinfo_t *ri)
{
  int threshold;
  uint32_t bw_kb = 0;
  long mbw_kb;

  tor_assert(ri);
  /* Check if we have a measured bandwidth, and check the threshold if not */
  if (!(dirserv_query_measured_bw_cache_kb(ri->cache_info.identity_digest,
                                       &mbw_kb, NULL))) {
    threshold = get_options()->MinMeasuredBWsForAuthToIgnoreAdvertised;
    if (routers_with_measured_bw > threshold) {
      /* Return zero for unmeasured bandwidth if we are above threshold */
      bw_kb = 0;
    } else {
      /* Return an advertised bandwidth otherwise */
      bw_kb = router_get_advertised_bandwidth_capped(ri) / 1000;
    }
  } else {
    /* We have the measured bandwidth in mbw */
    bw_kb = (uint32_t)mbw_kb;
  }

  return bw_kb;
}

/**
 * Read the measured bandwidth list file, apply it to the list of
 * vote_routerstatus_t and store all the headers in <b>bw_file_headers</b>.
 * Returns -1 on error, 0 otherwise.
 */
int
dirserv_read_measured_bandwidths(const char *from_file,
                                 smartlist_t *routerstatuses,
                                 smartlist_t *bw_file_headers)
{
  FILE *fp = tor_fopen_cloexec(from_file, "r");
  int applied_lines = 0;
  time_t file_time, now;
  int ok;
   /* This flag will be 1 only when the first successful bw measurement line
   * has been encountered, so that measured_bw_line_parse don't give warnings
   * if there are additional header lines, as introduced in Bandwidth List spec
   * version 1.1.0 */
  int line_is_after_headers = 0;
  int rv = -1;
  char *line = NULL;
  size_t n = 0;

  /* Initialise line, so that we can't possibly run off the end. */

  if (fp == NULL) {
    log_warn(LD_CONFIG, "Can't open bandwidth file at configured location: %s",
             from_file);
    goto err;
  }

  /* If fgets fails, line is either unmodified, or indeterminate. */
  if (tor_getline(&line,&n,fp) <= 0) {
    log_warn(LD_DIRSERV, "Empty bandwidth file");
    goto err;
  }

  if (!strlen(line) || line[strlen(line)-1] != '\n') {
    log_warn(LD_DIRSERV, "Long or truncated time in bandwidth file: %s",
             escaped(line));
    goto err;
  }

  line[strlen(line)-1] = '\0';
  file_time = (time_t)tor_parse_ulong(line, 10, 0, ULONG_MAX, &ok, NULL);
  if (!ok) {
    log_warn(LD_DIRSERV, "Non-integer time in bandwidth file: %s",
             escaped(line));
    goto err;
  }

  now = time(NULL);
  if ((now - file_time) > MAX_MEASUREMENT_AGE) {
    log_warn(LD_DIRSERV, "Bandwidth measurement file stale. Age: %u",
             (unsigned)(time(NULL) - file_time));
    goto err;
  }

  /* If timestamp was correct and bw_file_headers is not NULL,
   * add timestamp to bw_file_headers */
  if (bw_file_headers)
    smartlist_add_asprintf(bw_file_headers, "timestamp=%lu",
                           (unsigned long)file_time);

  if (routerstatuses)
    smartlist_sort(routerstatuses, compare_vote_routerstatus_entries);

  while (!feof(fp)) {
    measured_bw_line_t parsed_line;
    if (tor_getline(&line, &n, fp) >= 0) {
      if (measured_bw_line_parse(&parsed_line, line,
                                 line_is_after_headers) != -1) {
        /* This condition will be true when the first complete valid bw line
         * has been encountered, which means the end of the header lines. */
        line_is_after_headers = 1;
        /* Also cache the line for dirserv_get_bandwidth_for_router() */
        dirserv_cache_measured_bw(&parsed_line, file_time);
        if (measured_bw_line_apply(&parsed_line, routerstatuses) > 0)
          applied_lines++;
      /* if the terminator is found, it is the end of header lines, set the
       * flag but do not store anything */
      } else if (strcmp(line, BW_FILE_HEADERS_TERMINATOR) == 0) {
        line_is_after_headers = 1;
      /* if the line was not a correct relay line nor the terminator and
       * the end of the header lines has not been detected yet
       * and it is key_value and bw_file_headers did not reach the maximum
       * number of headers,
       * then assume this line is a header and add it to bw_file_headers */
      } else if (bw_file_headers &&
              (line_is_after_headers == 0) &&
              string_is_key_value(LOG_DEBUG, line) &&
              !strchr(line, ' ') &&
              (smartlist_len(bw_file_headers)
               < MAX_BW_FILE_HEADER_COUNT_IN_VOTE)) {
        line[strlen(line)-1] = '\0';
        smartlist_add_strdup(bw_file_headers, line);
      };
    }
  }

  /* Now would be a nice time to clean the cache, too */
  dirserv_expire_measured_bw_cache(now);

  log_info(LD_DIRSERV,
           "Bandwidth measurement file successfully read. "
           "Applied %d measurements.", applied_lines);
  rv = 0;

 err:
  if (line) {
    // we need to raw_free this buffer because we got it from tor_getdelim()
    raw_free(line);
  }
  if (fp)
    fclose(fp);
  return rv;
}

/**
 * Helper function to parse out a line in the measured bandwidth file
 * into a measured_bw_line_t output structure.
 *
 * If <b>line_is_after_headers</b> is true, then if we encounter an incomplete
 * bw line, return -1 and warn, since we are after the headers and we should
 * only parse bw lines. Return 0 otherwise.
 *
 * If <b>line_is_after_headers</b> is false then it means that we are not past
 * the header block yet. If we encounter an incomplete bw line, return -1 but
 * don't warn since there could be additional header lines coming. If we
 * encounter a proper bw line, return 0 (and we got past the headers).
 */
STATIC int
measured_bw_line_parse(measured_bw_line_t *out, const char *orig_line,
                       int line_is_after_headers)
{
  char *line = tor_strdup(orig_line);
  char *cp = line;
  int got_bw = 0;
  int got_node_id = 0;
  char *strtok_state; /* lame sauce d'jour */

  if (strlen(line) == 0) {
    log_warn(LD_DIRSERV, "Empty line in bandwidth file");
    tor_free(line);
    return -1;
  }

  /* Remove end of line character, so that is not part of the token */
  if (line[strlen(line) - 1] == '\n') {
    line[strlen(line) - 1] = '\0';
  }

  cp = tor_strtok_r(cp, " \t", &strtok_state);

  if (!cp) {
    log_warn(LD_DIRSERV, "Invalid line in bandwidth file: %s",
             escaped(orig_line));
    tor_free(line);
    return -1;
  }

  if (orig_line[strlen(orig_line)-1] != '\n') {
    log_warn(LD_DIRSERV, "Incomplete line in bandwidth file: %s",
             escaped(orig_line));
    tor_free(line);
    return -1;
  }

  do {
    // If the line contains vote=0, ignore it.
    if (strcmpstart(cp, "vote=0") == 0) {
      log_debug(LD_DIRSERV, "Ignoring bandwidth file line that contains "
                "vote=0: %s",escaped(orig_line));
      tor_free(line);
      return -1;
    } else if (strcmpstart(cp, "bw=") == 0) {
      int parse_ok = 0;
      char *endptr;
      if (got_bw) {
        log_warn(LD_DIRSERV, "Double bw= in bandwidth file line: %s",
                 escaped(orig_line));
        tor_free(line);
        return -1;
      }
      cp+=strlen("bw=");

      out->bw_kb = tor_parse_long(cp, 10, 0, LONG_MAX, &parse_ok, &endptr);
      if (!parse_ok || (*endptr && !TOR_ISSPACE(*endptr))) {
        log_warn(LD_DIRSERV, "Invalid bandwidth in bandwidth file line: %s",
                 escaped(orig_line));
        tor_free(line);
        return -1;
      }
      got_bw=1;
    } else if (strcmpstart(cp, "node_id=$") == 0) {
      if (got_node_id) {
        log_warn(LD_DIRSERV, "Double node_id= in bandwidth file line: %s",
                 escaped(orig_line));
        tor_free(line);
        return -1;
      }
      cp+=strlen("node_id=$");

      if (strlen(cp) != HEX_DIGEST_LEN ||
          base16_decode(out->node_id, DIGEST_LEN,
                        cp, HEX_DIGEST_LEN) != DIGEST_LEN) {
        log_warn(LD_DIRSERV, "Invalid node_id in bandwidth file line: %s",
                 escaped(orig_line));
        tor_free(line);
        return -1;
      }
      strlcpy(out->node_hex, cp, sizeof(out->node_hex));
      got_node_id=1;
    }
  } while ((cp = tor_strtok_r(NULL, " \t", &strtok_state)));

  if (got_bw && got_node_id) {
    tor_free(line);
    return 0;
  } else if (line_is_after_headers == 0) {
    /* There could be additional header lines, therefore do not give warnings
     * but returns -1 since it's not a complete bw line. */
    log_debug(LD_DIRSERV, "Missing bw or node_id in bandwidth file line: %s",
             escaped(orig_line));
    tor_free(line);
    return -1;
  } else {
    log_warn(LD_DIRSERV, "Incomplete line in bandwidth file: %s",
             escaped(orig_line));
    tor_free(line);
    return -1;
  }
}

/**
 * Helper function to apply a parsed measurement line to a list
 * of bandwidth statuses. Returns true if a line is found,
 * false otherwise.
 */
STATIC int
measured_bw_line_apply(measured_bw_line_t *parsed_line,
                       smartlist_t *routerstatuses)
{
  vote_routerstatus_t *rs = NULL;
  if (!routerstatuses)
    return 0;

  rs = smartlist_bsearch(routerstatuses, parsed_line->node_id,
                         compare_digest_to_vote_routerstatus_entry);

  if (rs) {
    rs->has_measured_bw = 1;
    rs->measured_bw_kb = (uint32_t)parsed_line->bw_kb;
  } else {
    log_info(LD_DIRSERV, "Node ID %s not found in routerstatus list",
             parsed_line->node_hex);
  }

  return rs != NULL;
}
