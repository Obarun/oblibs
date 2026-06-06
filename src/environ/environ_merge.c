/*
 * environ_length.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 * this following function come from:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libenvexec/env_merge.c
 * under license ISC. All credits go to Laurent Bercot authors of these functions.
 */

#include <string.h>
#include <errno.h>

#include <oblibs/string.h>

size_t environ_merge (char const **v, size_t vmax, char const *const *envp, size_t envlen, char const *modifs, size_t modiflen)
{
  size_t vlen = envlen ;
  size_t i = 0 ;
  if (envlen >= vmax) return 0 ;
  for (; i < envlen ; i++) v[i] = envp[i] ;
  for (i = 0 ; i < modiflen ; i += strlen(modifs + i) + 1)
  {
    size_t split = str_search(modifs + i, '=') ;
    size_t j = 0 ;
    for (; j < vlen ; j++)
      if (!strncmp(modifs + i, v[j], split) && (v[j][split] == '=')) break ;
    if (j < vlen) v[j] = v[--vlen] ;
    if (modifs[i + split])
    {
      if (vlen >= vmax) return 0 ;
      v[vlen++] = modifs + i ;
    }
  }
  if (vlen >= vmax) return 0 ;
  v[vlen++] = 0 ;
  return vlen ;
}
