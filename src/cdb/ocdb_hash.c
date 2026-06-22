/*
 * ocdb_hash.c
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

#include <stdint.h>

#include "cdb-internal.h"

uint32_t ocdb_hashadd(uint32_t h, unsigned char c)
{
    h += (h << 5) ;
    return h ^ c ;
}

uint32_t ocdb_hash(char const *buf, uint32_t len)
{
    uint32_t h = OCDB_HASHSTART ;
    while (len--) h = ocdb_hashadd(h, (unsigned char)*buf++) ;
    return h ;
}
