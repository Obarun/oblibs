/* 
 * stdout_die_sys.c
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

#include <errno.h>
#include <unistd.h>

#include <oblibs/error.h>

void stdout_msgsys (char const *x1, char const *x2, char const *x3, char const *x4, char const *x5, char const *x6)
{
  stdout_msg(x1, x2, x3, x4, x5, x6, strerror(errno)) ;
}

void stdout_diesys (int e, char const *x1, char const *x2, char const *x3, char const *x4, char const *x5, char const *x6)
{
  stdout_msgsys(x1, x2, x3, x4, x5, x6) ;
  _exit(e) ;
}
