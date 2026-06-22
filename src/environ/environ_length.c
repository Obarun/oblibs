/*
 * environ_length.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 * this following function come from:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libenvexec/env_len.c
 * under license ISC. All credits go to Laurent Bercot authors of these functions.
 */

#include <stddef.h>
#include <oblibs/environ.h>

size_t environ_length (char const *const *e)
{
  size_t i = 0 ;
  while (*e++) i++ ;
  return i ;
}
