/*
 * environ_rebuild_line.c
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

#include <oblibs/environ.h>

#include <string.h>
#include <sys/types.h>//ssize_t

#include <oblibs/string.h>//get_len_until
#include <oblibs/mill.h>
#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/fmtscan.h>

/** this following function (envfile_scanoct, envfile_cclass, envfile_next, envfile_parse_config) come from:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/execline/tree/src/execline/envfile.c
 * under license ISC. All credits go to Laurent Bercot authors of these functions.
 * These functions was modified by Eric Vidal <eric@obarun.org>  */
static void envfile_scanoct (stralloc *sa, size_t pos)
{
    unsigned int u ;
    if (!stralloc_0(sa))
        log_die_nomem("stralloc") ;
    uint_oscan(sa->s + pos, &u) ;
    sa->s[pos] = u ;
    sa->len = pos+1 ;
}

static inline uint8_t envfile_cclass (char c)
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

static inline char envfile_next(char const *s,size_t *pos)
{
    char c = 0 ;
    size_t slen = strlen(s) ;
    if (*pos > slen) return -1 ;
    c = s[*pos] ;
    (*pos) += 1 ;
    return c ;
}

static inline int envfile_parse_config (char const *file, stralloc *sa)
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
    while (state < 14)
    {
        char c = envfile_next(file, &pos) ;
        uint16_t what = table[state][envfile_cclass(c)] ;
        state = what & 0x0f ;
        if (what & 0x0400) envfile_scanoct(sa, mark) ;
        if (what & 0x0100) mark = sa->len ;
        if (what & 0x1000) c = 7 + byte_chr("abtnvfr", 7, c) ;
        if (what & 0x0010) if (!stralloc_catb(sa, &c, 1)) log_die_nomem("stralloc") ;
        if (what & 0x0020) line++ ;
        if (what & 0x0080) sa->len = mark ;
        if (what & 0x0040) if (!stralloc_0(sa)) log_die_nomem("stralloc") ;
        if (what & 0x0200)
        {
        sa->s[sa->len-2] = (fmtscan_num(sa->s[sa->len-2], 16) << 4) + fmtscan_num(sa->s[sa->len-1], 16) ;
        sa->len-- ;
        }
        if (what & 0x0800) envfile_scanoct(sa, mark) ;
    }
    if (state > 14) {
        char fmt[UINT_FMT] ;
        fmt[uint_fmt(fmt, line)] = 0 ;
        log_warn_return(LOG_EXIT_ZERO, "syntax error at line: ", fmt, " in file: ", file) ;
    }

    return 1 ;
}

int environ_rebuild_line(stralloc *sa)
{
    if (!sa->len) return 0 ;
    ssize_t r = -1 ;
    size_t pos = 0, len ;
    unsigned int found = 0, idx = 0, space = 0 ;
    stralloc tmp = STRALLOC_ZERO ;
    for (;pos < sa->len; pos += strlen(sa->s + pos)+1)
    {
        len = strlen(sa->s+pos) ;
        if (!stralloc_catb(&tmp,sa->s+pos,strlen(sa->s+pos))) goto err ;
        r = get_len_until(sa->s+pos,'=') ;
        if (r >= 0) found++ ;
        /**           case key=         case '='            multiple '=' ? */
        if ((!found || (r == len-1) || (!r && (len == 1))) && found < 2) continue ;

        if (!stralloc_catb(&tmp," ",1)) goto err ;

        space++ ;
        idx++ ;
    }
    // remove the last ' '
    if (space) tmp.len--;
    stralloc_0(&tmp) ;

    sa->len = 0 ;

    if (!envfile_parse_config(tmp.s,sa))
        goto err ;

    if(!stralloc_0(sa)) goto err ;
    sa->len-- ;

    stralloc_free(&tmp) ;
    return 1 ;
    err:
        stralloc_free(&tmp) ;
        return 0 ;
}
