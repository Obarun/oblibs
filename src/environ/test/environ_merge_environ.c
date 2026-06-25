/*
 * environ_merge_environ.c
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

/* Behavioural test for environ_merge_environ (sbl-native merge engine).
 *
 * environ_merge_environ(modifs, new) overlays @new onto @modifs in place:
 *   - if @modifs is empty it returns 1 and touches nothing (early return);
 *   - otherwise the result @modifs becomes [every entry of @new] followed by
 *     [the entries of the old @modifs whose key is ABSENT from @new]. @new wins
 *     on a shared key (the matching @modifs entry is dropped); disjoint @modifs
 *     entries are appended after the @new block. @new is read-only.
 * The key membership test is the internal helper environ_sbl_value, which matches
 * the key EXACTLY (length-checked) against the bytes before the first '=', so a
 * prefix collision (FOO vs FOOBAR) does not match and the stored value bytes
 * (backslash, etc.) never affect the key comparison.
 *
 * COMPARISON STRATEGY (sbl-to-sbl, mirroring environ_substitute.c).
 * Both the actual result and the expected result are built as SBLs via
 * environ_merge_string of envfile text, then compared byte-for-byte (same length
 * AND same bytes, NULs included). Both sides go through the same decoder
 * (environ_trim), so any '\' in an expected value is written in envfile syntax
 * (quoted + doubled) and decodes identically -- the comparison stays on decoded
 * bytes with no ambiguity. (An UNQUOTED single backslash is a syntax error in
 * envfile, so a literal backslash in a value is always written "x\\y".) */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <oblibs/strbuf.h>
#include <oblibs/environ.h>

#include "../environ-internal.h"

/* Build @m and @n into SBLs, merge @n onto @m, and assert the resulting SBL of
 * @m equals the SBL decoded from @expect. All three go through environ_trim, so
 * the comparison is on decoded bytes. Returns nothing; aborts on mismatch. */
static void compare(const char *m, const char *n, const char *expect)
{
    _cleanup_strbuf_ strbuf modifs = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf new = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf expected = STRBUF_ZERO ;

    assert(environ_merge_string(&modifs, m) == 1) ;
    assert(environ_merge_string(&new, n) == 1) ;
    assert(environ_merge_string(&expected, expect) == 1) ;

    assert(environ_merge_environ(&modifs, &new) == 1) ;

    /* sbl byte-for-byte: same length AND same bytes (NULs included) */
    assert(modifs.len == expected.len) ;
    assert(memcmp(modifs.s, expected.s, modifs.len) == 0) ;
}

/* Shared key: @new's B replaces @modifs's B (dropped), and the @new block comes
 * first; the disjoint @modifs entry A is appended after. */
void overlap()
{
    printf("Running test overlap...\n") ;
    /* modifs A=1,B=2 ; new B=9,C=3 -> B=9,C=3,A=1 */
    compare("A=1\nB=2", "B=9\nC=3", "B=9\nC=3\nA=1") ;
}

/* No shared key: the whole @new block precedes the whole (unchanged) @modifs. */
void disjoint()
{
    printf("Running test disjoint...\n") ;
    /* modifs A=1,B=2 ; new C=3,D=4 -> C=3,D=4,A=1,B=2 */
    compare("A=1\nB=2", "C=3\nD=4", "C=3\nD=4\nA=1\nB=2") ;
}

/* The only @modifs entry shares its key with @new: @new wins, @modifs entry is
 * dropped, leaving exactly the @new entry. */
void full_precedence()
{
    printf("Running test full_precedence...\n") ;
    /* modifs A=1 ; new A=2 -> A=2 only */
    compare("A=1", "A=2", "A=2") ;
}

/* Early-return contract: an empty @modifs returns 1 and is left empty -- @new is
 * NOT seeded into it, and @new itself is untouched. A mutation that seeds @new or
 * drops the early return changes modifs.len away from 0 and trips this. */
void empty_modifs()
{
    printf("Running test empty_modifs...\n") ;
    _cleanup_strbuf_ strbuf modifs = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf new = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf newref = STRBUF_ZERO ;

    assert(environ_merge_string(&new, "A=1") == 1) ;
    assert(environ_merge_string(&newref, "A=1") == 1) ;

    assert(environ_merge_environ(&modifs, &new) == 1) ;

    /* modifs stays empty (not seeded) */
    assert(modifs.len == 0) ;
    /* new is unchanged */
    assert(new.len == newref.len) ;
    assert(memcmp(new.s, newref.s, new.len) == 0) ;
}

/* Prefix collision: @new key FOO must drop ONLY @modifs's FOO, never FOOBAR.
 * This pins the exact-length key match in environ_sbl_value: a prefix match would
 * wrongly drop FOOBAR. Result keeps FOOBAR with its value intact. */
void prefix_collision()
{
    printf("Running test prefix_collision...\n") ;
    /* modifs FOO=1,FOOBAR=2 ; new FOO=9 -> FOO=9,FOOBAR=2 (FOOBAR kept) */
    compare("FOO=1\nFOOBAR=2", "FOO=9", "FOO=9\nFOOBAR=2") ;
}

/* Reverse prefix collision: the @modifs key FOO is a PREFIX of @new's FOOBAR.
 * The lookup of "FOO" in @new must NOT match the longer "FOOBAR=9" entry -- the
 * keys are distinct, so FOO is disjoint and KEPT. This is the case that bites the
 * exact-length guard in environ_sbl_value: without `r == klen`, strncmp(e, "FOO",
 * 3) over "FOOBAR=9" returns 0 and FOO would be wrongly dropped. Result: the @new
 * block (FOOBAR=9) first, then the kept FOO=1. */
void prefix_collision_reverse()
{
    printf("Running test prefix_collision_reverse...\n") ;
    /* modifs FOO=1 ; new FOOBAR=9 -> FOOBAR=9,FOO=1 (FOO kept, distinct key) */
    compare("FOO=1", "FOOBAR=9", "FOOBAR=9\nFOO=1") ;
}

/* Key match is by KEY only, independent of the value bytes. PATH carries a
 * literal backslash in @modifs (quoted+doubled "a\\b" decodes to a\b) yet its key
 * still matches @new's PATH and is dropped; KEEP (also backslash-valued, disjoint)
 * survives with its single literal '\' byte-preserved. Both sides decode the same
 * envfile syntax so the kept backslash compares equal. */
void backslash_value_key_match()
{
    printf("Running test backslash_value_key_match...\n") ;
    /* decoded modifs: PATH=a\b , KEEP=x\y ; new PATH=c -> PATH=c , KEEP=x\y */
    compare("PATH=\"a\\\\b\"\nKEEP=\"x\\\\y\"", "PATH=c",
            "PATH=c\nKEEP=\"x\\\\y\"") ;
}

/* A backslash in a @new value survives verbatim into the result: @new's B holds a
 * literal '\' (decoded from "c\\d") and lands first in the result, ahead of the
 * disjoint @modifs A. Pins that @new entries are copied byte-for-byte (no
 * re-decode of the stored value during the merge). */
void new_backslash_survives()
{
    printf("Running test new_backslash_survives...\n") ;
    /* modifs A=1 ; new B=c\d -> B=c\d , A=1 */
    compare("A=1", "B=\"c\\\\d\"", "B=\"c\\\\d\"\nA=1") ;
}

int main()
{
    overlap() ;
    disjoint() ;
    full_precedence() ;
    empty_modifs() ;
    prefix_collision() ;
    prefix_collision_reverse() ;
    backslash_value_key_match() ;
    new_backslash_survives() ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}
