/*
 * sastr_len.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/sastr.h>

#include <stddef.h>

#include <skalibs/stralloc.h>
#include <skalibs/bytestr.h>

size_t sastr_len(stralloc *sa)
{
    return byte_count(sa->s,sa->len,'\0') ;
}
