/*
 * fmt_digit.c
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
 * https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/tree/src/libstddjb/fmtscan_asc.c
 * under license ISC. All credits go to Laurent Bercot, author of skalibs.
 * It was renamed (fmtscan_asc -> fmt_digit) and integrated into oblibs
 * by Eric Vidal <eric@obarun.org> to remove the skalibs dependency. */

char fmt_digit(unsigned char v)
{
    static char const *tab = "0123456789abcdefghijklmnopqrstuvwxyz" ;
    return (v >= 36) ? 0 : tab[v] ;
}
