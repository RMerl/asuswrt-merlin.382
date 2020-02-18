/* Copyright (c) 2018-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file authmode.h
 * \brief Header file for directory authority mode.
 **/

#ifndef TOR_DIRAUTH_MODE_H
#define TOR_DIRAUTH_MODE_H

#include "feature/relay/router.h"

#ifdef HAVE_MODULE_DIRAUTH

int authdir_mode(const or_options_t *options);
int authdir_mode_handles_descs(const or_options_t *options, int purpose);
int authdir_mode_publishes_statuses(const or_options_t *options);
int authdir_mode_tests_reachability(const or_options_t *options);
int authdir_mode_bridge(const or_options_t *options);

/* Return true iff we believe ourselves to be a v3 authoritative directory
 * server. */
static inline int
authdir_mode_v3(const or_options_t *options)
{
  return authdir_mode(options) && options->V3AuthoritativeDir != 0;
}

#define have_module_dirauth() (1)

#else /* HAVE_MODULE_DIRAUTH */

#define authdir_mode(options) (((void)(options)),0)
#define authdir_mode_handles_descs(options,purpose) \
  (((void)(options)),((void)(purpose)),0)
#define authdir_mode_publishes_statuses(options) (((void)(options)),0)
#define authdir_mode_tests_reachability(options) (((void)(options)),0)
#define authdir_mode_bridge(options) (((void)(options)),0)
#define authdir_mode_v3(options) (((void)(options)),0)

#define have_module_dirauth() (0)

#endif /* HAVE_MODULE_DIRAUTH */

#endif /* TOR_MODE_H */
