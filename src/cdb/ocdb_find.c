/*
 * ocdb_find.c
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
#include <string.h>
#include <errno.h>

#include <oblibs/cdb.h>
#include <oblibs/types.h>

#include "cdb-internal.h"

int ocdb_findnext(ocdb const *c, ocdb_data *out, char const *key, uint32_t len, ocdb_find_state *d)
{
    if (!d->loop) {

        uint32_t u = ocdb_hash(key, len) ;
        char const *p = ocdb_p(c, 8, (u << 3) & 2047) ;

        if (!p)
            return (errno = EPROTO, -1) ;

        u32_unpack(p + 4, &d->hslots) ;

        if (!d->hslots)
            return 0 ;

        u32_unpack(p, &d->hpos) ;

        d->khash = u ;
        u >>= 8 ;
        u %= d->hslots ;
        u <<= 3 ;
        d->kpos = d->hpos + u ;
    }

    while (d->loop < d->hslots) {

        uint32_t pos, u ;
        char const *p = ocdb_p(c, 8, d->kpos) ;

        if (!p)
            return (errno = EPROTO, -1) ;

        u32_unpack(p + 4, &pos) ;
        if (!pos)
            return 0 ;

        d->loop++ ;
        d->kpos += 8 ;

        if (d->kpos == d->hpos + (d->hslots << 3))
            d->kpos = d->hpos ;

        u32_unpack(p, &u) ;

        if (u == d->khash) {

            p = ocdb_p(c, 8, pos) ;
            if (!p)
                return (errno = EPROTO, -1) ;

            u32_unpack(p, &u) ;

            if (u == len) {

                char const *k = ocdb_p(c, len, pos + 8) ;
                if (!k)
                    return (errno = EPROTO, -1) ;

                if (!memcmp(key, k, len)) {

                    u32_unpack(p + 4, &out->len) ;
                    out->s = c->map + pos + 8 + len ;
                    return 1 ;
                }
            }
        }
    }
    return 0 ;
}

int ocdb_find(ocdb const *c, ocdb_data *out, char const *key, uint32_t len)
{
    ocdb_find_state d = OCDB_FIND_STATE_ZERO ;
    return ocdb_findnext(c, out, key, len, &d) ;
}
