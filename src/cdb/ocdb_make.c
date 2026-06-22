/*
 * ocdb_make.c
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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <oblibs/cdb.h>
#include <oblibs/strbuf.h>
#include <oblibs/stream.h>
#include <oblibs/types.h>

#include "cdb-internal.h"

static int posplus(ocdbmaker *c, uint32_t len)
{
    uint32_t newpos = c->pos + len ;

    if (newpos < len)
        return (errno = ENOMEM, 0) ;   /* 32-bit wrap = file too big */
    c->pos = newpos ;

    return 1 ;
}

static int addbegin(ocdbmaker *c, uint32_t keylen, uint32_t datalen)
{
    char buf[8] ;

    u32_pack(buf, keylen) ;
    u32_pack(buf + 4, datalen) ;

    return ostream_put(&c->b, buf, 8) ;
}

static int addend(ocdbmaker *c, uint32_t keylen, uint32_t datalen, uint32_t h)
{
    ocdb_hp hp = { .h = h, .p = c->pos } ;
    return strbuf_catb(&c->hplist, (char const *)&hp, sizeof hp)
            && posplus(c, 8)
            && posplus(c, keylen)
            && posplus(c, datalen) ;
}

int ocdb_make_start(ocdbmaker *c, int fd)
{
    c->hplist = (strbuf)STRBUF_ZERO ;
    c->pos = OCDB_HEADER ;

    if (!ostream_init(&c->b, fd, c->buf, sizeof c->buf))
        return 0 ;

    return lseek(fd, c->pos, SEEK_SET) >= 0 ;
}

int ocdb_make_add(ocdbmaker *c, char const *key, uint32_t klen, char const *data, uint32_t dlen)
{
    if (!addbegin(c, klen, dlen) ||
        !ostream_put(&c->b, key, klen) ||
        !ostream_put(&c->b, data, dlen) ||
        !addend(c, klen, dlen, ocdb_hash(key, klen))) {
            strbuf_free(&c->hplist) ;
            return 0 ;
    }

    return 1 ;
}

int ocdb_make_finish(ocdbmaker *c)
{
    uint32_t count[256] ;
    uint32_t start[256] ;
    char final[OCDB_HEADER] ;
    unsigned int size = 1 ;
    unsigned int n = (unsigned int)(c->hplist.len / sizeof(ocdb_hp)) ;
    unsigned int i = 0 ;
    ocdb_hp *hp = (ocdb_hp *)c->hplist.s ;
    ocdb_hp *split, *work ;

    for (; i < 256 ; i++)
        count[i] = 0 ;

    for (i = 0 ; i < n ; i++)
        ++count[hp[i].h & 255] ;

    {
        uint32_t u = 0 ;

        for (i = 0 ; i < 256 ; i++)
            start[i] = u += count[i] ;   /* bounded by n */

        for (i = 0 ; i < 256 ; i++) {
            u = count[i] << 1 ;
            if (u > size) size = u ;
        }

        size += n ;                                              /* no overflow up to here */
        u = 0xffffffffUL ; u /= sizeof(ocdb_hp) ;
        if (size > u)
            return (errno = ENOMEM, 0) ;
    }

    /* heap allocation (DJB) rather than a stack VLA (skalibs): robust whatever
     * the entry count. The byte layout produced is identical. */
    split = (ocdb_hp *)malloc((size_t)size * sizeof(ocdb_hp)) ;
    if (!split)
        return 0 ;

    i = n ;
    while (i--)
        split[--start[hp[i].h & 255]] = hp[i] ;

    strbuf_free(&c->hplist) ;
    work = split + n ;

    for (i = 0 ; i < 256 ; ++i) {

        char buf[8] ;
        uint32_t k = count[i] ;
        uint32_t len = k << 1 ;                                  /* no overflow possible */
        ocdb_hp *p = split + start[i] ;

        u32_pack(final + (i << 3), c->pos) ;
        u32_pack(final + (i << 3) + 4, len) ;

        for (uint32_t j = 0 ; j < len ; j++)
            work[j].h = work[j].p = 0 ;

        for (uint32_t j = 0 ; j < k ; j++) {

            uint32_t where = (p->h >> 8) % len ;
            while (work[where].p) {

                if (++where == len)
                    where = 0 ;
            }
            work[where] = *p++ ;
        }

        for (uint32_t j = 0 ; j < len ; j++) {

            u32_pack(buf, work[j].h) ;
            u32_pack(buf + 4, work[j].p) ;

            if (!ostream_put(&c->b, buf, 8) || !posplus(c, 8)) {
                free(split) ;
                return 0 ;
            }
        }
    }

    free(split) ;

    if (!ostream_flush(&c->b) ||
        lseek(c->b.fd, 0, SEEK_SET) == -1 ||
        !ostream_putflush(&c->b, final, OCDB_HEADER))
            return 0 ;

    return 1 ;
}
