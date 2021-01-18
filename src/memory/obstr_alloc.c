/*
 * obstr_alloc.c
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

#include <stdlib.h>
#include <stddef.h>

#include <oblibs/memory.h>

void *obstr_alloc(size_t size)
{
    size_t i ;
    char *ptr ;

    i=0;
    if (!(ptr=(char *)malloc(sizeof(char) * (size+1))))
        return NULL ;
    while (i <= size)
    {
        ptr[i]= 0 ;
        i++ ;
    }
    ptr[i] = 0 ;
    return ptr ;
}
