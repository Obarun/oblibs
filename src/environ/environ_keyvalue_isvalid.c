/*
 * environ_keyvalue_isvalid.c
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

#include <oblibs/environ.h>

#include <oblibs/string.h>

int environ_keyvalue_isvalid(char const *key_value)
{
    if (get_len_until(key_value,'=') == -1)
        return 0 ;

    return 1 ;

}

