/*
 * environ_clean_unexport.c
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

#include <string.h>
#include <errno.h>

#include <oblibs/sastr.h>
#include <oblibs/stack.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>

#include <skalibs/stralloc.h>

uint8_t environ_clean_unexport(stralloc *modifs)
{
    if (!modifs->len)
        return 1 ;

    size_t pos = 0 ;
    _init_stack_(stk, modifs->len) ;

    FOREACH_SASTR(modifs, pos) {

        char *line = modifs->s + pos ;
        size_t len = strlen(line) ;
        ssize_t r = get_len_until(line, '=') ;
        if (r < 0)
            return 0 ;

        if (line[r + 1] == VAR_UNEXPORT) {

            if (!stack_add(&stk, line, ++r) ||
                !stack_add(&stk, line + (r + 1), len - (r + 1)) ||
                !stack_add(&stk, "", 1))
                    return 0 ;

        } else {

            if (!stack_add(&stk, line, len) ||
                !stack_add(&stk, "" , 1))
                    return 0 ;
        }
    }

    if (!stralloc_copyb(modifs, stk.s, stk.len))
        return (errno = EOVERFLOW, 0) ;

    return 1 ;
}