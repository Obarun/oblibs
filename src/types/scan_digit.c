/*
 * scan_digit.c
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
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libstddjb/fmtscan_num.c
 * under license ISC. All credits go to Laurent Bercot, author of skalibs.
 * It was renamed (fmtscan_num -> scan_digit) and integrated into oblibs
 * by Eric Vidal <eric@obarun.org> to remove the skalibs dependency. */

unsigned char scan_digit(unsigned char c, unsigned char base)
{
    return
        ((c < '0') || (base > 36)) ? base :
        (base <= 10) ? (c - '0' <= base) ? c - '0' : base :
        (c - '0' <= 9) ? c - '0' :
        (c < 'A') ? base :
        (c - 'A' < base - 10) ? c - 'A' + 10 :
        (c < 'a') ? base :
        (c - 'a' < base - 10) ? c - 'a' + 10 :
        base ;
}
