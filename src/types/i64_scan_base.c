/*
 * i64_scan_base.c
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
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libstddjb/int64_scan_base_max.c
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libstddjb/int64_scan.c
 * under license ISC. All credits go to Laurent Bercot, author of skalibs.
 * They were renamed (int64_scan_base[_max] -> i64_scan_base[_max]) and integrated
 * into oblibs by Eric Vidal <eric@obarun.org> to remove the skalibs dependency. */

size_t i64_scan_base_max(char const *s, int64_t *d, uint8_t base, uint64_t max)
{
    if (s[0] == '-')
    {
        uint64_t u ;
        size_t pos = u64_scan_base_max(s + 1, &u, base, max + 1) ;
        return pos ? (*d = -u, 1 + pos) : 0 ;
    }
    else if (s[0] == '+')
    {
        size_t pos = u64_scan_base_max(s + 1, (uint64_t *)d, base, max) ;
        return pos ? 1 + pos : 0 ;
    }
    else return u64_scan_base_max(s, (uint64_t *)d, base, max) ;
}

size_t i64_scan_base(char const *s, int64_t *d, uint8_t base)
{
    return i64_scan_base_max(s, d, base, INT64_MAX) ;
}
