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

#include <sys/time.h>
#include <stdint.h>
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

/**
 * @brief Read the monotonic clock (CLOCK_MONOTONIC) into @ts.
 *
 * The monotonic domain is the right one for deadlines and sleeps: it never
 * jumps backward on a wall-clock adjustment. @see clock_now for the wall clock.
 * @return 1 on success, 0 on error (errno set).
 */
extern int clock_now_mono(struct timespec *ts) ;

/**
 * @brief @r = @a + @b. The nanosecond field of @r is renormalised to [0, 1e9).
 * Aliasing is allowed (@r may be @a or @b).
 */
extern void clock_add(struct timespec *r, struct timespec const *a, struct timespec const *b) ;

/**
 * @brief @r = @a - @b. The nanosecond field of @r is renormalised to [0, 1e9).
 * Aliasing is allowed (@r may be @a or @b).
 */
extern void clock_sub(struct timespec *r, struct timespec const *a, struct timespec const *b) ;

/**
 * @brief @r = @a + @sec seconds (@sec may be negative). Nanoseconds are copied.
 * Aliasing is allowed.
 */
extern void clock_addsec(struct timespec *r, struct timespec const *a, int64_t sec) ;

/**
 * @brief Turn a millisecond count into a @r duration: r = {ms/1000, (ms%1000)*1e6}.
 */
extern void clock_from_ms(struct timespec *r, uint32_t ms) ;

/**
 * @brief Compare @a and @b. @return <0 if @a<@b, 0 if equal, >0 if @a>@b.
 */
extern int clock_cmp(struct timespec const *a, struct timespec const *b) ;

/**
 * @brief Break @ts down into local (TZ-aware) calendar fields @tm.
 * Wraps localtime_r; no leap-second machinery (correct for "now", @see header).
 * @return 1 on success, 0 on error.
 */
extern int clock_to_localtm(struct tm *tm, struct timespec const *ts) ;

/**
 * @brief Assemble local calendar fields @tm into @ts (nanoseconds set to 0).
 * Wraps mktime: @tm is interpreted in the local TZ and normalised in place.
 * @return 1 on success, 0 if the time is unrepresentable.
 */
extern int clock_from_localtm(struct timespec *ts, struct tm *tm) ;

/**
 * @brief Sleep until the absolute MONOTONIC deadline @deadline, EINTR-safe.
 * @return 1 once the deadline is reached, 0 on a non-EINTR error (errno set).
 */
extern int clock_deepsleep(struct timespec const *deadline) ;

/**
 * @brief Fill @tv from @ts, rounding nanoseconds to the nearest microsecond.
 */
extern void clock_to_timeval(struct timeval *tv, struct timespec const *ts) ;

/* On-wire size of a packed timespec: 8 big-endian seconds + 4 big-endian nanos. */
#define CLOCK_PACK 12

/**
 * @brief Pack @ts into @s as CLOCK_PACK bytes (8 BE seconds + 4 BE nanoseconds).
 * @param s buffer of at least CLOCK_PACK bytes.
 */
extern void clock_pack(char *s, struct timespec const *ts) ;

/**
 * @brief Unpack CLOCK_PACK bytes from @s into @ts (inverse of clock_pack).
 */
extern void clock_unpack(char const *s, struct timespec *ts) ;

#endif
