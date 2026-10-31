/*
 * str_rsearch.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>

size_t str_rsearch(char const *s, int c)
{
    char const *p = strrchr(s, c) ;
    return p ? (size_t)(p - s) : strlen(s) ;
}

