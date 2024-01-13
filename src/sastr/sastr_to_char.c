/*
 * sastr_sortndrop_element.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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

#include <string.h>

void sastr_to_char(char *str,stralloc *sa)
{
    size_t pos = 0 ;

    for(;pos < sa->len ; pos += strlen(sa->s + pos) + 1)
        memcpy(str + pos,sa->s + pos,strlen(sa->s + pos) + 1) ;

}


