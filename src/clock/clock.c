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
 * except according to the terms contained in the LICENSE file.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#include <oblibs/clock.h>
#include <oblibs/types.h>

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

int clock_now_mono(struct timespec *ts)
{
    return clock_gettime(CLOCK_MONOTONIC, ts) == 0 ;
}

void clock_add(struct timespec *r, struct timespec const *a, struct timespec const *b)
{
    time_t sec = a->tv_sec + b->tv_sec ;
    long nano = a->tv_nsec + b->tv_nsec ;
    if (nano >= 1000000000L) { nano -= 1000000000L ; sec++ ; }
    r->tv_sec = sec ;
    r->tv_nsec = nano ;
}

void clock_sub(struct timespec *r, struct timespec const *a, struct timespec const *b)
{
    time_t sec = a->tv_sec - b->tv_sec ;
    long nano = a->tv_nsec - b->tv_nsec ;
    if (nano < 0) { nano += 1000000000L ; sec-- ; }
    r->tv_sec = sec ;
    r->tv_nsec = nano ;
}

void clock_addsec(struct timespec *r, struct timespec const *a, int64_t sec)
{
    r->tv_sec = a->tv_sec + (time_t)sec ;
    r->tv_nsec = a->tv_nsec ;
}

void clock_from_ms(struct timespec *r, uint32_t ms)
{
    r->tv_sec = ms / 1000 ;
    r->tv_nsec = (long)(ms % 1000) * 1000000L ;
}

int clock_cmp(struct timespec const *a, struct timespec const *b)
{
    if (a->tv_sec != b->tv_sec) return a->tv_sec < b->tv_sec ? -1 : 1 ;
    if (a->tv_nsec != b->tv_nsec) return a->tv_nsec < b->tv_nsec ? -1 : 1 ;
    return 0 ;
}

int clock_to_localtm(struct tm *tm, struct timespec const *ts)
{
    return localtime_r(&ts->tv_sec, tm) != 0 ;
}

int clock_from_localtm(struct timespec *ts, struct tm *tm)
{
    time_t t = mktime(tm) ;
    ts->tv_sec = t ;
    ts->tv_nsec = 0 ;
    return t != (time_t)-1 ;
}

int clock_deepsleep(struct timespec const *deadline)
{
    int r ;
    while ((r = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, deadline, 0)) == EINTR) ;
    if (r) { errno = r ; return 0 ; }   /* clock_nanosleep returns the errno, it does not set it */
    return 1 ;
}

void clock_to_timeval(struct timeval *tv, struct timespec const *ts)
{
    tv->tv_sec = ts->tv_sec ;
    /* Round to nearest usec, matching skalibs timeval_from_tain byte-for-byte.
     * Like skalibs, the < 0.5us window where (nano+500)/1000 reaches 1000000
     * is left un-normalised: this is a display-only utmpx stamp. */
    tv->tv_usec = (ts->tv_nsec + 500) / 1000 ;
}

void clock_pack(char *s, struct timespec const *ts)
{
    u64_pack_big(s, (uint64_t)ts->tv_sec) ;
    u32_pack_big(s + 8, (uint32_t)ts->tv_nsec) ;
}

void clock_unpack(char const *s, struct timespec *ts)
{
    uint64_t sec ;
    uint32_t nano ;
    u64_unpack_big(s, &sec) ;
    u32_unpack_big(s + 8, &nano) ;
    ts->tv_sec = (time_t)sec ;
    ts->tv_nsec = (long)nano ;
}
