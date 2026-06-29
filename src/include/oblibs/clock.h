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
 * @brief Read the wall clock (`CLOCK_REALTIME`) into @ts.
 *
 * Thin wrapper over `clock_gettime(CLOCK_REALTIME, ts)`.
 *
 * @param[out] ts Receives the current wall-clock time. Must not be NULL (no NULL
 *                check is performed; a NULL pointer is forwarded to the syscall,
 *                which fails with `EFAULT`).
 * @return 1 on success.
 * @return 0 on error; errno is set to the `clock_gettime` errno (e.g. `EFAULT`
 *         if @ts is an invalid address; `EINVAL` if `CLOCK_REALTIME` is not
 *         supported).
 */
extern int clock_now(struct timespec *ts) ;

/**
 * @brief Write the TAI64N external stamp of @ts ('@' + 24 lowercase hex) into @s.
 *
 * Formats `@` followed by the 16-hex-digit TAI64 label (2^62 + @ts->tv_sec +
 * `CLOCK_TAI_UTC`) and the 8-hex-digit nanosecond field, via a single
 * `snprintf` bounded to `CLOCK_TAI64N_LEN + 1` bytes. The output is always
 * NUL-terminated by `snprintf`. The nanosecond field is taken as the low 32 bits
 * of @ts->tv_nsec (cast to `uint32_t`); the function does not validate that
 * @ts->tv_nsec lies in `[0, 1e9)`.
 *
 * @param[in] ts Time to stamp. Must not be NULL (no NULL check).
 * @param[out] s Buffer of at least `CLOCK_TAI64N_LEN + 1` bytes. Receives the
 *               stamp and a terminating NUL.
 * @return Always `CLOCK_TAI64N_LEN` (25), the number of characters written
 *         excluding the NUL. The label always occupies exactly 16 hex digits for
 *         every representable `time_t`, so the width is fixed and no truncation
 *         occurs. This function never fails.
 */
extern size_t clock_tai64n_fmt(char *s, struct timespec const *ts) ;

/**
 * @brief Parse a TAI64N external stamp ('@' + 24 hex) from @s into @ts.
 *
 * Exact inverse of clock_tai64n_fmt: expects `@`, then a 16-hex-digit TAI64
 * label, then an 8-hex-digit nanosecond field, and reconstructs @ts->tv_sec as
 * `label - 2^62 - CLOCK_TAI_UTC` and @ts->tv_nsec as the decoded nanoseconds.
 * Hex digits are accepted in either case. The scan is NUL-safe: a non-hex byte
 * (the terminating NUL included) ends parsing with a rejection, so the function
 * reads at most the 25 stamp bytes of a NUL-terminated string and never past the
 * NUL. Bytes after the 24th hex digit are not inspected; the caller delimits the
 * stamp (its width is the constant `CLOCK_TAI64N_LEN`).
 *
 * @param[in] s String to parse. Must not be NULL (no NULL check).
 * @param[out] ts Receives the decoded time on success; left untouched on failure.
 *                Must not be NULL on the success path.
 * @return 1 if @s holds a well-formed stamp with nanoseconds in `[0, 1e9)`.
 * @return 0 if @s does not start with `@`, contains a non-hex digit within the
 *         24-digit body, or the nanosecond field is `>= 1e9`. Sets no errno.
 */
extern int clock_tai64n_scan(char const *s, struct timespec *ts) ;

/**
 * @brief Write the local time of @ts as "YYYY-MM-DD HH:MM:SS.nnnnnnnnn" into @s.
 *
 * Converts @ts->tv_sec to broken-down local time with `localtime_r` (TZ-aware),
 * then formats it via `snprintf` bounded to `CLOCK_LOCAL_LEN + 1` bytes. The
 * output is always NUL-terminated by `snprintf`. The nanosecond field is printed
 * with `%09ld` from @ts->tv_nsec verbatim (not validated against `[0, 1e9)`).
 *
 * The year is printed with `%d` (no fixed width): for the usual 4-digit year the
 * stamp is exactly `CLOCK_LOCAL_LEN` (29) characters. For a 5-or-more-digit year
 * (or a negative year), the formatted form exceeds 29 characters, so the buffer
 * is truncated to 29 characters plus the NUL and the return value is larger than
 * the number of bytes actually written.
 *
 * @param[in] ts Time to format. Must not be NULL (no NULL check).
 * @param[out] s Buffer of at least `CLOCK_LOCAL_LEN + 1` bytes. Receives the
 *               stamp and a terminating NUL.
 * @return The number of characters the full stamp would occupy excluding the
 *         NUL (the `snprintf` return). Equal to `CLOCK_LOCAL_LEN` for a 4-digit
 *         year; greater (with output truncated) for a wider year field.
 * @note The return value of `localtime_r` is not checked. If it fails (returns
 *       NULL, e.g. `EOVERFLOW`), the `struct tm` is left indeterminate and the
 *       formatted output is undefined.
 */
extern size_t clock_local_fmt(char *s, struct timespec const *ts) ;

/**
 * @brief Read the monotonic clock (`CLOCK_MONOTONIC`) into @ts.
 *
 * Thin wrapper over `clock_gettime(CLOCK_MONOTONIC, ts)`. The monotonic domain
 * is the right one for deadlines and sleeps: it never jumps backward on a
 * wall-clock adjustment.
 *
 * @param[out] ts Receives the current monotonic time. Must not be NULL (no NULL
 *                check; a NULL pointer fails with `EFAULT`).
 * @return 1 on success.
 * @return 0 on error; errno is set to the `clock_gettime` errno (e.g. `EFAULT`
 *         if @ts is an invalid address; `EINVAL` if `CLOCK_MONOTONIC` is not
 *         supported).
 * @see clock_now for the wall clock.
 */
extern int clock_now_mono(struct timespec *ts) ;

/**
 * @brief @r = @a + @b, with a single nanosecond carry.
 *
 * Computes `tv_sec = a->tv_sec + b->tv_sec` and `tv_nsec = a->tv_nsec +
 * b->tv_nsec`, then applies at most one carry: if the nanosecond sum reaches
 * `1e9` it subtracts `1e9` and increments the seconds. With both inputs already
 * normalised to nanoseconds in `[0, 1e9)`, @r->tv_nsec is guaranteed to land in
 * `[0, 1e9)`. If an input nanosecond field is out of that range the single carry
 * does NOT fully renormalise @r. No overflow check is performed on `tv_sec`.
 *
 * @param[in] a First operand. Must not be NULL.
 * @param[in] b Second operand. Must not be NULL.
 * @param[out] r Result. Must not be NULL. Aliasing is allowed (@r may be @a or
 *               @b, or both).
 * @note Always succeeds; sets no errno.
 */
extern void clock_add(struct timespec *r, struct timespec const *a, struct timespec const *b) ;

/**
 * @brief @r = @a - @b, with a single nanosecond borrow.
 *
 * Computes `tv_sec = a->tv_sec - b->tv_sec` and `tv_nsec = a->tv_nsec -
 * b->tv_nsec`, then applies at most one borrow: if the nanosecond difference is
 * negative it adds `1e9` and decrements the seconds. With both inputs already
 * normalised to nanoseconds in `[0, 1e9)`, @r->tv_nsec is guaranteed to land in
 * `[0, 1e9)`; the seconds field may be negative, so a negative overall duration
 * is carried in @r->tv_sec while @r->tv_nsec stays non-negative. If an input
 * nanosecond field is out of `[0, 1e9)` the single borrow does NOT fully
 * renormalise @r. No overflow check is performed on `tv_sec`.
 *
 * @param[in] a Minuend. Must not be NULL.
 * @param[in] b Subtrahend. Must not be NULL.
 * @param[out] r Result. Must not be NULL. Aliasing is allowed (@r may be @a or
 *               @b, or both).
 * @note Always succeeds; sets no errno.
 */
extern void clock_sub(struct timespec *r, struct timespec const *a, struct timespec const *b) ;

/**
 * @brief @r = @a + @sec seconds; nanoseconds copied unchanged.
 *
 * Sets `r->tv_sec = a->tv_sec + (time_t)sec` and `r->tv_nsec = a->tv_nsec`.
 * @sec is cast to `time_t`; on a platform where `time_t` is narrower than
 * `int64_t` the value is truncated by that cast. No carry, no normalisation, no
 * overflow check.
 *
 * @param[in] a Base time. Must not be NULL.
 * @param[in] sec Seconds to add (may be negative).
 * @param[out] r Result. Must not be NULL. Aliasing is allowed (@r may be @a).
 * @note Always succeeds; sets no errno.
 */
extern void clock_addsec(struct timespec *r, struct timespec const *a, int64_t sec) ;

/**
 * @brief Turn a millisecond count into a @r duration.
 *
 * Sets `r->tv_sec = ms / 1000` and `r->tv_nsec = (ms % 1000) * 1000000`. Because
 * @ms is a `uint32_t`, @r->tv_sec lands in `[0, 4294967]` and @r->tv_nsec in
 * `[0, 999000000]`; the result is always normalised and cannot overflow.
 *
 * @param[in] ms Millisecond count.
 * @param[out] r Result duration. Must not be NULL.
 * @note Always succeeds; sets no errno.
 */
extern void clock_from_ms(struct timespec *r, uint32_t ms) ;

/**
 * @brief Compare @a and @b chronologically.
 *
 * Compares `tv_sec` first, then `tv_nsec`. The comparison is purely
 * field-by-field on the stored values; it does not renormalise either operand,
 * so for meaningful results both should have nanoseconds in `[0, 1e9)`.
 *
 * @param[in] a First time. Must not be NULL.
 * @param[in] b Second time. Must not be NULL.
 * @return -1 if @a < @b.
 * @return  0 if @a == @b (both fields equal).
 * @return  1 if @a > @b.
 * @note Always succeeds; sets no errno.
 */
extern int clock_cmp(struct timespec const *a, struct timespec const *b) ;

/**
 * @brief Break @ts down into local (TZ-aware) calendar fields @tm.
 *
 * Wraps `localtime_r(&ts->tv_sec, tm)`; no leap-second machinery (correct for
 * "now", @see the module header). The nanosecond field of @ts is ignored. The
 * local timezone is taken from the process TZ environment as resolved by libc.
 *
 * @param[in] ts Time to convert. Must not be NULL.
 * @param[out] tm Receives the broken-down local time. Must not be NULL. On
 *                failure its contents are indeterminate.
 * @return 1 on success (`localtime_r` returned non-NULL).
 * @return 0 on error (`localtime_r` returned NULL); errno is set to the
 *         `localtime_r` errno (e.g. `EOVERFLOW` when @ts->tv_sec cannot be
 *         represented as a local broken-down time).
 */
extern int clock_to_localtm(struct tm *tm, struct timespec const *ts) ;

/**
 * @brief Assemble local calendar fields @tm into @ts (nanoseconds set to 0).
 *
 * Wraps `mktime(tm)`: @tm is interpreted in the local TZ, normalised in place by
 * `mktime`, and the resulting `time_t` is written to @ts->tv_sec. @ts->tv_nsec
 * is set to 0 in every case, including failure. @ts->tv_sec is set to the raw
 * `mktime` return even on failure, i.e. `(time_t)-1`.
 *
 * @param[in,out] tm Local calendar fields. Must not be NULL. Normalised in place
 *                   by `mktime`.
 * @param[out] ts Receives the assembled time (nanoseconds always 0). Must not be
 *                NULL.
 * @return 1 on success (`mktime` returned a value other than `(time_t)-1`).
 * @return 0 if `mktime` returned `(time_t)-1` (the time is unrepresentable).
 *         POSIX permits `mktime` to set errno to `EOVERFLOW` on this path but
 *         does not require it, so errno is not guaranteed to be set or meaningful
 *         here.
 * @note `mktime` legitimately returns `(time_t)-1` for the representable instant
 *       one second before the epoch (1969-12-31 23:59:59 UTC); this function
 *       reports that valid instant as failure (0).
 */
extern int clock_from_localtm(struct timespec *ts, struct tm *tm) ;

/**
 * @brief Sleep until the absolute `CLOCK_MONOTONIC` deadline @deadline, EINTR-safe.
 *
 * Calls `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, deadline, 0)` in a loop,
 * retrying as long as it returns `EINTR`, so a signal interruption never ends the
 * sleep early. `clock_nanosleep` reports its error through its return value, not
 * through errno; on any non-`EINTR` error this function copies that value into
 * errno before returning. If @deadline is already in the past, the underlying
 * call returns immediately and this returns 1.
 *
 * @param[in] deadline Absolute monotonic deadline. Must not be NULL (no NULL
 *                     check; a NULL pointer yields `EFAULT`).
 * @return 1 once the deadline has been reached.
 * @return 0 on a non-`EINTR` error; errno is set to the `clock_nanosleep` error
 *         code (e.g. `EINVAL` if @deadline->tv_nsec is outside `[0, 1e9)`,
 *         `EFAULT` for an invalid address).
 */
extern int clock_deepsleep(struct timespec const *deadline) ;

/**
 * @brief Fill @tv from @ts, rounding nanoseconds to the nearest microsecond.
 *
 * Copies `tv->tv_sec = ts->tv_sec` verbatim and computes
 * `tv->tv_usec = (ts->tv_nsec + 500) / 1000`, i.e. round-half-up to the nearest
 * microsecond. The seconds field is never adjusted: when @ts->tv_nsec is in
 * `[999999500, 999999999]` the rounding yields `tv->tv_usec == 1000000` and the
 * result is left un-normalised (no carry into `tv_sec`). This matches skalibs
 * `timeval_from_tain` byte-for-byte and is acceptable because the output feeds a
 * display-only utmpx stamp.
 *
 * @param[in] ts Source time, with nanoseconds expected in `[0, 1e9)`. Must not
 *               be NULL.
 * @param[out] tv Result. Must not be NULL.
 * @note Always succeeds; sets no errno.
 */
extern void clock_to_timeval(struct timeval *tv, struct timespec const *ts) ;

/* On-wire size of a packed timespec: 8 big-endian seconds + 4 big-endian nanos. */
#define CLOCK_PACK 12

/**
 * @brief Pack @ts into @s as `CLOCK_PACK` bytes (8 BE seconds + 4 BE nanoseconds).
 *
 * Writes @ts->tv_sec cast to `uint64_t` as 8 big-endian bytes at `s[0..7]`, then
 * @ts->tv_nsec cast to `uint32_t` as 4 big-endian bytes at `s[8..11]`. A negative
 * `tv_sec`, or a `tv_nsec` outside the low 32 bits, is encoded by the two's-
 * complement / truncating cast.
 *
 * @param[in] ts Time to serialise. Must not be NULL.
 * @param[out] s Buffer of at least `CLOCK_PACK` (12) bytes.
 * @note Always succeeds; sets no errno.
 */
extern void clock_pack(char *s, struct timespec const *ts) ;

/**
 * @brief Unpack `CLOCK_PACK` bytes from @s into @ts (inverse of clock_pack).
 *
 * Reads 8 big-endian bytes into a `uint64_t` stored in @ts->tv_sec and 4
 * big-endian bytes into a `uint32_t` stored in @ts->tv_nsec. The decoded
 * unsigned values are cast to `time_t` and `long` respectively; a round trip
 * with clock_pack is exact for any @ts whose fields survive those casts (notably
 * a non-negative `tv_sec` fitting in `time_t`).
 *
 * @param[in] s Buffer of at least `CLOCK_PACK` (12) bytes.
 * @param[out] ts Receives the deserialised time. Must not be NULL.
 * @note Always succeeds; sets no errno.
 */
extern void clock_unpack(char const *s, struct timespec *ts) ;

#endif
