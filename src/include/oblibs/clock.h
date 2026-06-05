/*
 * clock.h
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
 *
 * Wall-clock reading and timestamp formatting, oblibs-native replacement for
 * the slice of skalibs tai/djbtime used for log timestamps. It deliberately
 * avoids the TAI64 + leap-second-table machinery:
 *
 *   - the local/ISO stamp goes straight from CLOCK_REALTIME through localtime_r,
 *     so leap seconds never enter the picture (skalibs adds then subtracts them,
 *     a no-op for display);
 *   - the TAI64N external stamp needs the current UTC->TAI offset only -- a live
 *     logger never stamps a past date, so the whole historical leap table is
 *     unnecessary: a single current-offset constant (CLOCK_TAI_UTC) is correct
 *     at init without NTP, which CLOCK_TAI would not be.
 */

#ifndef OB_CLOCK_H
#define OB_CLOCK_H

#include <sys/types.h>
#include <time.h>

/* Current TAI-UTC offset in seconds. A live logger only stamps "now", so it
 * needs just the current offset, not skalibs' full historical leap table. Bump
 * this if a new leap second is ever introduced (none since 2017-01-01; leap
 * seconds are slated to be discontinued by 2035). */
#define CLOCK_TAI_UTC 37

/* Length (without the NUL) of a TAI64N external stamp: '@' + 24 lowercase hex. */
#define CLOCK_TAI64N_LEN 25

/* Length (without the NUL) of the local stamp "YYYY-MM-DD HH:MM:SS.nnnnnnnnn"
 * for a 4-digit year. */
#define CLOCK_LOCAL_LEN 29

/**
 * @brief Read the wall clock (CLOCK_REALTIME) into @ts.
 * @return 1 on success, 0 on error (errno set).
 */
extern int clock_now(struct timespec *ts) ;

/**
 * @brief Write the TAI64N external stamp of @ts ('@' + 24 hex) into @s.
 * @param s buffer of at least CLOCK_TAI64N_LEN + 1 bytes ; NUL-terminated.
 * @return the stamp length (CLOCK_TAI64N_LEN).
 */
extern size_t clock_tai64n_fmt(char *s, struct timespec const *ts) ;

/**
 * @brief Write the local time of @ts as "YYYY-MM-DD HH:MM:SS.nnnnnnnnn" into @s.
 * @param s buffer of at least CLOCK_LOCAL_LEN + 1 bytes ; NUL-terminated.
 * @return the string length.
 */
extern size_t clock_local_fmt(char *s, struct timespec const *ts) ;

#endif
