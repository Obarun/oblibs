/*
 * subst.c
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
 * The substitution engine below is adapted from execline's el_substitute:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/execline/tree/src/libexecline/el_substitute.c
 * under license ISC. All credits go to Laurent Bercot, author of execline.
 *
 * Why this port: it lets oblibs drop its execline dependency entirely (the
 * ${VAR} substitution was the only reason oblibs linked execline). Re-owning
 * the code also dissolves the last skalibs island it forced -- exlsn_t's
 * genalloc was locked by libexecline's exlsn_free ABI; with our own type and
 * free function the per-variable array becomes an oblibs genbuf. See the
 * memory note "escaping-two-paths" for the escaping behaviour that had to be
 * preserved.
 */

/* Faithful port: the parseword/substword state machine is reproduced verbatim
 * (only stralloc->strbuf, genalloc->genbuf, byte_chr->memchr), so the output is
 * byte-for-byte identical to el_substitute -- proven by a differential test.
 * The single narrowing: oblibs only ever builds entries with n in {0,1}, but
 * the n-value cross-product machinery is kept for that equivalence. */

#include <oblibs/subst.h>

#include <string.h>

#include <oblibs/strbuf.h>
#include <oblibs/genbuf.h>

/* Internal cursor over a pending substitution inside the current word. */
typedef struct subsu_s
{
    subst_entry_t const *subst ;
    size_t pos ;
} subsu_t ;

typedef struct subsuinfo_s
{
    strbuf dst ;
    strbuf sa ;
    genbuf list ;   /* array of subsu_t */
    char const *values ;
} subsuinfo_t ;

#define SUBSUINFO_ZERO { .dst = STRBUF_ZERO, .sa = STRBUF_ZERO, .list = GENBUF_ZERO, .values = 0 }

#define TEST 0x80
#define MARK 0x40
#define KEEPESC 0x20
#define INCRESC 0x10

#define STATE 0x07
#define INWORD 0x00
#define INDOLL 0x01
#define INDBR 0x02
#define INVAR 0x03
#define INVARBR 0x04
#define ACCEPT 0x05

/* Index of @c in the 5-byte class set, or 5 if absent (was skalibs byte_chr). */
static inline size_t class_index(char const *class, char c)
{
    void const *p = memchr(class, c, 5) ;
    return p ? (size_t)((char const *)p - class) : 5 ;
}

static ssize_t parseword(strbuf *sa, genbuf *list, char const *s, char const *vars, subst_entry_t const *substs, unsigned int nsubst)
{
    static char const class[5] = { 0, '\\', '$', '{', '}' } ;
    static unsigned char const table[6][5] =
    {
        { ACCEPT, ACCEPT, ACCEPT, ACCEPT | TEST, ACCEPT },
        { INWORD | KEEPESC | INCRESC, INWORD | INCRESC, INWORD | INCRESC, INWORD | TEST | INCRESC, INWORD | INCRESC },
        { INDOLL | KEEPESC, INDOLL, INDOLL, INDOLL | TEST, INDOLL },
        { INWORD, INDBR | KEEPESC, INWORD, INWORD | TEST, INWORD },
        { INWORD, INWORD, INWORD, INWORD | TEST, INWORD | TEST },
        { INWORD, INVAR | MARK | KEEPESC, INVARBR | MARK | KEEPESC, INVAR | KEEPESC, INVARBR | KEEPESC }
    } ;

    /* salen/listlen are BYTE lengths, used to roll @sa/@list back on error. For
     * @list this is list->len (bytes), NOT genbuf_len() (element count): the err
     * path restores list->len directly. (execline's original wrote genalloc_len
     * here, a latent bug masked by the list always being empty on entry.) */
    size_t mark = 0, offset = 0, esc = 0, salen = sa->len, listlen = list->len ;
    ssize_t pos = 0 ;
    unsigned char state = INWORD ;

    while (state != ACCEPT)
    {
        int nopush = 0 ;
        unsigned char c = table[class_index(class, s[pos])][state] ;

        if (c & TEST)
        {
            unsigned int supp = (state == INVARBR) ;
            unsigned int i = 0 ;
            for (; i < nsubst ; i++)
            {
                if (!strncmp(vars + substs[i].var, s + mark, pos - mark) && !vars[substs[i].var + pos - mark])
                {
                    sa->len -= esc >> 1 ; offset += esc >> 1 ;
                    if (esc & 1)
                    {
                        memmove(sa->s + mark - offset - 2 - supp, sa->s + mark - offset + (esc >> 1) - 1 - supp, pos - mark + 1 + supp) ;
                        sa->len-- ; offset++ ;
                    }
                    else
                    {
                        subsu_t cur ;
                        cur.subst = substs + i ;
                        cur.pos = mark - offset - 1 - supp ;
                        if (!genbuf_append(subsu_t, list, &cur)) goto err ;
                        offset += sa->len - cur.pos ;
                        sa->len = cur.pos ;
                        if (supp) nopush = 1 ;
                    }
                    break ;
                }
            }
        }
        if (nopush) offset++ ;
        else if (!strbuf_catb(sa, s + pos, 1)) goto err ;
        if (c & MARK) mark = pos ;
        if (!(c & KEEPESC)) esc = 0 ;
        if (c & INCRESC) esc++ ;
        state = c & STATE ; pos++ ;
    }
    sa->len-- ;
    return pos ;

err:
    sa->len = salen ;
    list->len = listlen ;
    return -1 ;
}

static int substword(subsuinfo_t *info, size_t wordstart, size_t wordlen, unsigned int n, size_t offset)
{
    if (n < genbuf_len(subsu_t, &info->list))
    {
        subsu_t *list = genbuf_s(subsu_t, &info->list) ;
        char const *p = info->values + list[n].subst->value ;
        size_t l = list[n].pos + offset ;
        size_t dstbase = info->dst.len ;
        size_t sabase = info->sa.len ;
        unsigned int i = 0 ;
        int nc = 0 ;
        if (!strbuf_reserve(&info->sa, info->sa.len + l)) return -1 ;
        strbuf_catb(&info->sa, info->sa.s + wordstart, l) ;
        for (; i < list[n].subst->n ; i++)
        {
            size_t plen = strlen(p) ;
            int r ;
            info->sa.len = sabase + l ;
            if (!strbuf_reserve(&info->sa, info->sa.len + plen + wordlen - l)) goto err ;
            strbuf_catb(&info->sa, p, plen) ;
            strbuf_catb(&info->sa, info->sa.s + wordstart + l, wordlen - l) ;
            r = substword(info, sabase, info->sa.len - sabase, n + 1, offset + plen) ;
            if (r < 0) goto err ;
            nc += r ;
            p += plen + 1 ;
        }
        return nc ;
    err:
        info->sa.len = sabase ;
        info->dst.len = dstbase ;
        return -1 ;
    }
    else
    {
        if (!strbuf_reserve(&info->dst, info->dst.len + wordlen + 1)) return -1 ;
        strbuf_catb(&info->dst, info->sa.s + wordstart, wordlen) ;
        strbuf_terminate(&info->dst) ;
        return 1 ;
    }
}

void subst_free(subst_t *ctx)
{
    strbuf_free(&ctx->vars) ;
    strbuf_free(&ctx->values) ;
    genbuf_free(&ctx->data) ;
    strbuf_free(&ctx->modifs) ;
}

int subst(strbuf *dst, char const *src, size_t len, subst_t const *ctx)
{
    subsuinfo_t info = SUBSUINFO_ZERO ;
    char const *vars = ctx->vars.s ;
    subst_entry_t const *substs = genbuf_s(subst_entry_t const, &ctx->data) ;
    unsigned int nsubst = (unsigned int)genbuf_len(subst_entry_t const, &ctx->data) ;
    size_t i = 0 ;
    int nc = 0 ;
    int wasnull = !dst->s ;

    if (!strbuf_copy(&info.dst, dst)) return -1 ;
    info.values = ctx->values.s ;

    while (i < len)
    {
        info.list.len = 0 ;
        info.sa.len = 0 ;
        {
            ssize_t r = parseword(&info.sa, &info.list, src + i, vars, substs, nsubst) ;
            if (r < 0) goto err ;
            i += r ;
        }
        {
            int r = substword(&info, 0, info.sa.len, 0, 0) ;
            if (r < 0) goto err ;
            nc += r ;
        }
    }
    genbuf_free(&info.list) ;
    strbuf_free(&info.sa) ;
    if (!wasnull) strbuf_free(dst) ;
    *dst = info.dst ;
    return nc ;

err:
    genbuf_free(&info.list) ;
    strbuf_free(&info.sa) ;
    strbuf_free(&info.dst) ;
    return -1 ;
}
