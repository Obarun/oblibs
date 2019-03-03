/* 
 * byte_tosearch.c
 * 
 * Copyright (c) 2018-2019 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>

#include <oblibs/bytes.h>

size_t byte_tosearch (char const *s, size_t len, char const b)
{
  size_t n = 0 ;
  len++ ;
  while (len--) if (*s++ == b) n++ ;
  return n ;
}
