/* 
 * error2.h
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

#ifndef OB_ERROR2_H
#define OB_ERROR2_H

#include <errno.h>
#include <skalibs/strerr2.h>

#define exitusage(x1) strerr_dieusage(100, x1)
#define exitnomem(x1,x2) strerr_dief4x(111, "out of memory : ",x1," into function : ",x2) ;
#define exitstralloc(x1) exitnomem("stralloc",x1)
#define warnstralloc(x1) strerr_warnw2x("stralloc: ",x1)
#define retstralloc(e,x1) { warnstralloc(x1) ; return e ; }
#define retstralloc_clean(e,x1,func) { warnstralloc(x1) ; func ; return e ; }
#endif
