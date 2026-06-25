/*
 * environ_trim.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>
#include <oblibs/environ.h>
#include <oblibs/types.h>


/** this following function (environ_scanoct, environ_cclass, environ_next, environ_parse_files) come from:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/execline/tree/src/execline/envfile.c
 * under license ISC. All credits go to Laurent Bercot authors of these functions.
 * These functions was modified by Eric Vidal <eric@obarun.org> to use struct stack instead
 * of struct stralloc */

static uint8_t environ_scanoct (strbuf *sb, size_t pos)
{
    uint32_t u ;
    if (!strbuf_terminate(sb))
        log_die_nomem("strbuf") ;
    u32_oscan(sb->s + pos, &u) ;
    sb->s[pos] = u ;
    sb->len = pos+1 ;
    return (uint8_t)u ;
}

static inline uint8_t environ_cclass (char c)
{
    switch (c)
    {
        case 0 : return 0 ;
        case '#' : return 1 ;
        case '\n' : return 2 ;
        case '=' : return 3 ;
        case ' ' :
        case '\t' :
        case '\f' :
        case '\r' : return 4 ;
        case '\\' : return 5 ;
        case '\"' : return 6 ;
        case 'a' :
        case 'b' :
        case 'f' : return 7 ;
        case 'n' :
        case 'r' :
        case 't' :
        case 'v' : return 8 ;
        case '\'' :
        case '?' : return 9 ;
        case 'x' : return 10 ;
        case '0' :
        case '1' :
        case '2' :
        case '3' :
        case '4' :
        case '5' :
        case '6' :
        case '7' : return 11 ;
        case '8' :
        case '9' :
        case 'A' :
        case 'B' :
        case 'c' :
        case 'C' :
        case 'd' :
        case 'D' :
        case 'e' :
        case 'E' :
        case 'F' : return 12 ;
        default : return 13 ;
    }
}

static inline char environ_next(char const *s,size_t *pos)
{
    char c = 0 ;
    size_t slen = strlen(s) ;
    if (*pos > slen) return -1 ;
    c = s[*pos] ;
    (*pos) += 1 ;
    return c ;
}

int environ_trim (strbuf *sb, char const *file)
{
    static uint16_t const table[14][14] =
    {
        { 0x000e, 0x0001, 0x0020, 0x000f, 0x0000, 0x000f, 0x000f, 0x0012, 0x0012, 0x000f, 0x0012, 0x0012, 0x0012, 0x0012 },
        { 0x000e, 0x0001, 0x0020, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001 },
        { 0x000f, 0x0012, 0x000f, 0x0014, 0x0003, 0x000f, 0x000f, 0x0012, 0x0012, 0x000f, 0x0012, 0x0012, 0x0012, 0x0012 },
        { 0x000f, 0x000f, 0x000f, 0x0014, 0x0003, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f },
        { 0x004e, 0x0015, 0x0060, 0x0015, 0x0004, 0x0008, 0x0007, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015 },
        { 0x004e, 0x0015, 0x0060, 0x0015, 0x0116, 0x0008, 0x0007, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015 },
        { 0x00ce, 0x0015, 0x00d0, 0x0015, 0x0016, 0x0008, 0x0007, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015 },
        { 0x000f, 0x0017, 0x000f, 0x0017, 0x0017, 0x0009, 0x0005, 0x0017, 0x0017, 0x0017, 0x0017, 0x0017, 0x0017, 0x0017 },
        { 0x000f, 0x000f, 0x0025, 0x000f, 0x0015, 0x0015, 0x0015, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f },
        { 0x000f, 0x0017, 0x0027, 0x0017, 0x0017, 0x0017, 0x0017, 0x1017, 0x1017, 0x0017, 0x000a, 0x011c, 0x0017, 0x0017 },
        { 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x001b, 0x000f, 0x000f, 0x000f, 0x001b, 0x001b, 0x000f },
        { 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x0217, 0x000f, 0x000f, 0x000f, 0x0217, 0x0217, 0x000f },
        { 0x000f, 0x0417, 0x000f, 0x0417, 0x0417, 0x0409, 0x0405, 0x0417, 0x0417, 0x0417, 0x0417, 0x001d, 0x0417, 0x0417 },
        { 0x000f, 0x0417, 0x000f, 0x0417, 0x0417, 0x0409, 0x0405, 0x0417, 0x0417, 0x0417, 0x0417, 0x0817, 0x0417, 0x0417 }
    } ;
    unsigned int line = 1 ;
    size_t mark = 0, pos = 0, start = sb->len ;
    uint8_t state = 0, badbyte = 0 ;
    /* badbyte: a value byte decoded to a separator the sbl/text layout reserves --
     * NUL (0x00, the sbl entry separator) or newline (0x0a, the line separator in
     * the joined text form). Either one can only be produced by an escape inside a
     * quoted value -- the \n letter escape, or octal/hex (\012, \0, \x0a, \x00) --
     * so we reject it at each production point. A value containing one cannot round
     * -trip through the module (environ_untrim/environ_get_value/environ_substitute
     * walk entries by those separators), so an environment value must not contain
     * one. The table itself cannot catch the octal/hex forms: it is keyed on the
     * input character, not on the byte the escape computes. The letter escape only
     * yields 0x07..0x0d, so 0x0a is the only reserved byte reachable there. */
    while (state < 14) {
        char c = environ_next(file, &pos) ;
        uint16_t what = table[state][environ_cclass(c)] ;
        state = what & 0x0f ;
        if (what & 0x0400) { uint8_t b = environ_scanoct(sb, mark) ; if (b == '\0' || b == '\n') { badbyte = 1 ; break ; } }
        if (what & 0x0100) mark = sb->len ;
        if (what & 0x1000) {
            static char const esc[] = "abtnvfr" ;
            char const *p = memchr(esc, c, 7) ;
            c = 7 + (p ? p - esc : 7) ;
            if (c == '\n') { badbyte = 1 ; break ; }   /* the \n letter escape */
        }
        if (what & 0x0010) if (!strbuf_catb(sb, &c, 1)) log_die_nomem("strbuf") ;
        if (what & 0x0020) line++ ;
        if (what & 0x0080) sb->len = mark ;
        if (what & 0x0040) if (!strbuf_terminate(sb)) log_die_nomem("strbuf") ;
        if (what & 0x0200)
        {
        sb->s[sb->len-2] = (scan_digit(sb->s[sb->len-2], 16) << 4) + scan_digit(sb->s[sb->len-1], 16) ;
        sb->len-- ;
        if (sb->s[sb->len-1] == '\0' || sb->s[sb->len-1] == '\n') { badbyte = 1 ; break ; }   /* \x00 / \x0a hex escape */
        }
        if (what & 0x0800) { uint8_t b = environ_scanoct(sb, mark) ; if (b == '\0' || b == '\n') { badbyte = 1 ; break ; } }
    }
    if (badbyte) {
        sb->len = start ;
        log_warn_return(LOG_EXIT_ZERO, "a NUL or newline byte in a value is not allowed in file: ", file) ;
    }
    if (state > 14) {
        char fmt[U32_FMT] ;
        fmt[u32_fmt(fmt, line)] = 0 ;
        log_warn_return(LOG_EXIT_ZERO, "syntax error at line: ", fmt, " in file: ", file) ;
    }

    /* close the list: valid C string even when empty (NUL not counted) */
    if (!strbuf_uncounted(sb))
        return 0 ;
    return 1 ;
}

