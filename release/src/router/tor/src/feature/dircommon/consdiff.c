/* Copyright (c) 2014, Daniel Martí
 * Copyright (c) 2014-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file consdiff.c
 * \brief Consensus diff implementation, including both the generation and the
 * application of diffs in a minimal ed format.
 *
 * The consensus diff application is done in consdiff_apply_diff, which relies
 * on apply_ed_diff for the main ed diff part and on some digest helper
 * functions to check the digest hashes found in the consensus diff header.
 *
 * The consensus diff generation is more complex. consdiff_gen_diff generates
 * it, relying on gen_ed_diff to generate the ed diff and some digest helper
 * functions to generate the digest hashes.
 *
 * gen_ed_diff is the tricky bit. In it simplest form, it will take quadratic
 * time and linear space to generate an ed diff given two smartlists. As shown
 * in its comment section, calling calc_changes on the entire two consensuses
 * will calculate what is to be added and what is to be deleted in the diff.
 * Its comment section briefly explains how it works.
 *
 * In our case specific to consensuses, we take advantage of the fact that
 * consensuses list routers sorted by their identities. We use that
 * information to avoid running calc_changes on the whole smartlists.
 * gen_ed_diff will navigate through the two consensuses identity by identity
 * and will send small couples of slices to calc_changes, keeping the running
 * time near-linear. This is explained in more detail in the gen_ed_diff
 * comments.
 *
 * The allocation strategy tries to save time and memory by avoiding needless
 * copies.  Instead of actually splitting the inputs into separate strings, we
 * allocate cdline_t objects, each of which represents a line in the original
 * object or in the output.  We use memarea_t allocators to manage the
 * temporary memory we use when generating or applying diffs.
 **/

#define CONSDIFF_PRIVATE

#include "core/or/or.h"
#include "feature/dircommon/consdiff.h"
#include "lib/memarea/memarea.h"
#include "feature/dirparse/ns_parse.h"

static const char* ns_diff_version = "network-status-diff-version 1";
static const char* hash_token = "hash";

static char *consensus_join_lines(const smartlist_t *inp);

/** Return true iff a and b have the same contents. */
STATIC int
lines_eq(const cdline_t *a, const cdline_t *b)
{
  return a->len == b->len && fast_memeq(a->s, b->s, a->len);
}

/** Return true iff a has the same contents as the nul-terminated string b. */
STATIC int
line_str_eq(const cdline_t *a, const char *b)
{
  const size_t len = strlen(b);
  tor_assert(len <= UINT32_MAX);
  cdline_t bline = { b, (uint32_t)len };
  return lines_eq(a, &bline);
}

/** Return true iff a begins with the same contents as the nul-terminated
 * string b. */
static int
line_starts_with_str(const cdline_t *a, const char *b)
{
  const size_t len = strlen(b);
  tor_assert(len <= UINT32_MAX);
  return a->len >= len && fast_memeq(a->s, b, len);
}

/** Return a new cdline_t holding as its contents the nul-terminated
 * string s. Use the provided memory area for storage. */
static cdline_t *
cdline_linecpy(memarea_t *area, const char *s)
{
  size_t len = strlen(s);
  const char *ss = memarea_memdup(area, s, len);
  cdline_t *line = memarea_alloc(area, sizeof(cdline_t));
  line->s = ss;
  line->len = (uint32_t)len;
  return line;
}

/** Add a cdline_t to <b>lst</b> holding as its contents the nul-terminated
 * string s.  Use the provided memory area for storage. */
STATIC void
smartlist_add_linecpy(smartlist_t *lst, memarea_t *area, const char *s)
{
  smartlist_add(lst, cdline_linecpy(area, s));
}

/** Compute the digest of <b>cons</b>, and store the result in
 * <b>digest_out</b>. Return 0 on success, -1 on failure. */
/* This is a separate, mockable function so that we can override it when
 * fuzzing. */
MOCK_IMPL(STATIC int,
consensus_compute_digest,(const char *cons,
                          consensus_digest_t *digest_out))
{
  int r = crypto_digest256((char*)digest_out->sha3_256,
                           cons, strlen(cons), DIGEST_SHA3_256);
  return r;
}

/** Compute the digest-as-signed of <b>cons</b>, and store the result in
 * <b>digest_out</b>. Return 0 on success, -1 on failure. */
/* This is a separate, mockable function so that we can override it when
 * fuzzing. */
MOCK_IMPL(STATIC int,
consensus_compute_digest_as_signed,(const char *cons,
                                    consensus_digest_t *digest_out))
{
  return router_get_networkstatus_v3_sha3_as_signed(digest_out->sha3_256,
                                                    cons);
}

/** Return true iff <b>d1</b> and <b>d2</b> contain the same digest */
/* This is a separate, mockable function so that we can override it when
 * fuzzing. */
MOCK_IMPL(STATIC int,
consensus_digest_eq,(const uint8_t *d1,
                     const uint8_t *d2))
{
  return fast_memeq(d1, d2, DIGEST256_LEN);
}

/** Create (allocate) a new slice from a smartlist. Assumes that the start
 * and the end indexes are within the bounds of the initial smartlist. The end
 * element is not part of the resulting slice. If end is -1, the slice is to
 * reach the end of the smartlist.
 */
STATIC smartlist_slice_t *
smartlist_slice(const smartlist_t *list, int start, int end)
{
  int list_len = smartlist_len(list);
  tor_assert(start >= 0);
  tor_assert(start <= list_len);
  if (end == -1) {
    end = list_len;
  }
  tor_assert(start <= end);

  smartlist_slice_t *slice = tor_malloc(sizeof(smartlist_slice_t));
  slice->list = list;
  slice->offset = start;
  slice->len = end - start;
  return slice;
}

/** Helper: Compute the longest common subsequence lengths for the two slices.
 * Used as part of the diff generation to find the column at which to split
 * slice2 while still having the optimal solution.
 * If direction is -1, the navigation is reversed. Otherwise it must be 1.
 * The length of the resulting integer array is that of the second slice plus
 * one.
 */
STATIC int *
lcs_lengths(const smartlist_slice_t *slice1, const smartlist_slice_t *slice2,
            int direction)
{
  size_t a_size = sizeof(int) * (slice2->len+1);

  /* Resulting lcs lengths. */
  int *result = tor_malloc_zero(a_size);
  /* Copy of the lcs lengths from the last iteration. */
  int *prev = tor_malloc(a_size);

  tor_assert(direction == 1 || direction == -1);

  int si = slice1->offset;
  if (direction == -1) {
    si += (slice1->len-1);
  }

  for (int i = 0; i < slice1->len; ++i, si+=direction) {

    const cdline_t *line1 = smartlist_get(slice1->list, si);
    /* Store the last results. */
    memcpy(prev, result, a_size);

    int sj = slice2->offset;
    if (direction == -1) {
      sj += (slice2->len-1);
    }

    for (int j = 0; j < slice2->len; ++j, sj+=direction) {

      const cdline_t *line2 = smartlist_get(slice2->list, sj);
      if (lines_eq(line1, line2)) {
        /* If the lines are equal, the lcs is one line longer. */
        result[j + 1] = prev[j] + 1;
      } else {
        /* If not, see what lcs parent path is longer. */
        result[j + 1] = MAX(result[j], prev[j + 1]);
      }
    }
  }
  tor_free(prev);
  return result;
}

/** Helper: Trim any number of lines that are equally at the start or the end
 * of both slices.
 */
STATIC void
trim_slices(smartlist_slice_t *slice1, smartlist_slice_t *slice2)
{
  while (slice1->len>0 && slice2->len>0) {
    const cdline_t *line1 = smartlist_get(slice1->list, slice1->offset);
    const cdline_t *line2 = smartlist_get(slice2->list, slice2->offset);
    if (!lines_eq(line1, line2)) {
      break;
    }
    slice1->offset++; slice1->len--;
    slice2->offset++; slice2->len--;
  }

  int i1 = (slice1->offset+slice1->len)-1;
  int i2 = (slice2->offset+slice2->len)-1;

  while (slice1->len>0 && slice2->len>0) {
    const cdline_t *line1 = smartlist_get(slice1->list, i1);
    const cdline_t *line2 = smartlist_get(slice2->list, i2);
    if (!lines_eq(line1, line2)) {
      break;
    }
    i1--;
    slice1->len--;
    i2--;
    slice2->len--;
  }
}

/** Like smartlist_string_pos, but uses a cdline_t, and is restricted to the
 * bounds of the slice.
 */
STATIC int
smartlist_slice_string_pos(const smartlist_slice_t *slice,
                           const cdline_t *string)
{
  int end = slice->offset + slice->len;
  for (int i = slice->offset; i < end; ++i) {
    const cdline_t *el = smartlist_get(slice->list, i);
    if (lines_eq(el, string)) {
      return i;
    }
  }
  return -1;
}

/** Helper: Set all the appropriate changed booleans to true. The first slice
 * must be of length 0 or 1. All the lines of slice1 and slice2 which are not
 * present in the other slice will be set to changed in their bool array.
 * The two changed bool arrays are passed in the same order as the slices.
 */
STATIC void
set_changed(bitarray_t *changed1, bitarray_t *changed2,
            const smartlist_slice_t *slice1, const smartlist_slice_t *slice2)
{
  int toskip = -1;
  tor_assert(slice1->len == 0 || slice1->len == 1);

  if (slice1->len == 1) {
    const cdline_t *line_common = smartlist_get(slice1->list, slice1->offset);
    toskip = smartlist_slice_string_pos(slice2, line_common);
    if (toskip == -1) {
      bitarray_set(changed1, slice1->offset);
    }
  }
  int end = slice2->offset + slice2->len;
  for (int i = slice2->offset; i < end; ++i) {
    if (i != toskip) {
      bitarray_set(changed2, i);
    }
  }
}

/*
 * Helper: Given that slice1 has been split by half into top and bot, we want
 * to fetch the column at which to split slice2 so that we are still on track
 * to the optimal diff solution, i.e. the shortest one. We use lcs_lengths
 * since the shortest diff is just another way to say the longest common
 * subsequence.
 */
static int
optimal_column_to_split(const smartlist_slice_t *top,
                        const smartlist_slice_t *bot,
                        const smartlist_slice_t *slice2)
{
  int *lens_top = lcs_lengths(top, slice2, 1);
  int *lens_bot = lcs_lengths(bot, slice2, -1);
  int column=0, max_sum=-1;

  for (int i = 0; i < slice2->len+1; ++i) {
    int sum = lens_top[i] + lens_bot[slice2->len-i];
    if (sum > max_sum) {
      column = i;
      max_sum = sum;
    }
  }
  tor_free(lens_top);
  tor_free(lens_bot);

  return column;
}

/**
 * Helper: Figure out what elements are new or gone on the second smartlist
 * relative to the first smartlist, and store the booleans in the bitarrays.
 * True on the first bitarray means the element is gone, true on the second
 * bitarray means it's new.
 *
 * In its base case, either of the smartlists is of length <= 1 and we can
 * quickly see what elements are new or are gone. In the other case, we will
 * split one smartlist by half and we'll use optimal_column_to_split to find
 * the optimal column at which to split the second smartlist so that we are
 * finding the smallest diff possible.
 */
STATIC void
calc_changes(smartlist_slice_t *slice1,
             smartlist_slice_t *slice2,
             bitarray_t *changed1, bitarray_t *changed2)
{
  trim_slices(slice1, slice2);

  if (slice1->len <= 1) {
    set_changed(changed1, changed2, slice1, slice2);

  } else if (slice2->len <= 1) {
    set_changed(changed2, changed1, slice2, slice1);

  /* Keep on splitting the slices in two. */
  } else {
    smartlist_slice_t *top, *bot, *left, *right;

    /* Split the first slice in half. */
    int mid = slice1->len/2;
    top = smartlist_slice(slice1->list, slice1->offset, slice1->offset+mid);
    bot = smartlist_slice(slice1->list, slice1->offset+mid,
        slice1->offset+slice1->len);

    /* Split the second slice by the optimal column. */
    int mid2 = optimal_column_to_split(top, bot, slice2);
    left = smartlist_slice(slice2->list, slice2->offset, slice2->offset+mid2);
    right = smartlist_slice(slice2->list, slice2->offset+mid2,
        slice2->offset+slice2->len);

    calc_changes(top, left, changed1, changed2);
    calc_changes(bot, right, changed1, changed2);
    tor_free(top);
    tor_free(bot);
    tor_free(left);
    tor_free(right);
  }
}

/* This table is from crypto.c. The SP and PAD defines are different. */
#define NOT_VALID_BASE64 255
#define X NOT_VALID_BASE64
#define SP NOT_VALID_BASE64
#define PAD NOT_VALID_BASE64
static const uint8_t base64_compare_table[256] = {
  X, X, X, X, X, X, X, X, X, SP, SP, SP, X, SP, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  SP, X, X, X, X, X, X, X, X, X, X, 62, X, X, X, 63,
  52, 53, 54, 55, 56, 57, 58, 59, 60, 61, X, X, X, PAD, X, X,
  X, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, X, X, X, X, X,
  X, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
  41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
};

/** Helper: Get the identity hash from a router line, assuming that the line
 * at least appears to be a router line and thus starts with "r ".
 *
 * If an identity hash is found, store it (without decoding it) in
 * <b>hash_out</b>, and return 0.  On failure, return -1.
 */
STATIC int
get_id_hash(const cdline_t *line, cdline_t *hash_out)
{
  if (line->len < 2)
    return -1;

  /* Skip the router name. */
  const char *hash = memchr(line->s + 2, ' ', line->len - 2);
  if (!hash) {
    return -1;
  }

  hash++;
  const char *hash_end = hash;
  /* Stop when the first non-base64 character is found. Use unsigned chars to
   * avoid negative indexes causing crashes.
   */
  while (base64_compare_table[*((unsigned char*)hash_end)]
           != NOT_VALID_BASE64 &&
         hash_end < line->s + line->len) {
    hash_end++;
  }

  /* Empty hash. */
  if (hash_end == hash) {
    return -1;
  }

  hash_out->s = hash;
  /* Always true because lines are limited to this length */
  tor_assert(hash_end >= hash);
  tor_assert((size_t)(hash_end - hash) <= UINT32_MAX);
  hash_out->len = (uint32_t)(hash_end - hash);

  return 0;
}

/** Helper: Check that a line is a valid router entry. We must at least be
 * able to fetch a proper identity hash from it for it to be valid.
 */
STATIC int
is_valid_router_entry(const cdline_t *line)
{
  if (line->len < 2 || fast_memneq(line->s, "r ", 2))
    return 0;
  cdline_t tmp;
  return (get_id_hash(line, &tmp) == 0);
}

/** Helper: Find the next router line starting at the current position.
 * Assumes that cur is lower than the length of the smartlist, i.e. it is a
 * line within the bounds of the consensus. The only exception is when we
 * don't want to skip the first line, in which case cur will be -1.
 */
STATIC int
next_router(const smartlist_t *cons, int cur)
{
  int len = smartlist_len(cons);
  tor_assert(cur >= -1 && cur < len);

  if (++cur >= len) {
    return len;
  }

  const cdline_t *line = smartlist_get(cons, cur);
  while (!is_valid_router_entry(line)) {
    if (++cur >= len) {
      return len;
    }
    line = smartlist_get(cons, cur);
  }
  return cur;
}

/** Helper: compare two base64-encoded identity hashes, which may be of
 * different lengths. Comparison ends when the first non-base64 char is found.
 */
STATIC int
base64cmp(const cdline_t *hash1, const cdline_t *hash2)
{
  /* NULL is always lower, useful for last_hash which starts at NULL. */
  if (!hash1->s && !hash2->s) {
    return 0;
  }
  if (!hash1->s) {
    return -1;
  }
  if (!hash2->s) {
    return 1;
  }

  /* Don't index with a char; char may be signed. */
  const unsigned char *a = (unsigned char*)hash1->s;
  const unsigned char *b = (unsigned char*)hash2->s;
  const unsigned char *a_end = a + hash1->len;
  const unsigned char *b_end = b + hash2->len;
  while (1) {
    uint8_t av = base64_compare_table[*a];
    uint8_t bv = base64_compare_table[*b];
    if (av == NOT_VALID_BASE64) {
      if (bv == NOT_VALID_BASE64) {
        /* Both ended with exactly the same characters. */
        return 0;
      } else {
        /* hash2 goes on longer than hash1 and thus hash1 is lower. */
        return -1;
      }
    } else if (bv == NOT_VALID_BASE64) {
      /* hash1 goes on longer than hash2 and thus hash1 is greater. */
      return 1;
    } else if (av < bv) {
      /* The first difference shows that hash1 is lower. */
      return -1;
    } else if (av > bv) {
      /* The first difference shows that hash1 is greater. */
      return 1;
    } else {
      a++;
      b++;
      if (a == a_end) {
        if (b == b_end) {
          return 0;
        } else {
          return -1;
        }
      } else if (b == b_end) {
        return 1;
      }
    }
  }
}

/** Structure used to remember the previous and current identity hash of
 * the "r " lines in a consensus, to enforce well-ordering. */
typedef struct router_id_iterator_t {
  cdline_t last_hash;
  cdline_t hash;
} router_id_iterator_t;

/**
 * Initializer for a router_id_iterator_t.
 */
#define ROUTER_ID_ITERATOR_INIT { { NULL, 0 }, { NULL, 0 } }

/** Given an index *<b>idxp</b> into the consensus at <b>cons</b>, advance
 * the index to the next router line ("r ...") in the consensus, or to
 * an index one after the end of the list if there is no such line.
 *
 * Use <b>iter</b> to record the hash of the found router line, if any,
 * and to enforce ordering on the hashes.  If the hashes are mis-ordered,
 * return -1.  Else, return 0.
 **/
static int
find_next_router_line(const smartlist_t *cons,
                      const char *consname,
                      int *idxp,
                      router_id_iterator_t *iter)
{
  *idxp = next_router(cons, *idxp);
  if (*idxp < smartlist_len(cons)) {
    memcpy(&iter->last_hash, &iter->hash, sizeof(cdline_t));
    if (get_id_hash(smartlist_get(cons, *idxp), &iter->hash) < 0 ||
        base64cmp(&iter->hash, &iter->last_hash) <= 0) {
      log_warn(LD_CONSDIFF, "Refusing to generate consensus diff because "
               "the %s consensus doesn't have its router entries sorted "
               "properly.", consname);
      return -1;
    }
  }
  return 0;
}

/** Line-prefix indicating the beginning of the signatures section that we
 * intend to delete. */
#define START_OF_SIGNATURES_SECTION "directory-signature "

/** Pre-process a consensus in <b>cons</b> (represented as a list of cdline_t)
 * to remove the signatures from it.  If the footer is removed, return a
 * cdline_t containing a delete command to delete the footer, allocated in
 * <b>area</>.  If no footer is removed, return NULL.
 *
 * We remove the signatures here because they are not themselves signed, and
 * as such there might be different encodings for them.
 */
static cdline_t *
preprocess_consensus(memarea_t *area,
                     smartlist_t *cons)
{
  int idx;
  int dirsig_idx = -1;
  for (idx = 0; idx < smartlist_len(cons); ++idx) {
    cdline_t *line = smartlist_get(cons, idx);
    if (line_starts_with_str(line, START_OF_SIGNATURES_SECTION)) {
      dirsig_idx = idx;
      break;
    }
  }
  if (dirsig_idx >= 0) {
    char buf[64];
    while (smartlist_len(cons) > dirsig_idx)
      smartlist_del(cons, dirsig_idx);
    tor_snprintf(buf, sizeof(buf), "%d,$d", dirsig_idx+1);
    return cdline_linecpy(area, buf);
  } else {
    return NULL;
  }
}

/** Generate an ed diff as a smartlist from two consensuses, also given as
 * smartlists. Will return NULL if the diff could not be generated, which can
 * happen if any lines the script had to add matched "." or if the routers
 * were not properly ordered.
 *
 * All cdline_t objects in the resulting object are either references to lines
 * in one of the inputs, or are newly allocated lines in the provided memarea.
 *
 * This implementation is consensus-specific. To generate an ed diff for any
 * given input in quadratic time, you can replace all the code until the
 * navigation in reverse order with the following:
 *
 *   int len1 = smartlist_len(cons1);
 *   int len2 = smartlist_len(cons2);
 *   bitarray_t *changed1 = bitarray_init_zero(len1);
 *   bitarray_t *changed2 = bitarray_init_zero(len2);
 *   cons1_sl = smartlist_slice(cons1, 0, -1);
 *   cons2_sl = smartlist_slice(cons2, 0, -1);
 *   calc_changes(cons1_sl, cons2_sl, changed1, changed2);
 */
STATIC smartlist_t *
gen_ed_diff(const smartlist_t *cons1_orig, const smartlist_t *cons2,
            memarea_t *area)
{
  smartlist_t *cons1 = smartlist_new();
  smartlist_add_all(cons1, cons1_orig);
  cdline_t *remove_trailer = preprocess_consensus(area, cons1);

  int len1 = smartlist_len(cons1);
  int len2 = smartlist_len(cons2);
  smartlist_t *result = smartlist_new();

  if (remove_trailer) {
    /* There's a delete-the-trailer line at the end, so add it here. */
    smartlist_add(result, remove_trailer);
  }

  /* Initialize the changed bitarrays to zero, so that calc_changes only needs
   * to set the ones that matter and leave the rest untouched.
   */
  bitarray_t *changed1 = bitarray_init_zero(len1);
  bitarray_t *changed2 = bitarray_init_zero(len2);
  int i1=-1, i2=-1;
  int start1=0, start2=0;

  /* To check that hashes are ordered properly */
  router_id_iterator_t iter1 = ROUTER_ID_ITERATOR_INIT;
  router_id_iterator_t iter2 = ROUTER_ID_ITERATOR_INIT;

  /* i1 and i2 are initialized at the first line of each consensus. They never
   * reach past len1 and len2 respectively, since next_router doesn't let that
   * happen. i1 and i2 are advanced by at least one line at each iteration as
   * long as they have not yet reached len1 and len2, so the loop is
   * guaranteed to end, and each pair of (i1,i2) will be inspected at most
   * once.
   */
  while (i1 < len1 || i2 < len2) {

    /* Advance each of the two navigation positions by one router entry if not
     * yet at the end.
     */
    if (i1 < len1) {
      if (find_next_router_line(cons1, "base", &i1, &iter1) < 0) {
        goto error_cleanup;
      }
    }

    if (i2 < len2) {
      if (find_next_router_line(cons2, "target", &i2, &iter2) < 0) {
        goto error_cleanup;
      }
    }

    /* If we have reached the end of both consensuses, there is no need to
     * compare hashes anymore, since this is the last iteration.
     */
    if (i1 < len1 || i2 < len2) {

      /* Keep on advancing the lower (by identity hash sorting) position until
       * we have two matching positions. The only other possible outcome is
       * that a lower position reaches the end of the consensus before it can
       * reach a hash that is no longer the lower one. Since there will always
       * be a lower hash for as long as the loop runs, one of the two indexes
       * will always be incremented, thus assuring that the loop must end
       * after a finite number of iterations. If that cannot be because said
       * consensus has already reached the end, both are extended to their
       * respecting ends since we are done.
       */
      int cmp = base64cmp(&iter1.hash, &iter2.hash);
      while (cmp != 0) {
        if (i1 < len1 && cmp < 0) {
          if (find_next_router_line(cons1, "base", &i1, &iter1) < 0) {
            goto error_cleanup;
          }
          if (i1 == len1) {
            /* We finished the first consensus, so grab all the remaining
             * lines of the second consensus and finish up.
             */
            i2 = len2;
            break;
          }
        } else if (i2 < len2 && cmp > 0) {
          if (find_next_router_line(cons2, "target", &i2, &iter2) < 0) {
            goto error_cleanup;
          }
          if (i2 == len2) {
            /* We finished the second consensus, so grab all the remaining
             * lines of the first consensus and finish up.
             */
            i1 = len1;
            break;
          }
        } else {
          i1 = len1;
          i2 = len2;
          break;
        }
        cmp = base64cmp(&iter1.hash, &iter2.hash);
      }
    }

    /* Make slices out of these chunks (up to the common router entry) and
     * calculate the changes for them.
     * Error if any of the two slices are longer than 10K lines. That should
     * never happen with any pair of real consensuses. Feeding more than 10K
     * lines to calc_changes would be very slow anyway.
     */
#define MAX_LINE_COUNT (10000)
    if (i1-start1 > MAX_LINE_COUNT || i2-start2 > MAX_LINE_COUNT) {
      log_warn(LD_CONSDIFF, "Refusing to generate consensus diff because "
          "we found too few common router ids.");
      goto error_cleanup;
    }

    smartlist_slice_t *cons1_sl = smartlist_slice(cons1, start1, i1);
    smartlist_slice_t *cons2_sl = smartlist_slice(cons2, start2, i2);
    calc_changes(cons1_sl, cons2_sl, changed1, changed2);
    tor_free(cons1_sl);
    tor_free(cons2_sl);
    start1 = i1, start2 = i2;
  }

  /* Navigate the changes in reverse order and generate one ed command for
   * each chunk of changes.
   */
  i1=len1-1, i2=len2-1;
  char buf[128];
  while (i1 >= 0 || i2 >= 0) {

    int start1x, start2x, end1, end2, added, deleted;

    /* We are at a point were no changed bools are true, so just keep going. */
    if (!(i1 >= 0 && bitarray_is_set(changed1, i1)) &&
        !(i2 >= 0 && bitarray_is_set(changed2, i2))) {
      if (i1 >= 0) {
        i1--;
      }
      if (i2 >= 0) {
        i2--;
      }
      continue;
    }

    end1 = i1, end2 = i2;

    /* Grab all contiguous changed lines */
    while (i1 >= 0 && bitarray_is_set(changed1, i1)) {
      i1--;
    }
    while (i2 >= 0 && bitarray_is_set(changed2, i2)) {
      i2--;
    }

    start1x = i1+1, start2x = i2+1;
    added = end2-i2, deleted = end1-i1;

    if (added == 0) {
      if (deleted == 1) {
        tor_snprintf(buf, sizeof(buf), "%id", start1x+1);
        smartlist_add_linecpy(result, area, buf);
      } else {
        tor_snprintf(buf, sizeof(buf), "%i,%id", start1x+1, start1x+deleted);
        smartlist_add_linecpy(result, area, buf);
      }
    } else {
      int i;
      if (deleted == 0) {
        tor_snprintf(buf, sizeof(buf), "%ia", start1x);
        smartlist_add_linecpy(result, area, buf);
      } else if (deleted == 1) {
        tor_snprintf(buf, sizeof(buf), "%ic", start1x+1);
        smartlist_add_linecpy(result, area, buf);
      } else {
        tor_snprintf(buf, sizeof(buf), "%i,%ic", start1x+1, start1x+deleted);
        smartlist_add_linecpy(result, area, buf);
      }

      for (i = start2x; i <= end2; ++i) {
        cdline_t *line = smartlist_get(cons2, i);
        if (line_str_eq(line, ".")) {
          log_warn(LD_CONSDIFF, "Cannot generate consensus diff because "
              "one of the lines to be added is \".\".");
          goto error_cleanup;
        }
        smartlist_add(result, line);
      }
      smartlist_add_linecpy(result, area, ".");
    }
  }

  smartlist_free(cons1);
  bitarray_free(changed1);
  bitarray_free(changed2);

  return result;

 error_cleanup:

  smartlist_free(cons1);
  bitarray_free(changed1);
  bitarray_free(changed2);

  smartlist_free(result);

  return NULL;
}

/* Helper: Read a base-10 number between 0 and INT32_MAX from <b>s</b> and
 * store it in <b>num_out</b>.  Advance <b>s</b> to the characer immediately
 * after the number.  Return 0 on success, -1 on failure. */
static int
get_linenum(const char **s, int *num_out)
{
  int ok;
  char *next;
  if (!TOR_ISDIGIT(**s)) {
    return -1;
  }
  *num_out = (int) tor_parse_long(*s, 10, 0, INT32_MAX, &ok, &next);
  if (ok && next) {
    *s = next;
    return 0;
  } else {
    return -1;
  }
}

/** Apply the ed diff, starting at <b>diff_starting_line</b>, to the consensus
 * and return a new consensus, also as a line-based smartlist. Will return
 * NULL if the ed diff is not properly formatted.
 *
 * All cdline_t objects in the resulting object are references to lines
 * in one of the inputs; nothing is copied.
 */
STATIC smartlist_t *
apply_ed_diff(const smartlist_t *cons1, const smartlist_t *diff,
              int diff_starting_line)
{
  int diff_len = smartlist_len(diff);
  int j = smartlist_len(cons1);
  smartlist_t *cons2 = smartlist_new();

  for (int i=diff_starting_line; i<diff_len; ++i) {
    const cdline_t *diff_cdline = smartlist_get(diff, i);
    char diff_line[128];

    if (diff_cdline->len > sizeof(diff_line) - 1) {
      log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
               "an ed command was far too long");
      goto error_cleanup;
    }
    /* Copy the line to make it nul-terminated. */
    memcpy(diff_line, diff_cdline->s, diff_cdline->len);
    diff_line[diff_cdline->len] = 0;
    const char *ptr = diff_line;
    int start = 0, end = 0;
    int had_range = 0;
    int end_was_eof = 0;
    if (get_linenum(&ptr, &start) < 0) {
      log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
               "an ed command was missing a line number.");
      goto error_cleanup;
    }
    if (*ptr == ',') {
      /* Two-item range */
      had_range = 1;
      ++ptr;
      if (*ptr == '$') {
        end_was_eof = 1;
        end = smartlist_len(cons1);
        ++ptr;
      } else if (get_linenum(&ptr, &end) < 0) {
        log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
                 "an ed command was missing a range end line number.");
        goto error_cleanup;
      }
      /* Incoherent range. */
      if (end <= start) {
        log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
                 "an invalid range was found in an ed command.");
        goto error_cleanup;
      }
    } else {
      /* We'll take <n1> as <n1>,<n1> for simplicity. */
      end = start;
    }

    if (end > j) {
      log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
          "its commands are not properly sorted in reverse order.");
      goto error_cleanup;
    }

    if (*ptr == '\0') {
      log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
               "a line with no ed command was found");
      goto error_cleanup;
    }

    if (*(ptr+1) != '\0') {
      log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
          "an ed command longer than one char was found.");
      goto error_cleanup;
    }

    char action = *ptr;

    switch (action) {
      case 'a':
      case 'c':
      case 'd':
        break;
      default:
        log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
            "an unrecognised ed command was found.");
        goto error_cleanup;
    }

    /** $ is not allowed with non-d actions. */
    if (end_was_eof && action != 'd') {
      log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
               "it wanted to use $ with a command other than delete");
      goto error_cleanup;
    }

    /* 'a' commands are not allowed to have ranges. */
    if (had_range && action == 'a') {
      log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
          "it wanted to add lines after a range.");
      goto error_cleanup;
    }

    /* Add unchanged lines. */
    for (; j && j > end; --j) {
      cdline_t *cons_line = smartlist_get(cons1, j-1);
      smartlist_add(cons2, cons_line);
    }

    /* Ignore removed lines. */
    if (action == 'c' || action == 'd') {
      while (--j >= start) {
        /* Skip line */
      }
    }

    /* Add new lines in reverse order, since it will all be reversed at the
     * end.
     */
    if (action == 'a' || action == 'c') {
      int added_end = i;

      i++; /* Skip the line with the range and command. */
      while (i < diff_len) {
        if (line_str_eq(smartlist_get(diff, i), ".")) {
          break;
        }
        if (++i == diff_len) {
          log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
              "it has lines to be inserted that don't end with a \".\".");
          goto error_cleanup;
        }
      }

      int added_i = i-1;

      /* It would make no sense to add zero new lines. */
      if (added_i == added_end) {
        log_warn(LD_CONSDIFF, "Could not apply consensus diff because "
            "it has an ed command that tries to insert zero lines.");
        goto error_cleanup;
      }

      while (added_i > added_end) {
        cdline_t *added_line = smartlist_get(diff, added_i--);
        smartlist_add(cons2, added_line);
      }
    }
  }

  /* Add remaining unchanged lines. */
  for (; j > 0; --j) {
    cdline_t *cons_line = smartlist_get(cons1, j-1);
    smartlist_add(cons2, cons_line);
  }

  /* Reverse the whole thing since we did it from the end. */
  smartlist_reverse(cons2);
  return cons2;

 error_cleanup:

  smartlist_free(cons2);

  return NULL;
}

/** Generate a consensus diff as a smartlist from two given consensuses, also
 * as smartlists. Will return NULL if the consensus diff could not be
 * generated. Neither of the two consensuses are modified in any way, so it's
 * up to the caller to free their resources.
 */
smartlist_t *
consdiff_gen_diff(const smartlist_t *cons1,
                  const smartlist_t *cons2,
                  const consensus_digest_t *digests1,
                  const consensus_digest_t *digests2,
                  memarea_t *area)
{
  smartlist_t *ed_diff = gen_ed_diff(cons1, cons2, area);
  /* ed diff could not be generated - reason already logged by gen_ed_diff. */
  if (!ed_diff) {
    goto error_cleanup;
  }

  /* See that the script actually produces what we want. */
  smartlist_t *ed_cons2 = apply_ed_diff(cons1, ed_diff, 0);
  if (!ed_cons2) {
    /* LCOV_EXCL_START -- impossible if diff generation is correct */
    log_warn(LD_BUG|LD_CONSDIFF, "Refusing to generate consensus diff because "
        "the generated ed diff could not be tested to successfully generate "
        "the target consensus.");
    goto error_cleanup;
    /* LCOV_EXCL_STOP */
  }

  int cons2_eq = 1;
  if (smartlist_len(cons2) == smartlist_len(ed_cons2)) {
    SMARTLIST_FOREACH_BEGIN(cons2, const cdline_t *, line1) {
      const cdline_t *line2 = smartlist_get(ed_cons2, line1_sl_idx);
      if (! lines_eq(line1, line2) ) {
        cons2_eq = 0;
        break;
      }
    } SMARTLIST_FOREACH_END(line1);
  } else {
    cons2_eq = 0;
  }
  smartlist_free(ed_cons2);
  if (!cons2_eq) {
    /* LCOV_EXCL_START -- impossible if diff generation is correct. */
    log_warn(LD_BUG|LD_CONSDIFF, "Refusing to generate consensus diff because "
        "the generated ed diff did not generate the target consensus "
        "successfully when tested.");
    goto error_cleanup;
    /* LCOV_EXCL_STOP */
  }

  char cons1_hash_hex[HEX_DIGEST256_LEN+1];
  char cons2_hash_hex[HEX_DIGEST256_LEN+1];
  base16_encode(cons1_hash_hex, HEX_DIGEST256_LEN+1,
                (const char*)digests1->sha3_256, DIGEST256_LEN);
  base16_encode(cons2_hash_hex, HEX_DIGEST256_LEN+1,
                (const char*)digests2->sha3_256, DIGEST256_LEN);

  /* Create the resulting consensus diff. */
  char buf[160];
  smartlist_t *result = smartlist_new();
  tor_snprintf(buf, sizeof(buf), "%s", ns_diff_version);
  smartlist_add_linecpy(result, area, buf);
  tor_snprintf(buf, sizeof(buf), "%s %s %s", hash_token,
      cons1_hash_hex, cons2_hash_hex);
  smartlist_add_linecpy(result, area, buf);
  smartlist_add_all(result, ed_diff);
  smartlist_free(ed_diff);
  return result;

 error_cleanup:

  if (ed_diff) {
    /* LCOV_EXCL_START -- ed_diff is NULL except in unreachable cases above */
    smartlist_free(ed_diff);
    /* LCOV_EXCL_STOP */
  }

  return NULL;
}

/** Fetch the digest of the base consensus in the consensus diff, encoded in
 * base16 as found in the diff itself. digest1_out and digest2_out must be of
 * length DIGEST256_LEN or larger if not NULL.
 */
int
consdiff_get_digests(const smartlist_t *diff,
                     char *digest1_out,
                     char *digest2_out)
{
  smartlist_t *hash_words = NULL;
  const cdline_t *format;
  char cons1_hash[DIGEST256_LEN], cons2_hash[DIGEST256_LEN];
  char *cons1_hash_hex, *cons2_hash_hex;
  if (smartlist_len(diff) < 2) {
    log_info(LD_CONSDIFF, "The provided consensus diff is too short.");
    goto error_cleanup;
  }

  /* Check that it's the format and version we know. */
  format = smartlist_get(diff, 0);
  if (!line_str_eq(format, ns_diff_version)) {
    log_warn(LD_CONSDIFF, "The provided consensus diff format is not known.");
    goto error_cleanup;
  }

  /* Grab the base16 digests. */
  hash_words = smartlist_new();
  {
    const cdline_t *line2 = smartlist_get(diff, 1);
    char *h = tor_memdup_nulterm(line2->s, line2->len);
    smartlist_split_string(hash_words, h, " ", 0, 0);
    tor_free(h);
  }

  /* There have to be three words, the first of which must be hash_token. */
  if (smartlist_len(hash_words) != 3 ||
      strcmp(smartlist_get(hash_words, 0), hash_token)) {
    log_info(LD_CONSDIFF, "The provided consensus diff does not include "
        "the necessary digests.");
    goto error_cleanup;
  }

  /* Expected hashes as found in the consensus diff header. They must be of
   * length HEX_DIGEST256_LEN, normally 64 hexadecimal characters.
   * If any of the decodings fail, error to make sure that the hashes are
   * proper base16-encoded digests.
   */
  cons1_hash_hex = smartlist_get(hash_words, 1);
  cons2_hash_hex = smartlist_get(hash_words, 2);
  if (strlen(cons1_hash_hex) != HEX_DIGEST256_LEN ||
      strlen(cons2_hash_hex) != HEX_DIGEST256_LEN) {
    log_info(LD_CONSDIFF, "The provided consensus diff includes "
        "base16-encoded digests of incorrect size.");
    goto error_cleanup;
  }

  if (base16_decode(cons1_hash, DIGEST256_LEN,
          cons1_hash_hex, HEX_DIGEST256_LEN) != DIGEST256_LEN ||
      base16_decode(cons2_hash, DIGEST256_LEN,
          cons2_hash_hex, HEX_DIGEST256_LEN) != DIGEST256_LEN) {
    log_info(LD_CONSDIFF, "The provided consensus diff includes "
        "malformed digests.");
    goto error_cleanup;
  }

  if (digest1_out) {
    memcpy(digest1_out, cons1_hash, DIGEST256_LEN);
  }
  if (digest2_out) {
    memcpy(digest2_out, cons2_hash, DIGEST256_LEN);
  }

  SMARTLIST_FOREACH(hash_words, char *, cp, tor_free(cp));
  smartlist_free(hash_words);
  return 0;

 error_cleanup:

  if (hash_words) {
    SMARTLIST_FOREACH(hash_words, char *, cp, tor_free(cp));
    smartlist_free(hash_words);
  }
  return 1;
}

/** Apply the consensus diff to the given consensus and return a new
 * consensus, also as a line-based smartlist. Will return NULL if the diff
 * could not be applied. Neither the consensus nor the diff are modified in
 * any way, so it's up to the caller to free their resources.
 */
char *
consdiff_apply_diff(const smartlist_t *cons1,
                    const smartlist_t *diff,
                    const consensus_digest_t *digests1)
{
  smartlist_t *cons2 = NULL;
  char *cons2_str = NULL;
  char e_cons1_hash[DIGEST256_LEN];
  char e_cons2_hash[DIGEST256_LEN];

  if (consdiff_get_digests(diff, e_cons1_hash, e_cons2_hash) != 0) {
    goto error_cleanup;
  }

  /* See that the consensus that was given to us matches its hash. */
  if (!consensus_digest_eq(digests1->sha3_256,
                           (const uint8_t*)e_cons1_hash)) {
    char hex_digest1[HEX_DIGEST256_LEN+1];
    char e_hex_digest1[HEX_DIGEST256_LEN+1];
    log_warn(LD_CONSDIFF, "Refusing to apply consensus diff because "
        "the base consensus doesn't match the digest as found in "
        "the consensus diff header.");
    base16_encode(hex_digest1, HEX_DIGEST256_LEN+1,
                  (const char *)digests1->sha3_256, DIGEST256_LEN);
    base16_encode(e_hex_digest1, HEX_DIGEST256_LEN+1,
                  e_cons1_hash, DIGEST256_LEN);
    log_warn(LD_CONSDIFF, "Expected: %s; found: %s",
             hex_digest1, e_hex_digest1);
    goto error_cleanup;
  }

  /* Grab the ed diff and calculate the resulting consensus. */
  /* Skip the first two lines. */
  cons2 = apply_ed_diff(cons1, diff, 2);

  /* ed diff could not be applied - reason already logged by apply_ed_diff. */
  if (!cons2) {
    goto error_cleanup;
  }

  cons2_str = consensus_join_lines(cons2);

  consensus_digest_t cons2_digests;
  if (consensus_compute_digest(cons2_str, &cons2_digests) < 0) {
    /* LCOV_EXCL_START -- digest can't fail */
    log_warn(LD_CONSDIFF, "Could not compute digests of the consensus "
        "resulting from applying a consensus diff.");
    goto error_cleanup;
    /* LCOV_EXCL_STOP */
  }

  /* See that the resulting consensus matches its hash. */
  if (!consensus_digest_eq(cons2_digests.sha3_256,
                           (const uint8_t*)e_cons2_hash)) {
    log_warn(LD_CONSDIFF, "Refusing to apply consensus diff because "
        "the resulting consensus doesn't match the digest as found in "
        "the consensus diff header.");
    char hex_digest2[HEX_DIGEST256_LEN+1];
    char e_hex_digest2[HEX_DIGEST256_LEN+1];
    base16_encode(hex_digest2, HEX_DIGEST256_LEN+1,
        (const char *)cons2_digests.sha3_256, DIGEST256_LEN);
    base16_encode(e_hex_digest2, HEX_DIGEST256_LEN+1,
        e_cons2_hash, DIGEST256_LEN);
    log_warn(LD_CONSDIFF, "Expected: %s; found: %s",
             hex_digest2, e_hex_digest2);
    goto error_cleanup;
  }

  goto done;

 error_cleanup:
  tor_free(cons2_str); /* Sets it to NULL */

 done:
  if (cons2) {
    smartlist_free(cons2);
  }

  return cons2_str;
}

/** Any consensus line longer than this means that the input is invalid. */
#define CONSENSUS_LINE_MAX_LEN (1<<20)

/**
 * Helper: For every NL-terminated line in <b>s</b>, add a cdline referring to
 * that line (without trailing newline) to <b>out</b>.  Return -1 if there are
 * any non-NL terminated lines; 0 otherwise.
 *
 * Unlike tor_split_lines, this function avoids ambiguity on its
 * handling of a final line that isn't NL-terminated.
 *
 * All cdline_t objects are allocated in the provided memarea.  Strings
 * are not copied: if <b>s</b> changes or becomes invalid, then all
 * generated cdlines will become invalid.
 */
STATIC int
consensus_split_lines(smartlist_t *out, const char *s, memarea_t *area)
{
  const char *end_of_str = s + strlen(s);
  tor_assert(*end_of_str == '\0');

  while (*s) {
    const char *eol = memchr(s, '\n', end_of_str - s);
    if (!eol) {
      /* File doesn't end with newline. */
      return -1;
    }
    if (eol - s > CONSENSUS_LINE_MAX_LEN) {
      /* Line is far too long. */
      return -1;
    }
    cdline_t *line = memarea_alloc(area, sizeof(cdline_t));
    line->s = s;
    line->len = (uint32_t)(eol - s);
    smartlist_add(out, line);
    s = eol+1;
  }
  return 0;
}

/** Given a list of cdline_t, return a newly allocated string containing
 * all of the lines, terminated with NL, concatenated.
 *
 * Unlike smartlist_join_strings(), avoids lossy operations on empty
 * lists.  */
static char *
consensus_join_lines(const smartlist_t *inp)
{
  size_t n = 0;
  SMARTLIST_FOREACH(inp, const cdline_t *, cdline, n += cdline->len + 1);
  n += 1;
  char *result = tor_malloc(n);
  char *out = result;
  SMARTLIST_FOREACH_BEGIN(inp, const cdline_t *, cdline) {
    memcpy(out, cdline->s, cdline->len);
    out += cdline->len;
    *out++ = '\n';
  } SMARTLIST_FOREACH_END(cdline);
  *out++ = '\0';
  tor_assert(out == result+n);
  return result;
}

/** Given two consensus documents, try to compute a diff between them.  On
 * success, retun a newly allocated string containing that diff.  On failure,
 * return NULL. */
char *
consensus_diff_generate(const char *cons1,
                        const char *cons2)
{
  consensus_digest_t d1, d2;
  smartlist_t *lines1 = NULL, *lines2 = NULL, *result_lines = NULL;
  int r1, r2;
  char *result = NULL;

  r1 = consensus_compute_digest_as_signed(cons1, &d1);
  r2 = consensus_compute_digest(cons2, &d2);
  if (BUG(r1 < 0 || r2 < 0))
    return NULL; // LCOV_EXCL_LINE

  memarea_t *area = memarea_new();
  lines1 = smartlist_new();
  lines2 = smartlist_new();
  if (consensus_split_lines(lines1, cons1, area) < 0)
    goto done;
  if (consensus_split_lines(lines2, cons2, area) < 0)
    goto done;

  result_lines = consdiff_gen_diff(lines1, lines2, &d1, &d2, area);

 done:
  if (result_lines) {
    result = consensus_join_lines(result_lines);
    smartlist_free(result_lines);
  }

  memarea_drop_all(area);
  smartlist_free(lines1);
  smartlist_free(lines2);

  return result;
}

/** Given a consensus document and a diff, try to apply the diff to the
 * consensus.  On success return a newly allocated string containing the new
 * consensus.  On failure, return NULL. */
char *
consensus_diff_apply(const char *consensus,
                     const char *diff)
{
  consensus_digest_t d1;
  smartlist_t *lines1 = NULL, *lines2 = NULL;
  int r1;
  char *result = NULL;
  memarea_t *area = memarea_new();

  r1 = consensus_compute_digest_as_signed(consensus, &d1);
  if (BUG(r1 < 0))
    goto done;

  lines1 = smartlist_new();
  lines2 = smartlist_new();
  if (consensus_split_lines(lines1, consensus, area) < 0)
    goto done;
  if (consensus_split_lines(lines2, diff, area) < 0)
    goto done;

  result = consdiff_apply_diff(lines1, lines2, &d1);

 done:
  smartlist_free(lines1);
  smartlist_free(lines2);
  memarea_drop_all(area);

  return result;
}

/** Return true iff, based on its header, <b>document</b> is likely
 * to be a consensus diff. */
int
looks_like_a_consensus_diff(const char *document, size_t len)
{
  return (len >= strlen(ns_diff_version) &&
          fast_memeq(document, ns_diff_version, strlen(ns_diff_version)));
}
