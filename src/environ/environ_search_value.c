/*
 * environ_get_val.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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

#include <oblibs/stack.h>
#include <oblibs/lexer.h>
#include <oblibs/environ.h>

uint8_t environ_search_value(stack *stk, const char *str, const char *key)
{
    lexer_config cfg = LEXER_CONFIG_ZERO ;
    _alloc_stk_(s, strlen(str) + 1) ;

    if (!environ_search_key(&s, str, key, &cfg))
        return (errno = EINVAL, 0) ;

    if (!environ_get_value(stk, str + cfg.opos))
        return 0 ;

    return 1 ;
}
