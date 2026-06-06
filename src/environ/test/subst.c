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
 */

/* Tests for subst, the variable-substitution engine (oblibs-native, replacing
 * execline's el_substitute). Each expected output was CAPTURED from a byte-exact
 * differential run against el_substitute and is now pinned as a literal, so the
 * test is self-contained (no skalibs, no execline). Coverage: ${VAR} and $VAR,
 * backslash escaping, unknown variables left literal, empty value (n=0, word
 * vanishes), multiple refs, multi-word lists (return count), the n>=2 cross
 * product, and appending into a non-empty dst. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <oblibs/subst.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            exit(1) ; \
        } \
    } while(0)

/* A substitution entry expressed as offsets into the vars/values blobs. */
typedef struct ent_s { size_t var ; size_t value ; unsigned int n ; } ent_t ;

/* sizeof-1 keeps the trailing explicit NUL of the literal (the word separator)
 * while dropping the implicit one, giving the exact captured byte length. */
#define EXP(s) (s), (sizeof(s) - 1)

/*
 * Build the context from the blobs, run subst, and assert the result matches
 * the captured golden bytes and word count exactly.
 */
static void check(char const *name,
                  char const *vars, size_t varslen,
                  char const *values, size_t valslen,
                  ent_t const *ents, unsigned int nent,
                  char const *src, size_t srclen,
                  int exp_ret, char const *exp, size_t explen)
{
    printf("Running %-34s ...", name) ;

    subst_t ctx = SUBST_ZERO ;
    if (varslen) TEST_ASSERT(strbuf_catb(&ctx.vars, vars, varslen), "build vars") ;
    if (valslen) TEST_ASSERT(strbuf_catb(&ctx.values, values, valslen), "build values") ;
    for (unsigned int i = 0 ; i < nent ; i++) {
        subst_entry_t e = { .var = ents[i].var, .value = ents[i].value, .n = ents[i].n } ;
        TEST_ASSERT(genbuf_append(subst_entry_t, &ctx.data, &e), "build data") ;
    }

    strbuf dst = STRBUF_ZERO ;
    int r = subst(&dst, src, srclen, &ctx) ;

    TEST_ASSERT(r == exp_ret, "word count") ;
    if (exp_ret >= 0) {
        TEST_ASSERT(dst.len == explen, "output length") ;
        TEST_ASSERT(explen == 0 || !memcmp(dst.s, exp, explen), "output bytes") ;
    }

    subst_free(&ctx) ;
    strbuf_free(&dst) ;
    printf(" PASS\n") ;
}

/* convenience: a single FOO=bar resolution table (n=1) */
#define FOO_VARS "FOO\0"
#define FOO_VALS "bar\0"
static ent_t const foo_ent[] = { { 0, 0, 1 } } ;

int main(void)
{
    printf("=== subst test suite ===\n\n") ;

    /* --- single-word, ${VAR}/$VAR resolution --- */
    check("plain ${FOO}", FOO_VARS, sizeof FOO_VARS - 1, FOO_VALS, sizeof FOO_VALS - 1,
        foo_ent, 1, "${FOO}", strlen("${FOO}"), 1, EXP("bar\0")) ;

    check("bare $FOO", FOO_VARS, sizeof FOO_VARS - 1, FOO_VALS, sizeof FOO_VALS - 1,
        foo_ent, 1, "$FOO", strlen("$FOO"), 1, EXP("bar\0")) ;

    check("text around ${FOO}", FOO_VARS, sizeof FOO_VARS - 1, FOO_VALS, sizeof FOO_VALS - 1,
        foo_ent, 1, "pre${FOO}post", strlen("pre${FOO}post"), 1, EXP("prebarpost\0")) ;

    /* --- escaping (the proven-functional run-script behaviour) --- */
    check("escaped \\${FOO}", FOO_VARS, sizeof FOO_VARS - 1, FOO_VALS, sizeof FOO_VALS - 1,
        foo_ent, 1, "\\${FOO}", strlen("\\${FOO}"), 1, EXP("${FOO}\0")) ;

    check("escaped with text", FOO_VARS, sizeof FOO_VARS - 1, FOO_VALS, sizeof FOO_VALS - 1,
        foo_ent, 1, "pre \\${FOO} post", strlen("pre \\${FOO} post"), 1, EXP("pre ${FOO} post\0")) ;

    check("double backslash \\\\${FOO}", FOO_VARS, sizeof FOO_VARS - 1, FOO_VALS, sizeof FOO_VALS - 1,
        foo_ent, 1, "\\\\${FOO}", strlen("\\\\${FOO}"), 1, EXP("\\bar\0")) ;

    /* --- variable not in the table: left literal --- */
    check("unknown ${BAR}", FOO_VARS, sizeof FOO_VARS - 1, FOO_VALS, sizeof FOO_VALS - 1,
        foo_ent, 1, "${BAR}", strlen("${BAR}"), 1, EXP("${BAR}\0")) ;

    /* --- empty value, n=0: the whole word vanishes --- */
    {
        static char const vars[] = "EMPTY\0" ;
        static char const vals[] = "\0" ;
        static ent_t const ent[] = { { 0, 0, 0 } } ;
        check("empty value n=0 (word vanishes)", vars, sizeof vars - 1, vals, sizeof vals - 1,
            ent, 1, "${EMPTY}", strlen("${EMPTY}"), 0, "", 0) ;
        check("empty value n=0 with text", vars, sizeof vars - 1, vals, sizeof vals - 1,
            ent, 1, "a${EMPTY}b", strlen("a${EMPTY}b"), 0, "", 0) ;
    }

    /* --- several references in one word --- */
    {
        static char const vars[] = "A\0B\0" ;
        static char const vals[] = "1\0""2\0" ;
        static ent_t const ent[] = { { 0, 0, 1 }, { 2, 2, 1 } } ;
        check("two refs ${A}${B}", vars, sizeof vars - 1, vals, sizeof vals - 1,
            ent, 2, "${A}-${B}", strlen("${A}-${B}"), 1, EXP("1-2\0")) ;
    }

    /* --- multi-word list: return count must match --- */
    {
        static char const src[] = "${FOO}\0plain\0${FOO}x" ;
        check("multi-word list", FOO_VARS, sizeof FOO_VARS - 1, FOO_VALS, sizeof FOO_VALS - 1,
            foo_ent, 1, src, sizeof src - 1, 3, EXP("bar\0plain\0barx\0")) ;
    }

    /* --- n>=2: cross product (oblibs never builds this, but the port supports it) --- */
    {
        static char const vars[] = "M\0" ;
        static char const vals[] = "x\0y\0" ;
        static ent_t const ent[] = { { 0, 0, 2 } } ;
        check("n=2 cross product ${M}", vars, sizeof vars - 1, vals, sizeof vals - 1,
            ent, 1, "${M}", strlen("${M}"), 2, EXP("x\0y\0")) ;
        check("n=2 cross product pre${M}post", vars, sizeof vars - 1, vals, sizeof vals - 1,
            ent, 1, "pre${M}post", strlen("pre${M}post"), 2, EXP("prexpost\0preypost\0")) ;
    }

    /* --- append into a non-empty dst (drop-in semantics) --- */
    {
        printf("Running %-34s ...", "append into non-empty dst") ;
        strbuf dst = STRBUF_ZERO ;
        TEST_ASSERT(strbuf_catb(&dst, "seed\0", 5), "seed dst") ;

        subst_t ctx = SUBST_ZERO ;
        TEST_ASSERT(strbuf_catb(&ctx.vars, FOO_VARS, sizeof FOO_VARS - 1), "vars") ;
        TEST_ASSERT(strbuf_catb(&ctx.values, FOO_VALS, sizeof FOO_VALS - 1), "values") ;
        subst_entry_t e = { 0, 0, 1 } ;
        TEST_ASSERT(genbuf_append(subst_entry_t, &ctx.data, &e), "data") ;
        int r = subst(&dst, "${FOO}", strlen("${FOO}"), &ctx) ;

        /* seed\0 preserved, then the substituted word bar\0 appended */
        TEST_ASSERT(r == 1, "return") ;
        TEST_ASSERT(dst.len == 9, "length") ;
        TEST_ASSERT(!memcmp(dst.s, "seed\0bar\0", 9), "bytes") ;

        subst_free(&ctx) ; strbuf_free(&dst) ;
        printf(" PASS\n") ;
    }

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
