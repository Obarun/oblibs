/*
 * types.h
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_TYPES_H
#define OB_TYPES_H

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

/* ===========================================================================
 * Integer <-> string conversion, self-contained (no skalibs dependency).
 *
 * A single 64-bit engine performs all the work; smaller widths are macros that
 * widen losslessly to 64-bit. The behavior is identical, byte for byte, to the
 * skalibs uint*_fmt / uint*_scan family it replaces.
 *
 * Defined behavior requires base in [2,36]; base < 2 is undefined (as in skalibs).
 *
 * The conversion functions, the macro layout and the buffer-size constants are
 * derived from skalibs (headers uint64.h / uint32.h / uint16.h / types.h /
 * fmtscan.h), https://git.skarnet.org/cgi-bin/cgit.cgi/skalibs/ , under license
 * ISC. All credits go to Laurent Bercot, author of skalibs. Renamed and adapted
 * for oblibs by Eric Vidal <eric@obarun.org> to remove the skalibs dependency.
 * ===========================================================================*/

/* engine: number -> text */

/**
 * @brief Format an unsigned 64-bit integer into base-`base` text.
 * @param s     Destination buffer, or 0 to only compute the length.
 * @param x     Value to format.
 * @param base  Numeric base, in [2,36].
 * @return Number of characters the representation takes (written if s != 0).
 *         The output is NOT null-terminated.
 */
extern size_t u64_fmtbase(char *s, uint64_t x, uint8_t base) ;

/**
 * @brief Like u64_fmtbase, left-padded with '0' to at least `n` characters.
 * @param s     Destination buffer, or 0 to only compute the length.
 * @param x     Value to format.
 * @param n     Minimum field width.
 * @param base  Numeric base, in [2,36].
 * @return max(natural length, n).
 */
extern size_t u64_fmtpad(char *s, uint64_t x, size_t n, uint8_t base) ;

/**
 * @brief Format a signed 64-bit integer (leading '-' when negative).
 * @param s     Destination buffer, or 0 to only compute the length.
 * @param d     Value to format.
 * @param base  Numeric base, in [2,36].
 * @return Number of characters the representation takes.
 */
extern size_t i64_fmtbase(char *s, int64_t d, uint8_t base) ;

/* engine: text -> number */

/**
 * @brief Scan a base-`base` unsigned integer, capped at `max`.
 * @param s     Source text.
 * @param u     Result, written only if at least one digit is consumed.
 * @param base  Numeric base, in [2,36].
 * @param max   Maximum accepted value (scanning stops before overflowing it).
 * @return Number of characters consumed (0 if none).
 */
extern size_t u64_scan_base_max(char const *s, uint64_t *u, uint8_t base, uint64_t max) ;

/**
 * @brief Scan a base-`base` unsigned 64-bit integer (capped at UINT64_MAX).
 * @param s     Source text.
 * @param u     Result, written only if at least one digit is consumed.
 * @param base  Numeric base, in [2,36].
 * @return Number of characters consumed (0 if none).
 */
extern size_t u64_scan_base(char const *s, uint64_t *u, uint8_t base) ;

/**
 * @brief Scan a base-`base` signed integer (optional leading '-' or '+'), capped at `max`.
 * @param s     Source text.
 * @param d     Result, written only if at least one digit is consumed.
 * @param base  Numeric base, in [2,36].
 * @param max   Maximum accepted magnitude for positive values; negatives go down to -(max+1).
 * @return Number of characters consumed (0 if none).
 */
extern size_t i64_scan_base_max(char const *s, int64_t *d, uint8_t base, uint64_t max) ;

/**
 * @brief Scan a base-`base` signed 64-bit integer (range [INT64_MIN, INT64_MAX]).
 * @param s     Source text.
 * @param d     Result, written only if at least one digit is consumed.
 * @param base  Numeric base, in [2,36].
 * @return Number of characters consumed (0 if none).
 */
extern size_t i64_scan_base(char const *s, int64_t *d, uint8_t base) ;

/* digit helpers */

/**
 * @brief Map a digit value to its ASCII character (0->'0' .. 35->'z').
 * @param v Value in [0,35].
 * @return The matching character, or 0 if v >= 36.
 */
extern char fmt_digit(unsigned char v) ;

/**
 * @brief Map an ASCII character to its digit value for the given base.
 * @param c    The character.
 * @param base The base, in [0,36].
 * @return The digit value if valid for the base, otherwise `base` (out of range).
 */
extern unsigned char scan_digit(unsigned char c, unsigned char base) ;

/* convenience macros: format in a given base */

#define u64_fmt(s, x)   u64_fmtbase((s), (x), 10)
#define u64_ofmt(s, x)  u64_fmtbase((s), (x), 8)
#define u64_xfmt(s, x)  u64_fmtbase((s), (x), 16)
#define u64_bfmt(s, x)  u64_fmtbase((s), (x), 2)

#define u32_fmt(s, x)   u64_fmtbase((s), (uint64_t)(x), 10)
#define u32_ofmt(s, x)  u64_fmtbase((s), (uint64_t)(x), 8)
#define u32_xfmt(s, x)  u64_fmtbase((s), (uint64_t)(x), 16)
#define u32_bfmt(s, x)  u64_fmtbase((s), (uint64_t)(x), 2)

#define u16_fmt(s, x)   u64_fmtbase((s), (uint64_t)(x), 10)
#define u16_ofmt(s, x)  u64_fmtbase((s), (uint64_t)(x), 8)
#define u16_xfmt(s, x)  u64_fmtbase((s), (uint64_t)(x), 16)
#define u16_bfmt(s, x)  u64_fmtbase((s), (uint64_t)(x), 2)

#define u8_fmt(s, x)    u64_fmtbase((s), (uint64_t)(x), 10)
#define u8_ofmt(s, x)   u64_fmtbase((s), (uint64_t)(x), 8)
#define u8_xfmt(s, x)   u64_fmtbase((s), (uint64_t)(x), 16)
#define u8_bfmt(s, x)   u64_fmtbase((s), (uint64_t)(x), 2)

#define i64_fmt(s, d)   i64_fmtbase((s), (d), 10)
#define i64_ofmt(s, d)  i64_fmtbase((s), (d), 8)
#define i64_xfmt(s, d)  i64_fmtbase((s), (d), 16)
#define i64_bfmt(s, d)  i64_fmtbase((s), (d), 2)

#define i32_fmt(s, d)   i64_fmtbase((s), (int64_t)(d), 10)
#define i32_ofmt(s, d)  i64_fmtbase((s), (int64_t)(d), 8)
#define i32_xfmt(s, d)  i64_fmtbase((s), (int64_t)(d), 16)
#define i32_bfmt(s, d)  i64_fmtbase((s), (int64_t)(d), 2)

#define i16_fmt(s, d)   i64_fmtbase((s), (int64_t)(d), 10)
#define i16_ofmt(s, d)  i64_fmtbase((s), (int64_t)(d), 8)
#define i16_xfmt(s, d)  i64_fmtbase((s), (int64_t)(d), 16)
#define i16_bfmt(s, d)  i64_fmtbase((s), (int64_t)(d), 2)

#define i8_fmt(s, d)    i64_fmtbase((s), (int64_t)(d), 10)
#define i8_ofmt(s, d)   i64_fmtbase((s), (int64_t)(d), 8)
#define i8_xfmt(s, d)   i64_fmtbase((s), (int64_t)(d), 16)
#define i8_bfmt(s, d)   i64_fmtbase((s), (int64_t)(d), 2)

/* convenience macros: scan in a given base (single 64-bit engine) */

#define u64_scan(s, u)   u64_scan_base((s), (u), 10)
#define u64_oscan(s, u)  u64_scan_base((s), (u), 8)
#define u64_xscan(s, u)  u64_scan_base((s), (u), 16)
#define u64_bscan(s, u)  u64_scan_base((s), (u), 2)

#define i64_scan(s, d)   i64_scan_base((s), (d), 10)
#define i64_oscan(s, d)  i64_scan_base((s), (d), 8)
#define i64_xscan(s, d)  i64_scan_base((s), (d), 16)
#define i64_bscan(s, d)  i64_scan_base((s), (d), 2)

/* scan into a narrower width.
 *
 * The 64-bit engine is run capped at the target type's max, so it stops exactly
 * where a native per-size scan would (same bytes consumed, same value — matches
 * skalibs uintN_scan_base) and the final cast is guaranteed lossless. Provided
 * as static inline so the output pointer keeps its real type (no 8-byte write
 * into a smaller object) without exporting an extra symbol; the temporary lives
 * in a register, so there is no resource cost. */

static inline size_t u32_scan_base(char const *s, uint32_t *u, uint8_t base)
{
    uint64_t tmp ;
    size_t n = u64_scan_base_max(s, &tmp, base, UINT32_MAX) ;
    if (n) *u = (uint32_t)tmp ;
    return n ;
}

static inline size_t u16_scan_base(char const *s, uint16_t *u, uint8_t base)
{
    uint64_t tmp ;
    size_t n = u64_scan_base_max(s, &tmp, base, UINT16_MAX) ;
    if (n) *u = (uint16_t)tmp ;
    return n ;
}

static inline size_t u8_scan_base(char const *s, uint8_t *u, uint8_t base)
{
    uint64_t tmp ;
    size_t n = u64_scan_base_max(s, &tmp, base, UINT8_MAX) ;
    if (n) *u = (uint8_t)tmp ;
    return n ;
}

#define u32_scan(s, u)   u32_scan_base((s), (u), 10)
#define u32_oscan(s, u)  u32_scan_base((s), (u), 8)
#define u32_xscan(s, u)  u32_scan_base((s), (u), 16)
#define u32_bscan(s, u)  u32_scan_base((s), (u), 2)

#define u16_scan(s, u)   u16_scan_base((s), (u), 10)
#define u16_oscan(s, u)  u16_scan_base((s), (u), 8)
#define u16_xscan(s, u)  u16_scan_base((s), (u), 16)
#define u16_bscan(s, u)  u16_scan_base((s), (u), 2)

#define u8_scan(s, u)    u8_scan_base((s), (u), 10)
#define u8_oscan(s, u)   u8_scan_base((s), (u), 8)
#define u8_xscan(s, u)   u8_scan_base((s), (u), 16)
#define u8_bscan(s, u)   u8_scan_base((s), (u), 2)

/* buffer sizes: maximum characters for a width, base (excludes terminator) */

#define U64_FMT 21
#define U64_OFMT 25
#define U64_XFMT 17
#define U64_BFMT 65

#define U32_FMT 11
#define U32_OFMT 13
#define U32_XFMT 9
#define U32_BFMT 33

#define U16_FMT 6
#define U16_OFMT 7
#define U16_XFMT 5
#define U16_BFMT 17

#define U8_FMT 4
#define U8_OFMT 4
#define U8_XFMT 3
#define U8_BFMT 9

/* signed: one extra char for the leading '-' (I*_FMT == 1 + U*_FMT) */

#define I64_FMT 22
#define I64_OFMT 26
#define I64_XFMT 18
#define I64_BFMT 66

#define I32_FMT 12
#define I32_OFMT 14
#define I32_XFMT 10
#define I32_BFMT 34

#define I16_FMT 7
#define I16_OFMT 8
#define I16_XFMT 6
#define I16_BFMT 18

#define I8_FMT 5
#define I8_OFMT 5
#define I8_XFMT 4
#define I8_BFMT 10

/* ===========================================================================
 * Conversion of the POSIX system types uid_t / gid_t / pid_t <-> string.
 *
 * Static inline so the output pointer keeps its real type (no fixed-size pointer
 * cast as skalibs does — robust to any width) and the temporary lives in a
 * register (no resource cost). uid_t / gid_t are taken as unsigned (true on all
 * real targets); pid_t is signed and scanned within its strict signed range.
 * ===========================================================================*/

/* uid_t : unsigned, capped at the type's max (robust to its width) */
static inline size_t uid_fmt_base(char *s, uid_t u, uint8_t base)
{
    return u64_fmtbase(s, (uint64_t)u, base) ;
}
static inline size_t uid_scan_base(char const *s, uid_t *u, uint8_t base)
{
    uint64_t t ;
    size_t n = u64_scan_base_max(s, &t, base, (uint64_t)(uid_t)-1) ;
    if (n) *u = (uid_t)t ;
    return n ;
}

/* gid_t : unsigned, capped at the type's max */
static inline size_t gid_fmt_base(char *s, gid_t g, uint8_t base)
{
    return u64_fmtbase(s, (uint64_t)g, base) ;
}
static inline size_t gid_scan_base(char const *s, gid_t *g, uint8_t base)
{
    uint64_t t ;
    size_t n = u64_scan_base_max(s, &t, base, (uint64_t)(gid_t)-1) ;
    if (n) *g = (gid_t)t ;
    return n ;
}

/* pid_t : signed, scanned within the type's strict signed range [PID_MIN, PID_MAX] */
static inline size_t pid_fmt_base(char *s, pid_t p, uint8_t base)
{
    return i64_fmtbase(s, (int64_t)p, base) ;
}
static inline size_t pid_scan_base(char const *s, pid_t *p, uint8_t base)
{
    int64_t t ;
    size_t n = i64_scan_base_max(s, &t, base, ((uint64_t)1 << (sizeof(pid_t)*8 - 1)) - 1) ;
    if (n) *p = (pid_t)t ;
    return n ;
}

#define uid_fmt(s, u)   uid_fmt_base((s), (u), 10)
#define uid_ofmt(s, u)  uid_fmt_base((s), (u), 8)
#define uid_xfmt(s, u)  uid_fmt_base((s), (u), 16)
#define uid_bfmt(s, u)  uid_fmt_base((s), (u), 2)
#define uid_scan(s, u)  uid_scan_base((s), (u), 10)
#define uid_oscan(s, u) uid_scan_base((s), (u), 8)
#define uid_xscan(s, u) uid_scan_base((s), (u), 16)
#define uid_bscan(s, u) uid_scan_base((s), (u), 2)

#define gid_fmt(s, g)   gid_fmt_base((s), (g), 10)
#define gid_ofmt(s, g)  gid_fmt_base((s), (g), 8)
#define gid_xfmt(s, g)  gid_fmt_base((s), (g), 16)
#define gid_bfmt(s, g)  gid_fmt_base((s), (g), 2)
#define gid_scan(s, g)  gid_scan_base((s), (g), 10)
#define gid_oscan(s, g) gid_scan_base((s), (g), 8)
#define gid_xscan(s, g) gid_scan_base((s), (g), 16)
#define gid_bscan(s, g) gid_scan_base((s), (g), 2)

#define pid_fmt(s, p)   pid_fmt_base((s), (p), 10)
#define pid_ofmt(s, p)  pid_fmt_base((s), (p), 8)
#define pid_xfmt(s, p)  pid_fmt_base((s), (p), 16)
#define pid_bfmt(s, p)  pid_fmt_base((s), (p), 2)
#define pid_scan(s, p)  pid_scan_base((s), (p), 10)
#define pid_oscan(s, p) pid_scan_base((s), (p), 8)
#define pid_xscan(s, p) pid_scan_base((s), (p), 16)
#define pid_bscan(s, p) pid_scan_base((s), (p), 2)

/* buffer sizes (oversized but safe whatever the type width) */
#define UID_FMT U64_FMT
#define GID_FMT U64_FMT
#define PID_FMT I64_FMT

/* ===========================================================================
 * Bit flags helpers and mode_t comparison
 * ===========================================================================*/

// can be used like FLAGS_SET(has, want|want1|...)
#define FLAGS_SET(has, want) ((has) |= (want))
#define FLAGS_CLEAR(has, want) ((has) &= ~(want))
// return the interger of the flag if its set, 0 otherwise
#define FLAGS_ISSET(has, want) (((has) & (want)) == (want))

/** compare @mode with @stmode
 * @Return @stmode if @mode == @stmode
 * otherwise @Return 0 */
extern mode_t get_flags(mode_t mode, mode_t stmode) ;

#endif
