/*
 * mill.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 * */

#include <string.h>
#include <sys/types.h>

#include <oblibs/mill.h>
#include <oblibs/log.h>
#include <oblibs/stack.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

char mill_next(char const *s, size_t *pos)
{
    char c = 0 ;
    size_t slen = strlen(s) ;
    if (*pos > slen) return -1 ;
    c = s[*pos] ;
    (*pos) += 1 ;
    return c ;
}

uint8_t mill_cclass (parse_mill_t *p)
{
    uint8_t *nopen = &p->inner.nopen ;
    uint8_t *nclose = &p->inner.nclose ;
    uint8_t opclequal = 0 ;
    uint8_t range = 0 ;// 0-> out, 1->in
    uint8_t firstopen = 0 ;
    uint8_t r = 0 ;
    char curr = p->inner.curr ;

    if (p->open == p->close)
        opclequal++ ;

    /** open */
    if (curr == p->open)
    {
        if (!(*nopen)) firstopen = 1 ;
        (*nopen)++ ;
    }
    /** close */
    if (curr == p->close)
    {
        //if (!(*nclose)) firstclose = 1 ;
        /** we can have a close before an open*/
        if (*nopen)
        {
            (*nclose)++ ;
            /** special exist when close and open are equal
            * in this case do not considere close if we are
            * on firstopen */
            if (opclequal && firstopen)
                (*nclose)-- ;
        }
    }
    if (curr == '\n')
    {
        if (p->flush) p->inner.flushed = 1 ;
        p->inner.jumped = 0 ;
        p->inner.nline++ ;
    }
    /** we opened already but not closed */
    if ((*nopen))
        if (!(*nclose) || (*nopen) != (*nclose)) range = 1 ;

    DBG_PARSER ("   nopen:%i::nclose:%i::opclequal:%i::firstopen:%i::",*nopen,*nclose,opclequal,firstopen) ;

    /** jump */
    r = mill_inner_loop(curr,p->jump,p->jumplen) ;
    if (r)
    {
        p->inner.jumped = 1 ;
        return JUMP ;
    }
    /** search */
    r = mill_inner_loop(curr,p->search,p->searchlen) ;
    if (r)
    {
        p->inner.search = 1 ;
        return END ;
    }
    /** end */
    r = mill_inner_loop(curr,p->end,p->endlen) ;
    if (r) return END ;


    if ((*nopen) && !(*nclose))
    {
        /** skip */
        r = mill_inner_loop(curr,p->skip,p->skiplen) ;
        if (r)
        {
            if (!p->forceskip) return KEEP ;
            else return IGN ;
        }

        /** key to key */
        if (firstopen && !p->forceopen && !p->keepopen) return IGN ;
        else return KEEP ;
    }
    else if ((*nclose))
    {
        /** skip */
        r = mill_inner_loop(curr,p->skip,p->skiplen) ;
        if (r)
            if (!p->forceskip) return KEEP ;
        if (p->forceclose) return END ;
        if (*nopen == *nclose) return END ;
        if (range) return KEEP ;
    }

    return IGN ;
}

/** @Return 1 on sucess/end of string
 * @Return -1 system error
 * @Return 0 parsing error */
int mill_config(stack *stk, parse_mill_t *p, char const *string, size_t *pos)
{
    static uint8_t const table[6] = { IGN, KEEP, JUMP, END, EFILE } ;
    uint8_t what = 0, efile = 0 ;
    char const *file = p->file ;
    char j = 0 ;
    int res = -2 ;

    DBG_PARSER ("[PARSE PROCESS OF %s STARTS]", p->inner.debug) ;
    DBG_PARSER ("open:%i:%c:",p->open,p->open) ;
    DBG_PARSER ("close:%i:%c:",p->close,p->close) ;
    DBG_PARSER ("keepopen:%i:",p->keepopen) ;
    DBG_PARSER ("keeclose:%i:",p->keepclose) ;
    DBG_PARSER ("forceopen:%i:",p->forceopen) ;
    DBG_PARSER ("forceclose:%i:",p->forceclose) ;
    DBG_PARSER ("forceskip:%i:",p->forceskip) ;
    DBG_PARSER ("flush:%i:\n----\n",p->flush) ;

    while (res < -1)
    {
        DBG_PARSER ("   parse line:%s:",string+(*pos)) ;
        DBG_PARSER ("   len of line:%li:",strlen(string)) ;

        p->inner.curr = mill_next(string, pos) ;
        // end of file
        if (p->inner.curr == -1) what = EFILE ;
        else what = table[mill_cclass(p)] ;

        if (p->inner.flushed)
        {
            stk->len = 0 ;
            wild_zero(p) ;
            what = IGN ;
        }

        // end of file
        //if (p->inner.curr == -1) what = EFILE ;
        DBG_PARSER ("   pos:%li::current:%c:",*pos,p->inner.curr) ;
        DBG_PARSER ("   nopen:%i::nclose:%i::what:%li:\n    ----\n",p->inner.nopen, p->inner.nclose,what) ;

        switch(what)
        {
            case KEEP:
                if (p->inner.nopen)
                    if (!stack_add(stk, &p->inner.curr, 1))
                        return -1 ;

                DBG_PARSER("    string state:%s:\n", stk->s) ;
                break ;
            case JUMP:
                    while (j != '\n')
                    {
                        j = mill_next(string, pos) ;
                        if (j < 0) {
                             res = 1 ;
                             break ; //end of string
                        }
                    }
                    p->inner.nline++ ;
                    res = 1 ;
                break ;
            case IGN:
                break ;
            case EFILE:
                efile++ ;
                res = 1 ;
                break ;
            case END:
                if (p->keepclose)
                    if (!stack_add(stk, &p->inner.curr, 1))
                        return -1 ;
                res = 1 ;
                break ;
            default: break ;
        }
    }
    p->inner.lastclose = (*pos) ;
    if (p->inner.search || p->inner.jumped) stk->len = 0 ;
    else
    {
        if (!stack_close(stk)) return 0 ;

    }
    DBG_PARSER("what:%s:efile:%i:res:%i:", !what ? "ign" : \
                                what == 1 ? "keep" : \
                                what == 2 ? "jump" : \
                                what == 3 ? "end" : "efile",efile,res) ;
    DBG_PARSER ("string kept::%s::len:%li:",kp->s,kp->len) ;
    DBG_PARSER ("[PARSE PROCESS OF %s END]\n", p->inner.debug) ;

    {
        if (p->search && !p->inner.search) return 0 ;
        else if (p->jump && !p->inner.jumped) return 0 ;

        if (!p->forceopen)
            if (p->open && !p->inner.nopen){ stk->len = 0 ; return 0 ; }

        if (!p->forceclose)
            if (p->close && !p->inner.nclose){ stk->len = 0 ; return 0 ; }
    }

    if (p->check && p->inner.nopen != p->inner.nclose)
    {
        char fmt[UINT_FMT] ;
        fmt[uint_fmt(fmt, p->inner.nline-1)] = 0 ;
        char sepopen[2] = { p->open,0 } ;
        char sepclose[2] = { p->close,0 } ;
        log_warn("umatched ",(p->inner.nopen > p->inner.nclose) ? sepopen : sepclose," in: ",file," at line: ",fmt) ;
        return 0 ;
    }
    return res ;
}

int mill_prepare(stack *stk, char const *string, parse_mill_t *sep, size_t *pos)
{
    int r = 0 ;
    size_t newpos = (*pos), ebase = sep->endlen, i = 0 ;
    static char tend[64+2] ;

    _init_stack_(tstk, strlen(string)) ;

    memset(tend,0,66) ;

    parse_mill_t csep = PARSE_MILL_ZERO ;
    mill_copy(&csep,sep) ;

    csep.inner.nclose = csep.inner.nopen = 0 ;

    /** end and skip cannot be the same */
    for (; i < csep.endlen ; i++)
    {
        if (mill_inner_loop(csep.end[i],csep.skip,csep.skiplen))
            goto err ;
    }

    /** open,close and skip cannot be the same */
    i = 0 ;
    for (;i < csep.skiplen ; i++)
        if ((csep.skip[i] == csep.open)  || (csep.skip[i] == csep.close))
            goto err ;

    /** prepare sep->end */
    i = 0 ;
    while (i < ebase){
        tend[i] = csep.end[i] ;
        i++ ;
    }
     /** In case of search and jump
     * open, close, check, skip,
     * forceopen, forceclose, forceskip must be 0
     * this limit avoid complex exit status.
     * Reverse is also true */

    if (csep.search || csep.jump)
    {
        csep.open = csep.close = csep.check = csep.skiplen = 0 ;
        csep.skip = 0 ;
        csep.forceopen = csep.forceclose = csep.forceskip = 0 ;
    }
    else
    {
        csep.jumplen = csep.searchlen = 0 ;
        csep.search = csep.jump = 0 ;
        if (csep.forceopen)
            csep.open = string[(*pos)] ;

        if (csep.forceclose)
        {
            tend[i++] = string[strlen(string)] ;
            csep.endlen = ebase + 1 ;
        }
    }

    tend[i] = 0 ;
    csep.end = tend ;
    r = mill_config(&tstk, &csep, string, &newpos) ;
    sep->inner.pos = newpos ;
    sep->inner.nline = csep.inner.nline ;
    if (r == -1) goto err ;
    else if (!r || !tstk.len) goto freed ;
    if (!stack_add(stk, tstk.s, tstk.len + 1)) goto err ;
    freed:
        (*pos) = newpos ;
        return r ;
    err:
        (*pos) = newpos ;
        return -1 ;
}

int mill_element(stack *stk, char const *string, parse_mill_t *sep, size_t *pos)
{
    int r = 1 ;
    if (!string) return 0 ;
    _init_stack_(tstk, strlen(string) + 1) ;
    r = mill_prepare(&tstk, string, sep, pos) ;
    if (r == -1) return -1 ;
    else if (!r || !tstk.len) return r ;
    if (!stack_add(stk, tstk.s, strlen(tstk.s) + 1)) return -1 ;
    return r ;
}

int mill_string(stack *stk, char const *string, size_t len, parse_mill_t *sep)
{
    if (!len) return 0 ;
    int r = 0 ;
    size_t newpos = 0, pos = 0 ;

    while (pos < len) {
        newpos = 0 ;
        r = mill_element(stk, string + pos, sep, &newpos) ;
        if (r == -1) return -1 ;
        if (!r) return 0 ;
        pos += newpos ;
        sep->inner.pos = pos ;
    }
    return 1 ;
}

int mill_nstring(stack *stk, char const *string, size_t len, parse_mill_t *sep)
{
    if (!len) return 0 ;
    int r = 0 ;
    size_t newpos = 0, pos = 0 ;

    for (; pos < len; pos += strlen(string + pos) + 1) {
        newpos = 0 ;
        r = mill_element(stk, string + pos, sep, &newpos) ;
        if (r == -1) return -1 ;
        if (!r) return 0 ;
        sep->inner.pos += newpos ;
    }
    return 1 ;
}

// helper
int mill_inner_loop(char curr,char const *s,size_t len)
{
    size_t i = 0 ;
    for (i = 0 ; i < len ; i++)
        if (curr == s[i])
            return 1 ;
    return 0 ;
}

void wild_zero(parse_mill_t *m)
{
    m->inner.nopen = m->inner.nclose = m->inner.jumped = m->inner.flushed = m->inner.search = 0 ;
}

void wild_zero_nline(parse_mill_t *m)
{
    m->inner.nline = 1 ;
}

void wild_zero_pos(parse_mill_t *m)
{
    m->inner.pos = m->inner.lastclose = 0 ;
}

void wild_zero_all(parse_mill_t *m)
{
    wild_zero(m) ;
    wild_zero_nline(m) ;
    wild_zero_pos(m) ;
}

void mill_copy(parse_mill_t *cp, parse_mill_t *m)
{
    cp->file = m->file ;
    cp->open = m->open ;
    cp->close = m->close ;
    cp->skip = m->skip ;
    cp->skiplen = m->skiplen ;
    cp->end = m->end ;
    cp->endlen = m->endlen ;
    cp->jump = m->jump ;
    cp->jumplen = m->jumplen ;
    cp->search = m->search ;
    cp->searchlen = m->searchlen ;
    cp->check = m->check ;
    cp->flush = m->flush ;
    cp->keepopen = m->keepopen ;
    cp->keepclose = m->keepclose ;
    cp->forceskip = m->forceskip ;
    cp->forceopen = m->forceopen ;
    cp->forceclose = m->forceclose ;
    cp->inner.curr = m->inner.curr ;
    cp->inner.nline = m->inner.nline ;
    cp->inner.nopen = m->inner.nopen ;
    cp->inner.nclose = m->inner.nclose ;
    cp->inner.jumped = m->inner.jumped ;
    cp->inner.flushed = m->inner.flushed ;
    cp->inner.search = m->inner.search ;
    cp->inner.debug = m->inner.debug ;
    cp->inner.pos = m->inner.pos ;
    cp->inner.lastclose = m->inner.lastclose ;
}

