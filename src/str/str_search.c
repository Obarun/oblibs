/*
 * str_search.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>   /* strchrnul (_GNU_SOURCE, set project-wide) */

size_t str_search(char const *s, int c)
{
    return (size_t)(strchrnul(s, c) - s) ;
}
