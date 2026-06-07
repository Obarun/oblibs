/*
 * ocdb_p.c
 *
 * Copyright (c) 2018-2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>

#include "cdb-internal.h"

char const *ocdb_p(ocdb const *c, uint32_t len, uint32_t pos)
{
    return pos <= c->size && len <= c->size - pos ? c->map + pos : 0 ;
}
