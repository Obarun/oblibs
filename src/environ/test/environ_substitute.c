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
#include <oblibs/environ.h>

#include "../environ-internal.h"

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

/* Indirect cycle: dollar_replace tracks the keys expanded in one chain, so a
 * two-step (or longer) cycle is caught instead of looping forever -- value used
 * to oscillate between ${A} and ${B}. The reaching of this assertion at all is
 * the regression guard: a re-introduced loop would hang the suite. The cyclic
 * variables are left literal and an indirect cycle is reported as failure (0),
 * unlike a direct self-reference which succeeds (1) and is left as-is. */
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

/* Direct self-reference A=${A}: the anti-loop guard in dollar_replace
 * ("if (!strcmp(mkey, regex.s)) ... continue") catches a variable that refers to
 * itself and leaves the ${A} literal instead of recursing forever. Without the
 * guard this would loop; with it, A=${A} comes out verbatim. */
void self_reference()
{
    printf("Running test self_reference...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    compare(&sa, "A=${A}", "A=${A}") ;
    /* same guard, with surrounding literal text kept intact */
    sa.len = 0 ;
    compare(&sa, "A=x${A}y", "A=x${A}y") ;
}

/* Escaping is DEAD at the config-value pipeline (environ_merge -> environ_trim ->
 * environ_substitute): environ_trim consumes the backslash inside the quotes
 * before substitution ever runs, so "\${VAR}" is indistinguishable from ${VAR}
 * and the variable IS substituted. This pins the documented behavior -- a
 * backslash does NOT protect a variable here (escaping only survives in the
 * run-script pipeline, a different code path). */
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

/* Refactor fix #1: a value carrying a LITERAL backslash plus a ${ref}.
 * Raw envfile: A="path\\to ${B}"  (the C source doubles every backslash again).
 * environ_trim decodes "\\" to one '\', leaving the decoded value  path\to ${B}.
 * environ_substitute then resolves ${B} via environ_sbl_value, which returns B's
 * value VERBATIM, giving  path\to ok. The old engine re-ran environ_trim on the
 * stored value during lookup and the literal '\' did not survive -- this case
 * failed before the refactor and must pass now. Expected envfile re-encodes the
 * single '\' as "\\" inside quotes so both sides decode to the same byte. */
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

/* Refactor fix #2: a value with a literal double-quote (via \") plus a ${ref}.
 * Raw envfile: A="say \"hi\" ${B}"  -> decoded A = say "hi" ${B}, then ${B}->ok.
 * The verbatim lookup must keep the '"' bytes; a re-trim on lookup would choke. */
void quote_with_ref()
{
    printf("Running test quote_with_ref...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    const char *a = "A=\"say \\\"hi\\\" ${B}\"\nB=ok" ;
    const char *b = "A=\"say \\\"hi\\\" ok\"\nB=ok" ;
    compare(&sa, a, b) ;
}

/* Refactor fix #3: '#' and '=' literal inside a value, plus a ${ref}. Neither
 * is escaped by the encoder and both are kept literal by the parser; the value
 * lookup must return them verbatim and still substitute ${B}. */
void hash_and_equal_with_ref()
{
    printf("Running test hash_and_equal_with_ref...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    /* '#' inside a quoted value is literal; '=' is always literal in a value */
    const char *a = "A=\"k=v #c ${B}\"\nB=ok" ;
    const char *b = "A=\"k=v #c ok\"\nB=ok" ;
    compare(&sa, a, b) ;
}

/* Prefix collision: X=${FOO} must resolve to FOO's value, never FOOBAR's. This
 * exercises the exact-length match in environ_sbl_value at the substitution
 * level: a prefix match would wrongly pick FOOBAR. */
void prefix_collision()
{
    printf("Running test prefix_collision...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    const char *a = "FOO=good\nFOOBAR=bad\nX=${FOO}" ;
    const char *b = "FOO=good\nFOOBAR=bad\nX=good" ;
    compare(&sa, a, b) ;
}

/* Key with surrounding whitespace: environ_get_key trims the key when building
 * the sbl, so "  Key  =val" stores key "Key"; a ${Key} reference must still
 * resolve. (The decoded sbl already holds the trimmed key, so this also pins
 * that environ_trim trims the key.) */
void key_whitespace()
{
    printf("Running test key_whitespace...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    const char *a = "  Key  =val\nRef=${Key}" ;
    const char *b = "Key=val\nRef=val" ;
    compare(&sa, a, b) ;
}

/* getenv precedence: a ${VAR} set in the process environment wins over an entry
 * of the same name in the sbl. dollar_replace calls getenv first. */
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

    printf("All tests passed successfully.\n") ;

    return 0 ;
}
