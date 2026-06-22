/*
 * u64_scan_base.c
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

#include <oblibs/types.h>

/** These functions come from skalibs:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libstddjb/uint64_scan_base_max.c
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libstddjb/uint64_scan.c
 * under license ISC. All credits go to Laurent Bercot, author of skalibs.
 * They were renamed (uint64_scan_base[_max] -> u64_scan_base[_max]) and integrated
 * into oblibs by Eric Vidal <eric@obarun.org> to remove the skalibs dependency. */

size_t u64_scan_base_max(char const *s, uint64_t *u, uint8_t base, uint64_t max)
{
    uint64_t result = 0 ;
    size_t pos = 0 ;
    for (;; pos++)
    {
        uint8_t c = scan_digit(s[pos], base) ;
        if (c >= base || result > ((max - c) / base)) break ;
        result = result * base + c ;
    }
    if (pos) *u = result ;
    return pos ;
}

size_t u64_scan_base(char const *s, uint64_t *u, uint8_t base)
{
    return u64_scan_base_max(s, u, base, UINT64_MAX) ;
}
