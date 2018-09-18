/* 
 * stdout_die.c
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */
#include <oblibs/error.h>

#include <errno.h>
#include <unistd.h>

#include <skalibs/buffer.h>

void stdout_msg (char const *x1, char const *x2, char const *x3, char const *x4, char const *x5, char const *x6, char const *se)
{
  int e = errno ;
  if (x1) buffer_puts(buffer_2, x1) ;
  if (x2) buffer_puts(buffer_2, x2) ;
  if (x3) buffer_puts(buffer_2, x3) ;
  if (x4) buffer_puts(buffer_2, x4) ;
  if (x5) buffer_puts(buffer_2, x5) ;
  if (x6) buffer_puts(buffer_2, x6) ;
  if (se) buffer_puts(buffer_2, se) ;
  buffer_putflush(buffer_2, "\n", 1) ;
  errno = e ;
}

void stdout_die (int e,char const *x1, char const *x2, char const *x3, char const *x4, char const *x5, char const *x6, char const *se)
{
  stdout_msg(x1, x2, x3, x4, x5, x6, se) ;
  _exit(e) ;
}
