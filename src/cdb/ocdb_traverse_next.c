/*
 * ocdb_traverse_next.c
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
#include <errno.h>

#include <oblibs/cdb.h>
#include <oblibs/types.h>

#include "cdb-internal.h"

int ocdb_traverse_next(ocdb const *c, ocdb_data *key, ocdb_data *data, uint32_t *pos)
{
    uint32_t eod ;
    char const *p = ocdb_p(c, 4, 0) ;

    if (!p)
        return (errno = EPROTO, -1) ;

    u32_unpack(p, &eod) ;                 /* end of data = first hash table pos */

    if (eod < OCDB_HEADER || eod - 8 < *pos)
        return 0 ;

    if (*pos + 8 < *pos)
        return (errno = EPROTO, -1) ;

    p = ocdb_p(c, 8, *pos) ;
    if (!p)
        return (errno = EPROTO, -1) ;

    u32_unpack(p, &key->len) ;
    u32_unpack(p + 4, &data->len) ;

    key->s = c->map + *pos + 8 ;
    data->s = key->s + key->len ;
    *pos += 8 + key->len + data->len ;

    return 1 ;
}
