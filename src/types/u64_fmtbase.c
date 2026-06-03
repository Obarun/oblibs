/*
 * u64_fmtbase.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/types.h>

/** This function comes from skalibs:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libstddjb/uint64_fmt_generic.c
 * under license ISC. All credits go to Laurent Bercot, author of skalibs.
 * It was renamed (uint64_fmt_generic -> u64_fmtbase) and integrated into oblibs
 * by Eric Vidal <eric@obarun.org> to remove the skalibs dependency. */

size_t u64_fmtbase(char *s, uint64_t x, uint8_t base)
{
    size_t len = 1 ;
    uint64_t q = x ;
    while (q >= base) { len++ ; q /= base ; }
    if (s)
    {
        s += len ;
        do { *--s = fmt_digit(x % base) ; x /= base ; } while (x) ;
    }
    return len ;
}
