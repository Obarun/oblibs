/*
 * sastr_dir_drop_child.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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
#include <stdint.h> //uint8_t

#include <oblibs/directory.h>

#include <skalibs/stralloc.h>

int sastr_dir_drop_child(stralloc *sa)
{
    stralloc tmp = STRALLOC_ZERO ;

    size_t pos = 0 ;
    uint8_t on_parent = 0 ;
    char *current = 0, *previous = 0 ;

    for (; pos < sa->len ; pos += strlen(sa->s + pos) + 1)
    {
        current = sa->s + pos ;

        // we don't want to drop all :)
        if (!strcmp(current,"/")) continue ;

        // add the first one
        if (!previous) {
            if (!sastr_add_string(&tmp,current)) return 0 ;
            previous = current ;
            continue ;
        }

        if (!dir_is_child(previous,current)) {
            if (!sastr_add_string(&tmp,current)) return 0 ;
            on_parent = 1 ;
        }
        else on_parent = 0 ;

        if (on_parent)
            previous = current ;
    }

    if (!stralloc_copy(sa,&tmp) ||
        !stralloc_0(sa)) return 0 ;
    sa->len-- ;
    stralloc_free(&tmp) ;

    return 1 ;
}


