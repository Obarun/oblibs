/*
 * i64_fmtbase.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libstddjb/int64_fmt_generic.c
 * under license ISC. All credits go to Laurent Bercot, author of skalibs.
 * It was renamed (int64_fmt_generic -> i64_fmtbase) and integrated into oblibs
 * by Eric Vidal <eric@obarun.org> to remove the skalibs dependency. */

size_t i64_fmtbase(char *s, int64_t d, uint8_t base)
{
    if (d >= 0) return u64_fmtbase(s, (uint64_t)d, base) ;
    if (s) *s++ = '-' ;
    return 1 + u64_fmtbase(s, -(uint64_t)d, base) ;
}
