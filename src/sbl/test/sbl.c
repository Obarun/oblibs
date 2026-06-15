/*
 * sbl.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * Self-contained test for the sbl module (the strbuf-based replacement that
 * merged stack + sastr). The expected outputs were captured from a byte-to-byte
 * differential run against the legacy sastr_* implementation while both
 * coexisted; they are now frozen here as golden values.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/files.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            if (errno) printf("      errno: %d (%s)\n", errno, strerror(errno)) ; \
            exit(1) ; \
        } \
    } while(0)

#define TEST_START(name) printf("Running %s...", name)
#define TEST_END() printf(" PASS\n")

/* Verify sb's NUL-separated elements equal exp[0..n) exactly (no extra/missing). */
static void check_list(strbuf *sb, char const *const *exp, size_t n)
{
    size_t pos = 0 ;
    for (size_t i = 0 ; i < n ; i++) {
        TEST_ASSERT(pos < sb->len, "fewer elements than expected") ;
        TEST_ASSERT(!strcmp(sb->s + pos, exp[i]), exp[i]) ;
        pos += strlen(sb->s + pos) + 1 ;
    }
    TEST_ASSERT(pos == sb->len, "exact element count") ;
}

/* Verify sb is the C string @s (len excludes the trailing NUL). */
static void check_cstr(strbuf *sb, char const *s)
{
    TEST_ASSERT(sb->len == strlen(s), "cstring length") ;
    TEST_ASSERT(!strcmp(sb->s, s), s) ;
}

static void build(strbuf *sb, char const *const *el, size_t n)
{
    for (size_t i = 0 ; i < n ; i++)
        if (!sbl_add(sb, el[i])) { printf("build failed\n") ; exit(1) ; }
}

static char const *const WORDS[] = { "alpha", "beta", "gamma", "delta" } ;
#define NWORDS 4

static void test_add(void)
{
    TEST_START("test_add") ;
    strbuf sb = STRBUF_ZERO ;
    build(&sb, WORDS, NWORDS) ;
    check_list(&sb, WORDS, NWORDS) ;
    TEST_ASSERT(sb.len == (5+1) + (4+1) + (5+1) + (5+1), "expected total len") ;
    errno = 0 ;
    TEST_ASSERT(!sbl_add(&sb, ""), "empty element rejected") ;
    TEST_ASSERT(errno == EINVAL, "EINVAL on empty") ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

/* sbl_addb stores exactly n bytes (a slice), with no reliance on a NUL in @s. */
static void test_addb(void)
{
    TEST_START("test_addb") ;
    strbuf sb = STRBUF_ZERO ;
    char const *blob = "helloworld" ;          /* no internal delimiter */
    TEST_ASSERT(sbl_addb(&sb, blob, 5), "addb hello slice") ;
    TEST_ASSERT(sbl_addb(&sb, blob + 5, 5), "addb world slice") ;
    TEST_ASSERT(sbl_count(&sb) == 2, "two elements") ;
    TEST_ASSERT(!strcmp(sb.s, "hello"), "first element = hello") ;
    TEST_ASSERT(!strcmp(sb.s + 6, "world"), "second element = world") ;
    TEST_ASSERT(sb.len == (5 + 1) + (5 + 1), "len includes each NUL") ;
    errno = 0 ;
    TEST_ASSERT(!sbl_addb(&sb, blob, 0), "n == 0 rejected") ;
    TEST_ASSERT(errno == EINVAL, "EINVAL on n == 0") ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

static void test_count_search_byid(void)
{
    TEST_START("test_count_search_byid") ;
    strbuf sb = STRBUF_ZERO ;
    build(&sb, WORDS, NWORDS) ;

    TEST_ASSERT(sbl_count(&sb) == NWORDS, "count == 4") ;

    /* present elements: search returns an offset pointing at that element */
    for (size_t i = 0 ; i < NWORDS ; i++) {
        ssize_t o = sbl_search(&sb, WORDS[i]) ;
        TEST_ASSERT(o >= 0 && !strcmp(sb.s + o, WORDS[i]), "search present") ;
    }
    TEST_ASSERT(sbl_search(&sb, "zzz") == -1, "search absent -> -1") ;
    TEST_ASSERT(sbl_search(&sb, "") == -1, "search empty -> -1") ;

    for (unsigned int id = 0 ; id < NWORDS ; id++) {
        ssize_t o = sbl_element_byid(&sb, id) ;
        TEST_ASSERT(o >= 0 && !strcmp(sb.s + o, WORDS[id]), "byid in range") ;
    }
    TEST_ASSERT(sbl_element_byid(&sb, NWORDS) == -1, "byid out of range -> -1") ;

    strbuf_free(&sb) ;
    TEST_END() ;
}

static void test_find(void)
{
    TEST_START("test_find") ;
    strbuf sb = STRBUF_ZERO ;
    build(&sb, WORDS, NWORDS) ;
    /* substring search anywhere in the buffer */
    char const *present[] = { "amma", "alpha", "delta", "a" } ;
    for (size_t i = 0 ; i < 4 ; i++) {
        ssize_t o = sbl_find(&sb, present[i]) ;
        TEST_ASSERT(o >= 0 && !memcmp(sb.s + o, present[i], strlen(present[i])), present[i]) ;
    }
    TEST_ASSERT(sbl_find(&sb, "xyz") == -1, "absent substring -> -1") ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

static void test_remove(void)
{
    TEST_START("test_remove") ;
    char const *const base[] = { "alpha", "beta", "gamma", "delta" } ;
    /* remove first / middle / last / absent -> golden remaining lists */
    char const *exp_alpha[] = { "beta", "gamma", "delta" } ;
    char const *exp_gamma[] = { "alpha", "beta", "delta" } ;
    char const *exp_delta[] = { "alpha", "beta", "gamma" } ;
    struct { char const *target ; char const *const *exp ; size_t n ; } cases[] = {
        { "alpha", exp_alpha, 3 },
        { "gamma", exp_gamma, 3 },
        { "delta", exp_delta, 3 },
        { "zzz",   base,      4 },   /* absent -> unchanged */
    } ;
    for (size_t t = 0 ; t < 4 ; t++) {
        strbuf sb = STRBUF_ZERO ;
        build(&sb, base, 4) ;
        TEST_ASSERT(sbl_remove(&sb, cases[t].target), "remove") ;
        check_list(&sb, cases[t].exp, cases[t].n) ;
        strbuf_free(&sb) ;
    }
    TEST_END() ;
}

static void test_reverse(void)
{
    TEST_START("test_reverse") ;
    strbuf sb = STRBUF_ZERO ;
    build(&sb, WORDS, NWORDS) ;
    TEST_ASSERT(sbl_reverse(&sb), "sbl_reverse") ;
    char const *exp[] = { "delta", "gamma", "beta", "alpha" } ;
    check_list(&sb, exp, 4) ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

static void test_sort(void)
{
    TEST_START("test_sort") ;
    char const *un[] = { "delta", "alpha", "gamma", "beta", "alpha" } ;
    strbuf sb = STRBUF_ZERO ;
    build(&sb, un, 5) ;
    TEST_ASSERT(sbl_sort(&sb), "sbl_sort") ;
    char const *exp[] = { "alpha", "alpha", "beta", "delta", "gamma" } ;
    check_list(&sb, exp, 5) ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

static void test_dropdup_sortndrop(void)
{
    TEST_START("test_dropdup_sortndrop") ;
    char const *exp[] = { "a", "b", "c", "d" } ;
    {
        char const *sorted[] = { "a", "a", "b", "c", "c", "c", "d" } ;
        strbuf sb = STRBUF_ZERO ;
        build(&sb, sorted, 7) ;
        TEST_ASSERT(sbl_drop_duplicate(&sb), "sbl_drop_duplicate") ;
        check_list(&sb, exp, 4) ;
        strbuf_free(&sb) ;
    }
    {
        char const *un[] = { "c", "a", "b", "a", "c", "d", "b" } ;
        strbuf sb = STRBUF_ZERO ;
        build(&sb, un, 7) ;
        TEST_ASSERT(sbl_sortndrop(&sb), "sbl_sortndrop") ;
        check_list(&sb, exp, 4) ;
        strbuf_free(&sb) ;
    }
    TEST_END() ;
}

static void test_dir_drop_child(void)
{
    TEST_START("test_dir_drop_child") ;
    char const *paths[] = { "/etc", "/etc/66", "/etc/66/conf", "/usr", "/usr/bin", "/var" } ;
    strbuf sb = STRBUF_ZERO ;
    build(&sb, paths, 6) ;
    TEST_ASSERT(sbl_dir_drop_child(&sb), "sbl_dir_drop_child") ;
    /* children (/etc/66, /etc/66/conf, /usr/bin) are dropped, parents kept */
    char const *exp[] = { "/etc", "/usr", "/var" } ;
    check_list(&sb, exp, 3) ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

static void test_rebuild(void)
{
    TEST_START("test_rebuild") ;
    {
        strbuf sb = STRBUF_ZERO ;
        build(&sb, WORDS, NWORDS) ;
        TEST_ASSERT(sbl_rebuild_oneline(&sb), "sbl oneline") ;
        check_cstr(&sb, "alpha beta gamma delta") ;
        strbuf_free(&sb) ;
    }
    {
        strbuf sb = STRBUF_ZERO ;
        build(&sb, WORDS, NWORDS) ;
        TEST_ASSERT(sbl_rebuild_nline(&sb), "sbl nline") ;
        check_cstr(&sb, "alpha\nbeta\ngamma\ndelta\n") ;   /* keeps trailing '\n' */
        strbuf_free(&sb) ;
    }
    TEST_END() ;
}

static void test_to_char(void)
{
    TEST_START("test_to_char") ;
    strbuf sb = STRBUF_ZERO ;
    build(&sb, WORDS, NWORDS) ;
    char dst[256] ;
    memset(dst, 0xAA, sizeof dst) ;
    sbl_to_char(dst, &sb) ;
    TEST_ASSERT(!memcmp(dst, sb.s, sb.len), "to_char copies the whole buffer") ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

/* sbl-only: prefix an existing element with raw bytes at its offset. */
static void test_insert(void)
{
    TEST_START("test_insert") ;
    char const *el[] = { "aa", "cc" } ;
    strbuf sb = STRBUF_ZERO ;
    build(&sb, el, 2) ;                 /* "aa\0cc\0" len 6 */
    TEST_ASSERT(sbl_insert(&sb, 0, "X"), "insert front") ;   /* "Xaa\0cc\0" */
    TEST_ASSERT(sb.len == 7, "len grew by 1") ;
    TEST_ASSERT(!strcmp(sb.s, "Xaa"), "first element became Xaa") ;
    TEST_ASSERT(!strcmp(sb.s + 4, "cc"), "second element intact") ;
    errno = 0 ;
    TEST_ASSERT(!sbl_insert(&sb, 0, ""), "empty insert rejected") ;
    TEST_ASSERT(errno == EINVAL, "EINVAL") ;
    /* offset == len has no existing element to fasten to: rejected, list intact
     * (inserting there would leave bytes unterminated past len). */
    errno = 0 ;
    TEST_ASSERT(!sbl_insert(&sb, sb.len, "Y"), "insert at offset==len rejected") ;
    TEST_ASSERT(errno == EINVAL, "EINVAL at len") ;
    TEST_ASSERT(sb.len == 7 && sb.s[sb.len - 1] == 0, "list untouched & canonical") ;
    strbuf_free(&sb) ;
    /* empty list (len 0): offset 0 == len -> rejected, stays empty */
    {
        strbuf empty = STRBUF_ZERO ;
        errno = 0 ;
        TEST_ASSERT(!sbl_insert(&empty, 0, "Z"), "insert into empty rejected") ;
        TEST_ASSERT(errno == EINVAL, "EINVAL on empty list") ;
        TEST_ASSERT(empty.len == 0, "empty list stays empty") ;
        strbuf_free(&empty) ;
    }
    TEST_END() ;
}

/* sbl-only: join with an arbitrary delimiter, no trailing delim. */
static void test_rebuild_with_delim(void)
{
    TEST_START("test_rebuild_with_delim") ;
    char const *el[] = { "a", "b", "c" } ;
    strbuf sb = STRBUF_ZERO ;
    build(&sb, el, 3) ;
    TEST_ASSERT(sbl_rebuild_with_delim(&sb, '/'), "rebuild /") ;
    check_cstr(&sb, "a/b/c") ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

/* directory walkers on a temp tree. */
static void test_dir_get(void)
{
    TEST_START("test_dir_get") ;
    char tmpl[] = "/tmp/sbl_dir_XXXXXX" ;
    char *dir = mkdtemp(tmpl) ;
    TEST_ASSERT(dir != 0, "mkdtemp") ;

    char path[512] ;
    char const *files[] = { "a", "b", "c" } ;
    for (size_t i = 0 ; i < 3 ; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, files[i]) ;
        int fd = creat(path, 0644) ;
        TEST_ASSERT(fd >= 0, "creat") ;
        close(fd) ;
    }
    snprintf(path, sizeof path, "%s/sub", dir) ;
    TEST_ASSERT(mkdir(path, 0755) == 0, "mkdir sub") ;
    char subfile[600] ;
    snprintf(subfile, sizeof subfile, "%s/sub/d", dir) ;
    int fd = creat(subfile, 0644) ;
    TEST_ASSERT(fd >= 0, "creat sub/d") ;
    close(fd) ;

    char const *excl[] = { "b", 0 } ;

    /* dir_get: regular files, excluding "b", sorted -> {a, c} */
    {
        strbuf sb = STRBUF_ZERO ;
        TEST_ASSERT(sbl_dir_get(&sb, dir, excl, S_IFREG), "sbl_dir_get") ;
        TEST_ASSERT(sbl_sort(&sb), "sort") ;
        char const *exp[] = { "a", "c" } ;
        check_list(&sb, exp, 2) ;
        strbuf_free(&sb) ;
    }
    /* dir_get_recursive with include_path, sorted -> {dir/a, dir/c, dir/sub/d} */
    {
        char const *none[] = { 0 } ;
        char ea[600], eb[600], ec[600], ed[700] ;
        snprintf(ea, sizeof ea, "%s/a", dir) ;
        snprintf(eb, sizeof eb, "%s/b", dir) ;
        snprintf(ec, sizeof ec, "%s/c", dir) ;
        snprintf(ed, sizeof ed, "%s/sub/d", dir) ;
        char const *exp[] = { ea, eb, ec, ed } ;   /* no exclusion -> b kept */
        strbuf sb = STRBUF_ZERO ;
        TEST_ASSERT(sbl_dir_get_recursive(&sb, dir, none, S_IFREG, 1), "sbl rec") ;
        TEST_ASSERT(sbl_sort(&sb), "sort") ;
        check_list(&sb, exp, 4) ;
        strbuf_free(&sb) ;
    }
    /* dir_get_element: "a" present once */
    {
        strbuf sb = STRBUF_ZERO ;
        int n = sbl_dir_get_element(&sb, dir, "a", S_IFREG, 0) ;
        TEST_ASSERT(n == 1, "dir_get_element count") ;
        char const *exp[] = { "a" } ;
        check_list(&sb, exp, 1) ;
        strbuf_free(&sb) ;
    }

    /* cleanup */
    file_tryunlink(subfile) ;
    snprintf(path, sizeof path, "%s/sub", dir) ; rmdir(path) ;
    for (size_t i = 0 ; i < 3 ; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, files[i]) ; unlink(path) ;
    }
    rmdir(dir) ;
    TEST_END() ;
}

/* lexer-backed: clean a raw string into a list. */
static void test_clean_string(void)
{
    TEST_START("test_clean_string") ;
    {
        strbuf sb = STRBUF_ZERO ;
        /* '\t' is a skip char (removed) but NOT a separator (only ' ' and '\n'
         * are), so "bar\tbaz" collapses to "barbaz". */
        TEST_ASSERT(sbl_clean_string(&sb, "  foo   bar\tbaz  "), "clean") ;
        char const *exp[] = { "foo", "barbaz" } ;
        check_list(&sb, exp, 2) ;
        strbuf_free(&sb) ;
    }
    {
        strbuf sb = STRBUF_ZERO ;
        TEST_ASSERT(sbl_clean_string(&sb, "single"), "clean single") ;
        char const *exp[] = { "single" } ;
        check_list(&sb, exp, 1) ;
        strbuf_free(&sb) ;
    }
    {
        strbuf sb = STRBUF_ZERO ;
        TEST_ASSERT(sbl_clean_string(&sb, "a b c d e"), "clean spaced") ;
        char const *exp[] = { "a", "b", "c", "d", "e" } ;
        check_list(&sb, exp, 5) ;
        strbuf_free(&sb) ;
    }
    TEST_END() ;
}

/* lexer-backed: split a C string into one element per line. */
static void test_split_nline(void)
{
    TEST_START("test_split_nline") ;
    strbuf sb = STRBUF_ZERO ;
    TEST_ASSERT(strbuf_catb(&sb, "line1\nline2\nline3", strlen("line1\nline2\nline3") + 1), "seed") ;
    TEST_ASSERT(sbl_split_string_in_nline(&sb), "split") ;
    char const *exp[] = { "line1", "line2", "line3" } ;
    check_list(&sb, exp, 3) ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

/* lexer-backed: replace every exact occurrence of @regex with @by. */
static void test_replace(void)
{
    TEST_START("test_replace") ;
    {
        char const *el[] = { "a@b@c", "x@y" } ;
        strbuf sb = STRBUF_ZERO ;
        build(&sb, el, 2) ;
        TEST_ASSERT(sbl_replace(&sb, "@", "__"), "replace @") ;
        char const *exp[] = { "a__b__c", "x__y" } ;
        check_list(&sb, exp, 2) ;
        strbuf_free(&sb) ;
    }
    {
        char const *el[] = { "${FOO}/bin" } ;
        strbuf sb = STRBUF_ZERO ;
        build(&sb, el, 1) ;
        TEST_ASSERT(sbl_replace(&sb, "${FOO}", "/usr"), "replace var") ;
        char const *exp[] = { "/usr/bin" } ;
        check_list(&sb, exp, 1) ;
        strbuf_free(&sb) ;
    }
    {
        char const *el[] = { "aaa" } ;
        strbuf sb = STRBUF_ZERO ;
        build(&sb, el, 1) ;
        TEST_ASSERT(sbl_replace(&sb, "a", "bb"), "replace a->bb") ;
        char const *exp[] = { "bbbbbb" } ;
        check_list(&sb, exp, 1) ;
        strbuf_free(&sb) ;
    }
    TEST_END() ;
}

/* lexer-backed: split into lines, replace on each, rejoin with '\n'. */
static void test_replace_nline(void)
{
    TEST_START("test_replace_nline") ;
    strbuf sb = STRBUF_ZERO ;
    TEST_ASSERT(strbuf_catb(&sb, "a@b\nc@d@e\nf", strlen("a@b\nc@d@e\nf") + 1), "seed") ;
    TEST_ASSERT(sbl_replace_nline(&sb, "@", "::"), "replace_nline") ;
    check_cstr(&sb, "a::b\nc::d::e\nf\n") ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

int main(void)
{
    printf("=== sbl test suite ===\n\n") ;

    test_add() ;
    test_addb() ;
    test_count_search_byid() ;
    test_find() ;
    test_remove() ;
    test_reverse() ;
    test_sort() ;
    test_dropdup_sortndrop() ;
    test_dir_drop_child() ;
    test_rebuild() ;
    test_to_char() ;
    test_insert() ;
    test_rebuild_with_delim() ;
    test_dir_get() ;
    test_clean_string() ;
    test_split_nline() ;
    test_replace() ;
    test_replace_nline() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
