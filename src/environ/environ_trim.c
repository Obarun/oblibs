/*
 * environ_trim.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/fmtscan.h>

#include <oblibs/log.h>
#include <oblibs/environ.h>


/** this following function (environ_scanoct, environ_cclass, environ_next, environ_parse_files) come from:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/execline/tree/src/execline/envfile.c
 * under license ISC. All credits go to Laurent Bercot authors of these functions.
 * These functions was modified by Eric Vidal <eric@obarun.org> to use struct stack instead
 * of struct stralloc */

static void environ_scanoct (stack *stk, size_t pos)
{
    unsigned int u ;
    if (!stack_add(stk, "", 1))
        log_die_nomem("stack") ;
    uint_oscan(stk->s + pos, &u) ;
    stk->s[pos] = u ;
    stk->len = pos+1 ;
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

int environ_trim (stack *stk, char const *file)
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
    size_t mark = 0, pos = 0 ;
    uint8_t state = 0 ;
    while (state < 14) {
        char c = environ_next(file, &pos) ;
        uint16_t what = table[state][environ_cclass(c)] ;
        state = what & 0x0f ;
        if (what & 0x0400) environ_scanoct(stk, mark) ;
        if (what & 0x0100) mark = stk->len ;
        if (what & 0x1000) c = 7 + byte_chr("abtnvfr", 7, c) ;
        if (what & 0x0010) if (!stack_add(stk, &c, 1)) log_die_nomem("stack") ;
        if (what & 0x0020) line++ ;
        if (what & 0x0080) stk->len = mark ;
        if (what & 0x0040) if (!stack_add(stk,"",1)) log_die_nomem("stack") ;
        if (what & 0x0200)
        {
        stk->s[stk->len-2] = (fmtscan_num(stk->s[stk->len-2], 16) << 4) + fmtscan_num(stk->s[stk->len-1], 16) ;
        stk->len-- ;
        }
        if (what & 0x0800) environ_scanoct(stk, mark) ;
    }
    if (state > 14) {
        char fmt[UINT_FMT] ;
        fmt[uint_fmt(fmt, line)] = 0 ;
        log_warn_return(LOG_EXIT_ZERO, "syntax error at line: ", fmt, " in file: ", file) ;
    }

    return stack_close(stk) ;
}

