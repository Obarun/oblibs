/*
 * u64_fmtpad.c
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

#include <string.h>

#include <oblibs/types.h>

/** This function comes from skalibs:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libstddjb/uint640_fmt_generic.c
 * under license ISC. All credits go to Laurent Bercot, author of skalibs.
 * It was renamed (uint640_fmt_generic -> u64_fmtpad) and integrated into oblibs
 * by Eric Vidal <eric@obarun.org> to remove the skalibs dependency. */

size_t u64_fmtpad(char *s, uint64_t x, size_t n, uint8_t base)
{
    size_t len = u64_fmtbase(0, x, base) ;
    if (s)
    {
        if (len < n)
        {
            memset(s, '0', n - len) ;
            s += n - len ;
        }
        u64_fmtbase(s, x, base) ;
    }
    return len > n ? len : n ;
}
