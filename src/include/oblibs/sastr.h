/* 
 * sastr.h
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
 
#ifndef SASTR_H
#define SASTR_H

#include <sys/stat.h>
#include <skalibs/stralloc.h>

/** get all compoments in @srcdir as mode @mode execpt string @exclude
 * put the result in chained stralloc 
 * @Return 0 on fail
 * @Return 1 on success */
extern int sastr_dir_get(stralloc *sa, char const *srcdir,char const *exclude,mode_t mode) ;

/** Compare @s in a chainled stralloc @sa
 * @Return -1 if not found
 * @Return the position of @s into the chained stralloc @sa */
extern ssize_t sastr_cmp(stralloc *sa,char const *s) ;

#endif
