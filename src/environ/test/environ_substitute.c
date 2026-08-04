/*
 * environ_substitute.c
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

/* Behavioural test for environ_substitute (sbl-native engine).
 *
 * environ_substitute expands ${VAR} references in every value of a decoded sbl,
 * resolving each VAR first from the process environment (getenv) then from the
 * other entries of the SAME sbl, via the internal helper environ_sbl_value
 * (which copies the stored value VERBATIM -- no second environ_trim pass). It
 * rewrites the sbl in place.
 *
 * COMPARISON STRATEGY (approach a -- sbl-to-sbl).
 * The deleted helper environ_join_nl used to flatten the result to \n-text for
 * strcmp; it is gone. Instead we compare two SBLs byte-for-byte:
 *   - the actual result: environ_merge_string(a) then environ_substitute;
 *   - the expected: environ_merge_string(b) where b is the post-substitution
 *     envfile text. Both go through the same decoder (environ_trim), so any '\'
 *     or '"' in an expected value is written in envfile syntax in b and decoded
 *     identically -- the comparison stays on decoded bytes, no ambiguity.
 * A value still bearing its VAR_UNEXPORT '!' survives substitution and trim
 * identically on both sides, so it compares equal too. */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/genbuf.h>
#include <oblibs/subst.h>
#include <oblibs/environ.h>

#include "../environ-internal.h"

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            if (errno) printf("      errno: %d (%s)\n", errno, strerror(errno)) ; \
            exit(1) ; \
        } \
    } while(0)

/* Run substitution on @a and assert the resulting sbl equals the sbl decoded
 * from @b. Both sides are decoded by environ_trim (via environ_merge_string),
 * so the comparison is on decoded bytes; @b is post-substitution envfile text. */
void compare(strbuf *sa, const char *a, const char *b)
{
    subst_t info = SUBST_ZERO ;
    _cleanup_strbuf_ strbuf expected = STRBUF_ZERO ;

    sa->len = 0 ;
    assert(environ_merge_string(sa, a) == 1) ;
    assert(environ_substitute(sa, &info) == 1) ;

    assert(environ_merge_string(&expected, b) == 1) ;

    /* sbl byte-for-byte: same length AND same bytes (NULs included) */
    assert(sa->len == expected.len) ;
    assert(memcmp(sa->s, expected.s, sa->len) == 0) ;

    subst_free(&info) ;
}

void basic()
{
    printf("Running test basic...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Args=!-n ${ArgsConfFile}\n\
ArgsConfFile=!-f /etc/bluetooth/main.conf" ;

    const char *b = "Args=!-n -f /etc/bluetooth/main.conf\n\
ArgsConfFile=!-f /etc/bluetooth/main.conf" ;

    compare(&sa, a, b) ;
}

void basic1()
{
    printf("Running test basic1...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Args=!%%tmpfiles%%/opentmpfiles.sh ${ArgsScript}\n\
ArgsScript=!--create --verbose --boot" ;

    const char *b = "Args=!%%tmpfiles%%/opentmpfiles.sh --create --verbose --boot\n\
ArgsScript=!--create --verbose --boot" ;

    compare(&sa, a, b) ;
}

void basic2()
{
    printf("Running test basic2...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Args=!--session --print-pid=4 --nofork --nopidfile --address=unix:path=${Socket}\n\
Socket=!@R/bus" ;

    const char *b = "Args=!--session --print-pid=4 --nofork --nopidfile --address=unix:path=@R/bus\n\
Socket=!@R/bus" ;

    compare(&sa, a, b) ;
}

void basic3()
{
    printf("Running test basic3...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Args=!${ArgsConfFile} -aN\n\
ArgsConfFile=!-c /etc/zfs/zpool.cache" ;

    const char *b = "Args=!-c /etc/zfs/zpool.cache -aN\n\
ArgsConfFile=!-c /etc/zfs/zpool.cache" ;

    compare(&sa, a, b) ;
}

void complex()
{
    printf("Running test complex...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Args=!--nodaemonize ${ArgsConfFile} ${ArgsPidFile}\n\
ArgsConfFile=!--fpm-config /etc/php/php-fpm.conf\n\
LiveDirectory=!/run/php-fpm\n\
ArgsPidFile=!--pid ${LiveDirectory}/php-fpm.pid" ;

    const char *b = "Args=!--nodaemonize --fpm-config /etc/php/php-fpm.conf --pid /run/php-fpm/php-fpm.pid\n\
ArgsConfFile=!--fpm-config /etc/php/php-fpm.conf\n\
LiveDirectory=!/run/php-fpm\n\
ArgsPidFile=!--pid /run/php-fpm/php-fpm.pid" ;

    compare(&sa, a, b) ;
}

void complex1()
{
    printf("Running test complex1...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "ArgsStart=!-d -v ${ArgsConfFile} ${ArgsPidFile}\n\
ArgsStop=!-r\n\
ArgsConfFile=!-cf ${ConfDirectory}/${ConfFile}\n\
ArgsPidFile=!-pf ${PidFile}\n\
ConfDirectory=!/etc/dhcp\n\
ConfFile=dhclient-@I.conf\n\
LiveDirectory=!/run/dhclient-@I\n\
PidFile=${LiveDirectory}/dhclient.pid" ;

    const char *b = "ArgsStart=!-d -v -cf /etc/dhcp/dhclient-@I.conf -pf /run/dhclient-@I/dhclient.pid\n\
ArgsStop=!-r\n\
ArgsConfFile=!-cf /etc/dhcp/dhclient-@I.conf\n\
ArgsPidFile=!-pf /run/dhclient-@I/dhclient.pid\n\
ConfDirectory=!/etc/dhcp\n\
ConfFile=dhclient-@I.conf\n\
LiveDirectory=!/run/dhclient-@I\n\
PidFile=/run/dhclient-@I/dhclient.pid" ;

    compare(&sa, a, b) ;
}

void nope()
{
    printf("Running test nope...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "ConfFile=!%%initconf%%\n\
Timeout=!60000\n\
Verbosity=3" ;

    const char *b = "ConfFile=!%%initconf%%\n\
Timeout=!60000\n\
Verbosity=3" ;

    compare(&sa, a, b) ;
}

void comment()
{
    printf("Running test comment...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "ConfFile=!%%initconf%%\n\
## first comment\n\
## Second comment\n\
Timeout=!60000\n\
Verbosity=3" ;

    const char *b = "ConfFile=!%%initconf%%\n\
Timeout=!60000\n\
Verbosity=3" ;

    compare(&sa, a, b) ;
}

void unknown()
{
    printf("Running test unknown...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    /* ${unknown} resolves to nothing (no env var, no entry) -> left literal */
    const char *a = "Socket=/run/libvirt/libvirt-sock ${unknown}" ;

    const char *b = "Socket=/run/libvirt/libvirt-sock ${unknown}" ;

    compare(&sa, a, b) ;
}

void empty_line()
{
    printf("Running test empty_line...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Socket=/run/libvirt/libvirt-sock ${empty}\n\
\n\
\n\
empty=t\n\n" ;

    const char *b = "Socket=/run/libvirt/libvirt-sock t\n\
empty=t" ;

    compare(&sa, a, b) ;
}

void cyclic_reference()
{
    printf("Running test cyclic_reference...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf expected = STRBUF_ZERO ;
    subst_t info = SUBST_ZERO ;
    assert(environ_merge_string(&sa, "B=${A}\nA=${B}") == 1) ;
    assert(environ_substitute(&sa, &info) == 0) ;        /* terminates, not a hang */
    /* left literal: result sbl equals the decoded input sbl */
    assert(environ_merge_string(&expected, "B=${A}\nA=${B}") == 1) ;
    assert(sa.len == expected.len) ;
    assert(memcmp(sa.s, expected.s, sa.len) == 0) ;
    subst_free(&info) ;

    /* a longer cycle terminates too */
    sa.len = 0 ;
    subst_t info2 = SUBST_ZERO ;
    assert(environ_merge_string(&sa, "A=${B}\nB=${C}\nC=${A}") == 1) ;
    assert(environ_substitute(&sa, &info2) == 0) ;
    subst_free(&info2) ;
}

void self_reference()
{
    printf("Running test self_reference...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    compare(&sa, "A=${A}", "A=${A}") ;
    /* same guard, with surrounding literal text kept intact */
    sa.len = 0 ;
    compare(&sa, "A=x${A}y", "A=x${A}y") ;
}

void escape_is_dead()
{
    printf("Running test escape_is_dead...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    const char *a = "Key=\"\\${ArgsConfFile}\"\n\
ArgsConfFile=!-f /etc/bluetooth/main.conf" ;
    /* the backslash is gone after trim, so Key gets the substituted value */
    const char *b = "Key=-f /etc/bluetooth/main.conf\n\
ArgsConfFile=!-f /etc/bluetooth/main.conf" ;
    compare(&sa, a, b) ;
}

void backslash_with_ref()
{
    printf("Running test backslash_with_ref...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    /* envfile: A="path\\to ${B}"  /  B=ok   (C doubles each '\') */
    const char *a = "A=\"path\\\\to ${B}\"\nB=ok" ;
    /* envfile: A="path\\to ok"  /  B=ok   -> decoded A = path\to ok */
    const char *b = "A=\"path\\\\to ok\"\nB=ok" ;
    compare(&sa, a, b) ;
}

void quote_with_ref()
{
    printf("Running test quote_with_ref...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    const char *a = "A=\"say \\\"hi\\\" ${B}\"\nB=ok" ;
    const char *b = "A=\"say \\\"hi\\\" ok\"\nB=ok" ;
    compare(&sa, a, b) ;
}

void hash_and_equal_with_ref()
{
    printf("Running test hash_and_equal_with_ref...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    /* '#' inside a quoted value is literal; '=' is always literal in a value */
    const char *a = "A=\"k=v #c ${B}\"\nB=ok" ;
    const char *b = "A=\"k=v #c ok\"\nB=ok" ;
    compare(&sa, a, b) ;
}

void prefix_collision()
{
    printf("Running test prefix_collision...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    const char *a = "FOO=good\nFOOBAR=bad\nX=${FOO}" ;
    const char *b = "FOO=good\nFOOBAR=bad\nX=good" ;
    compare(&sa, a, b) ;
}

void key_whitespace()
{
    printf("Running test key_whitespace...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    const char *a = "  Key  =val\nRef=${Key}" ;
    const char *b = "Key=val\nRef=val" ;
    compare(&sa, a, b) ;
}

void getenv_precedence()
{
    printf("Running test getenv_precedence...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf expected = STRBUF_ZERO ;
    subst_t info = SUBST_ZERO ;

    assert(setenv("OB_TEST_PRECEDENCE", "fromenv", 1) == 0) ;

    const char *a = "OB_TEST_PRECEDENCE=fromentry\nUse=${OB_TEST_PRECEDENCE}" ;
    assert(environ_merge_string(&sa, a) == 1) ;
    assert(environ_substitute(&sa, &info) == 1) ;

    /* Use must take the env value, not the entry value */
    const char *b = "OB_TEST_PRECEDENCE=fromentry\nUse=fromenv" ;
    assert(environ_merge_string(&expected, b) == 1) ;
    assert(sa.len == expected.len) ;
    assert(memcmp(sa.s, expected.s, sa.len) == 0) ;

    subst_free(&info) ;
    assert(unsetenv("OB_TEST_PRECEDENCE") == 0) ;
}

static void feed(subst_t *info, const char *txt)
{
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    TEST_ASSERT(environ_merge_string(&sa, txt) == 1, "merge_string should succeed") ;
    TEST_ASSERT(environ_substitute(&sa, info) == 1, "environ_substitute should succeed") ;
}

static void expect_entry(subst_t *info, size_t i, size_t var, const char *name, size_t value, const char *val, unsigned int n)
{
    subst_entry_t *d = genbuf_s(subst_entry_t, &info->data) ;

    TEST_ASSERT(i < genbuf_len(subst_entry_t, &info->data), "entry index out of range") ;
    TEST_ASSERT(d[i].var == var, "entry var offset mismatch") ;
    TEST_ASSERT(d[i].value == value, "entry value offset mismatch") ;
    TEST_ASSERT(d[i].n == n, "entry n mismatch") ;
    /* the offset must land on the intended bytes, inside the blob */
    TEST_ASSERT(var + strlen(name) < info->vars.len, "var offset outside vars") ;
    TEST_ASSERT(!strcmp(info->vars.s + var, name), "var offset points at another name") ;
    if (n) {
        TEST_ASSERT(value + strlen(val) < info->values.len, "value offset outside values") ;
        TEST_ASSERT(!strcmp(info->values.s + value, val), "value offset points at another value") ;
    }
}

static void expect_blob(strbuf *sb, const char *bytes, size_t len, const char *what)
{
    TEST_ASSERT(sb->len == len, what) ;
    /* a zero-length blob may still have a NULL .s: never hand it to memcmp */
    TEST_ASSERT(!len || !memcmp(sb->s, bytes, len), what) ;
}

void duplicate_in_one_source()
{
    printf("Running test duplicate_in_one_source...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    subst_t info = SUBST_ZERO ;

    TEST_ASSERT(environ_merge_string(&sa, "FOO=1\nFOO=2") == 1, "merge_string should succeed") ;
    /* the decoder keeps both entries: the duplicate really reaches the engine */
    expect_blob(&sa, "FOO=1\0FOO=2\0", 12, "source sbl must hold both entries") ;

    TEST_ASSERT(environ_substitute(&sa, &info) == 1, "environ_substitute should succeed") ;

    expect_blob(&info.vars, "FOO\0\0", 5, "the name must be stored exactly once") ;
    TEST_ASSERT(genbuf_len(subst_entry_t, &info.data) == 1, "exactly one entry per name") ;
    /* values holds "1\0" twice: the first resolution, then the rewritten one */
    expect_blob(&info.values, "1\0" "1\0", 4, "values blob") ;
    expect_entry(&info, 0, 0, "FOO", 2, "1", 1) ;

    subst_free(&info) ;
}

void duplicate_in_one_source_by_effect()
{
    printf("Running test duplicate_in_one_source_by_effect...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf out = STRBUF_ZERO ;
    subst_t info = SUBST_ZERO ;

    TEST_ASSERT(environ_merge_string(&sa, "FOO=1\nFOO=2") == 1, "merge_string should succeed") ;
    TEST_ASSERT(environ_substitute(&sa, &info) == 1, "environ_substitute should succeed") ;

    TEST_ASSERT(subst(&out, "x=${FOO}\0", 9, &info) == 1, "exactly one word produced") ;
    expect_blob(&out, "x=1\0", 4, "substituted word") ;

    subst_free(&info) ;
}

void two_calls_overlapping()
{
    printf("Running test two_calls_overlapping...\n") ;

    subst_t info = SUBST_ZERO ;

    feed(&info, "A=1\nB=2") ;
    expect_blob(&info.vars, "A\0\0B\0\0", 6, "names after the first call") ;
    TEST_ASSERT(genbuf_len(subst_entry_t, &info.data) == 2, "two entries after the first call") ;

    feed(&info, "B=9") ;

    /* the names blob is untouched: B reused its offset, A is still intact */
    expect_blob(&info.vars, "A\0\0B\0\0", 6, "names must be unchanged by the second call") ;
    TEST_ASSERT(genbuf_len(subst_entry_t, &info.data) == 2, "no entry added for a known name") ;
    /* the superseded "2" stays in the blob, unreferenced */
    expect_blob(&info.values, "1\0" "2\0" "9\0", 6, "values blob") ;
    expect_entry(&info, 0, 0, "A", 0, "1", 1) ;   /* first call, untouched */
    expect_entry(&info, 1, 3, "B", 4, "9", 1) ;   /* rewritten in place */

    subst_free(&info) ;
}

void two_calls_disjoint()
{
    printf("Running test two_calls_disjoint...\n") ;

    subst_t info = SUBST_ZERO ;

    feed(&info, "A=1") ;
    feed(&info, "B=2") ;

    expect_blob(&info.vars, "A\0\0B\0\0", 6, "both names appended") ;
    expect_blob(&info.values, "1\0" "2\0", 4, "both values appended") ;
    TEST_ASSERT(genbuf_len(subst_entry_t, &info.data) == 2, "two entries") ;
    expect_entry(&info, 0, 0, "A", 0, "1", 1) ;
    expect_entry(&info, 1, 3, "B", 2, "2", 1) ;

    subst_free(&info) ;
}

void unexport_both_ways()
{
    printf("Running test unexport_both_ways...\n") ;

    {   /* unexport then export: the key must be gone */
        subst_t info = SUBST_ZERO ;
        feed(&info, "FOO=!1") ;
        expect_blob(&info.modifs, "FOO\0\0", 5, "the marker registers the key") ;
        feed(&info, "FOO=2") ;
        expect_blob(&info.modifs, "\0", 1, "no marker must remove the key") ;
        TEST_ASSERT(sbl_search(&info.modifs, "FOO") == -1, "FOO must not be a record any more") ;
        subst_free(&info) ;
    }

    {   /* export then unexport: the key must appear */
        subst_t info = SUBST_ZERO ;
        feed(&info, "FOO=1") ;
        expect_blob(&info.modifs, "", 0, "no marker, nothing registered") ;
        feed(&info, "FOO=!2") ;
        expect_blob(&info.modifs, "FOO\0\0", 5, "the marker registers the key") ;
        TEST_ASSERT(sbl_search(&info.modifs, "FOO") == 0, "FOO recorded at offset 0") ;
        subst_free(&info) ;
    }

    {   /* unexport twice: a single occurrence */
        subst_t info = SUBST_ZERO ;
        feed(&info, "FOO=!1") ;
        feed(&info, "FOO=!2") ;
        expect_blob(&info.modifs, "FOO\0\0", 5, "the key must not be recorded twice") ;
        subst_free(&info) ;
    }
}

void empty_value_on_second_call()
{
    printf("Running test empty_value_on_second_call...\n") ;

    _cleanup_strbuf_ strbuf out = STRBUF_ZERO ;
    subst_t info = SUBST_ZERO ;

    feed(&info, "FOO=1") ;
    expect_entry(&info, 0, 0, "FOO", 0, "1", 1) ;

    feed(&info, "FOO=") ;

    TEST_ASSERT(genbuf_len(subst_entry_t, &info.data) == 1, "still one entry") ;
    expect_blob(&info.values, "1\0", 2, "an empty value must not grow the blob") ;
    /* n == 0, so value is never dereferenced; it holds the blob end */
    expect_entry(&info, 0, 0, "FOO", 2, 0, 0) ;

    /* by effect: the word carrying ${FOO} expands to nothing, "y" survives */
    TEST_ASSERT(subst(&out, "x=${FOO}\0y\0", 11, &info) == 1, "one word left") ;
    expect_blob(&out, "y\0", 2, "the empty variable must swallow its word") ;

    subst_free(&info) ;
}

void single_call_unchanged()
{
    printf("Running test single_call_unchanged...\n") ;

    subst_t info = SUBST_ZERO ;

    feed(&info, "A=1\nB=${A}x") ;

    expect_blob(&info.vars, "A\0\0B\0\0", 6, "names blob") ;
    expect_blob(&info.values, "1\0" "1x\0", 5, "values blob") ;
    expect_blob(&info.modifs, "", 0, "no unexport") ;
    TEST_ASSERT(genbuf_len(subst_entry_t, &info.data) == 2, "two entries") ;
    expect_entry(&info, 0, 0, "A", 0, "1", 1) ;
    expect_entry(&info, 1, 3, "B", 2, "1x", 1) ;

    subst_free(&info) ;
}

int main() {

    basic() ;
    basic1() ;
    basic2() ;
    basic3() ;
    complex() ;
    complex1() ;
    nope() ;
    comment() ;
    unknown() ;
    empty_line() ;
    cyclic_reference() ;
    self_reference() ;
    escape_is_dead() ;
    backslash_with_ref() ;
    quote_with_ref() ;
    hash_and_equal_with_ref() ;
    prefix_collision() ;
    key_whitespace() ;
    getenv_precedence() ;
    duplicate_in_one_source() ;
    duplicate_in_one_source_by_effect() ;
    two_calls_overlapping() ;
    two_calls_disjoint() ;
    unexport_both_ways() ;
    empty_value_on_second_call() ;
    single_call_unchanged() ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}
