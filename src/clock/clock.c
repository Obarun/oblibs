/*
 * clock.c
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

#include <oblibs/clock.h>

#include <stdio.h>
#include <stdint.h>
#include <time.h>

int clock_now(struct timespec *ts)
{
    return clock_gettime(CLOCK_REALTIME, ts) == 0 ;
}

size_t clock_tai64n_fmt(char *s, struct timespec const *ts)
{
    /* TAI64 label = 2^62 + (unix seconds + current TAI-UTC offset). The 8-byte
     * big-endian packing skalibs hex-encodes is just the integer in %016x. */
    uint64_t label = (uint64_t)0x4000000000000000ULL + (uint64_t)ts->tv_sec + CLOCK_TAI_UTC ;
    return (size_t)snprintf(s, CLOCK_TAI64N_LEN + 1, "@%016llx%08lx",
                            (unsigned long long)label,
                            (unsigned long)(uint32_t)ts->tv_nsec) ;
}

size_t clock_local_fmt(char *s, struct timespec const *ts)
{
    struct tm tm ;
    localtime_r(&ts->tv_sec, &tm) ;
    return (size_t)snprintf(s, CLOCK_LOCAL_LEN + 1, "%d-%02d-%02d %02d:%02d:%02d.%09ld",
                            1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
                            tm.tm_hour, tm.tm_min, tm.tm_sec, (long)ts->tv_nsec) ;
}
