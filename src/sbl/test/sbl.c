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
 * except according to the terms contained in the LICENSE file.
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
#include <sys/socket.h>
#include <sys/un.h>

#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/files.h>
#include <oblibs/types.h>

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

/* sbl_sort short-circuits on the empty list with return 0 (the "if (!sb->len)
 * return 0" guard), leaving the buffer empty. This is its ONLY 0 return that is
 * not an allocation failure. Dropping the guard makes an empty sort return 1. */
static void test_sort_empty(void)
{
    TEST_START("test_sort_empty") ;
    strbuf sb = STRBUF_ZERO ;
    TEST_ASSERT(sbl_sort(&sb) == 0, "sort of an empty list returns 0") ;
    TEST_ASSERT(sb.len == 0, "empty list stays empty") ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

/* The sort is non-destructive on duplicates: identical elements are NEVER merged
 * (that is sbl_drop_duplicate's job), only reordered. A run of N identical keys
 * must survive sort as N adjacent copies. sbl_sort uses strcmp > 0 (strict), so
 * equal elements keep their relative order -- but since equal elements are
 * byte-identical here, that order is not externally observable; what IS pinned is
 * that the count and adjacency of duplicates are preserved across the sort.
 * sbl_sortndrop then proves the equal run collapses to exactly one. */
static void test_sort_duplicates(void)
{
    TEST_START("test_sort_duplicates") ;
    {
        /* three "b" and two "a" scattered -> sort keeps all five, grouped */
        char const *un[] = { "b", "a", "b", "c", "a", "b" } ;
        strbuf sb = STRBUF_ZERO ;
        build(&sb, un, 6) ;
        TEST_ASSERT(sbl_sort(&sb), "sbl_sort with duplicates") ;
        char const *exp[] = { "a", "a", "b", "b", "b", "c" } ;
        check_list(&sb, exp, 6) ;   /* all duplicates kept, none dropped, grouped */
        strbuf_free(&sb) ;
    }
    {
        /* same input through sortndrop collapses each equal run to one */
        char const *un[] = { "b", "a", "b", "c", "a", "b" } ;
        strbuf sb = STRBUF_ZERO ;
        build(&sb, un, 6) ;
        TEST_ASSERT(sbl_sortndrop(&sb), "sbl_sortndrop with duplicates") ;
        char const *exp[] = { "a", "b", "c" } ;
        check_list(&sb, exp, 3) ;
        strbuf_free(&sb) ;
    }
    TEST_END() ;
}

/* Numeric comparator for sbl_sort_cmp: scan both operands as decimal u64 and
 * order by value, so "9" precedes "10" and "33" precedes "1000" -- an ordering
 * that strcmp (byte order) cannot produce for mixed-length decimal strings. */
static int cmp_numeric(char const *a, char const *b)
{
    uint64_t ua = 0, ub = 0 ;
    u64_scan(a, &ua) ;
    u64_scan(b, &ub) ;
    return ua < ub ? -1 : ua > ub ? 1 : 0 ;
}

/* sbl_sort_cmp with a numeric comparator yields value order, and sbl_sort on the
 * very same data yields lexicographic (byte) order -- proving the comparator is
 * honoured and that the two orderings genuinely differ on this dataset. */
static void test_sort_cmp(void)
{
    TEST_START("test_sort_cmp") ;
    /* mixed-length decimals where value order and byte order disagree */
    char const *un[] = { "10", "2", "1000", "9", "33" } ;
    {
        strbuf sb = STRBUF_ZERO ;
        build(&sb, un, 5) ;
        errno = 0 ;
        TEST_ASSERT(sbl_sort_cmp(&sb, cmp_numeric), "sbl_sort_cmp numeric") ;
        char const *exp[] = { "2", "9", "10", "33", "1000" } ;
        check_list(&sb, exp, 5) ;
        strbuf_free(&sb) ;
    }
    {
        /* non-regression: the same input through plain sbl_sort stays byte-sorted */
        strbuf sb = STRBUF_ZERO ;
        build(&sb, un, 5) ;
        TEST_ASSERT(sbl_sort(&sb), "sbl_sort lexicographic") ;
        char const *exp[] = { "10", "1000", "2", "33", "9" } ;
        check_list(&sb, exp, 5) ;
        strbuf_free(&sb) ;
    }
    TEST_END() ;
}

/* Like sbl_sort, sbl_sort_cmp reports the empty list as 0 WITHOUT setting errno
 * (the "if (!sb->len) return 0" guard runs before the comparator is ever used). */
static void test_sort_cmp_empty(void)
{
    TEST_START("test_sort_cmp_empty") ;
    strbuf sb = STRBUF_ZERO ;
    errno = 0 ;
    TEST_ASSERT(sbl_sort_cmp(&sb, cmp_numeric) == 0, "empty list returns 0") ;
    TEST_ASSERT(errno == 0, "empty list leaves errno untouched") ;
    TEST_ASSERT(sb.len == 0, "empty list stays empty") ;
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
    /* sentinel: the byte right after the copied region must be untouched.
     * sbl_to_char must copy EXACTLY sb->len bytes, never one more (an off-by-one
     * memcpy of sb->len+1 would clobber this 0xAA). */
    TEST_ASSERT(dst[sb.len] == (char)0xAA, "to_char does not write past sb->len") ;
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
        TEST_ASSERT(sbl_dir_get(&sb, dir, excl, FLAGS_REG), "sbl_dir_get") ;
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
        TEST_ASSERT(sbl_dir_get_recursive(&sb, dir, none, FLAGS_REG, 1), "sbl rec") ;
        TEST_ASSERT(sbl_sort(&sb), "sort") ;
        check_list(&sb, exp, 4) ;
        strbuf_free(&sb) ;
    }
    /* dir_get_element: "a" present once */
    {
        strbuf sb = STRBUF_ZERO ;
        int n = sbl_dir_get_element(&sb, dir, "a", FLAGS_REG, 0) ;
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

/* ---- FLAGS_* filtering of the directory walkers ----
 * Mixed tree under a fresh temp dir R:
 *   R/f          regular file
 *   R/d/         directory, holding d/g (regular) and d/l2 -> g (symlink)
 *   R/lr -> f    symlink to a regular file
 *   R/ld -> d    symlink to a directory
 *   R/p          fifo
 *   R/s          unix socket
 * sbl_dir_get and sbl_dir_get_element follow links (fstatat flag 0), so lr is
 * a REG and ld a DIR for them, and no entry is ever a LNK. sbl_dir_get_recursive
 * does not follow (AT_SYMLINK_NOFOLLOW): lr, ld, l2 are LNK and ld is not
 * descended into. The old get_flags() ANDed S_IF* numbers: S_IFLNK & S_IFREG
 * and S_IFSOCK & S_IFDIR are non-zero, so regular files leaked into a LNK
 * listing and the socket into a DIR listing. */

static char mt_root[64] ;

static void mt_path(char *out, size_t len, char const *rel)
{
    snprintf(out, len, "%s/%s", mt_root, rel) ;
}

static void mixed_tree_create(void)
{
    char tmpl[] = "/tmp/sbl_flags_XXXXXX" ;
    TEST_ASSERT(mkdtemp(tmpl) != 0, "mkdtemp") ;
    snprintf(mt_root, sizeof mt_root, "%s", tmpl) ;
    char p[256], q[256] ;
    int fd ;
    mt_path(p, sizeof p, "f") ;
    fd = open(p, O_CREAT | O_EXCL | O_WRONLY, 0644) ; TEST_ASSERT(fd >= 0, "create f") ; close(fd) ;
    mt_path(p, sizeof p, "d") ;
    TEST_ASSERT(mkdir(p, 0755) == 0, "mkdir d") ;
    mt_path(p, sizeof p, "d/g") ;
    fd = open(p, O_CREAT | O_EXCL | O_WRONLY, 0644) ; TEST_ASSERT(fd >= 0, "create d/g") ; close(fd) ;
    mt_path(p, sizeof p, "d/l2") ;
    TEST_ASSERT(symlink("g", p) == 0, "symlink d/l2") ;
    mt_path(p, sizeof p, "lr") ;
    TEST_ASSERT(symlink("f", p) == 0, "symlink lr") ;
    mt_path(p, sizeof p, "ld") ;
    TEST_ASSERT(symlink("d", p) == 0, "symlink ld") ;
    mt_path(p, sizeof p, "p") ;
    TEST_ASSERT(mkfifo(p, 0644) == 0, "mkfifo p") ;
    mt_path(q, sizeof q, "s") ;
    int s = socket(AF_UNIX, SOCK_STREAM, 0) ;
    TEST_ASSERT(s >= 0, "socket") ;
    struct sockaddr_un sa ;
    memset(&sa, 0, sizeof sa) ;
    sa.sun_family = AF_UNIX ;
    TEST_ASSERT(strlen(q) < sizeof sa.sun_path, "socket path fits") ;
    memcpy(sa.sun_path, q, strlen(q) + 1) ;
    TEST_ASSERT(bind(s, (struct sockaddr *)&sa, sizeof sa) == 0, "bind s") ;
    close(s) ;
}

static void mixed_tree_destroy(void)
{
    char const *rel[] = { "s", "p", "ld", "lr", "d/l2", "d/g" } ;
    char p[256] ;
    for (size_t i = 0 ; i < sizeof rel / sizeof *rel ; i++) {
        mt_path(p, sizeof p, rel[i]) ; unlink(p) ;
    }
    mt_path(p, sizeof p, "d") ; rmdir(p) ;
    mt_path(p, sizeof p, "f") ; unlink(p) ;
    rmdir(mt_root) ;
}

/* list @srcdir with sbl_dir_get(@flags), sort, compare to exp[0..n) */
static void mt_check_get(uint32_t flags, char const *const *exp, size_t n)
{
    char const *none[] = { 0 } ;
    strbuf sb = STRBUF_ZERO ;
    errno = 0 ;
    TEST_ASSERT(sbl_dir_get(&sb, mt_root, none, flags) == 1, "sbl_dir_get returns 1") ;
    if (n) TEST_ASSERT(sbl_sort(&sb), "sort") ;
    check_list(&sb, exp, n) ;
    strbuf_free(&sb) ;
}

static void mt_check_rec(uint32_t flags, uint8_t include_path, char const *const *exp, size_t n)
{
    char const *none[] = { 0 } ;
    strbuf sb = STRBUF_ZERO ;
    errno = 0 ;
    TEST_ASSERT(sbl_dir_get_recursive(&sb, mt_root, none, flags, include_path) == 1, "sbl_dir_get_recursive returns 1") ;
    if (n) TEST_ASSERT(sbl_sort(&sb), "sort") ;
    check_list(&sb, exp, n) ;
    strbuf_free(&sb) ;
}

static void mt_check_elem(char const *element, uint32_t flags, uint8_t include_path, char const *exp)
{
    strbuf sb = STRBUF_ZERO ;
    errno = 0 ;
    int r = sbl_dir_get_element(&sb, mt_root, element, flags, include_path) ;
    if (exp) {
        TEST_ASSERT(r == 1, "sbl_dir_get_element finds exactly one") ;
        check_list(&sb, &exp, 1) ;
    } else {
        TEST_ASSERT(r == 0, "sbl_dir_get_element finds none") ;
        TEST_ASSERT(errno == 0, "no-match leaves errno unchanged") ;
        TEST_ASSERT(sb.len == 0, "no-match appends nothing") ;
    }
    strbuf_free(&sb) ;
}

static void test_dir_get_flags(void)
{
    TEST_START("test_dir_get_flags") ;
    mixed_tree_create() ;
    { char const *e[] = { "d", "f", "ld", "lr" } ; mt_check_get(FLAGS_REG | FLAGS_DIR, e, 4) ; }
    { char const *e[] = { "f", "lr" } ; mt_check_get(FLAGS_REG, e, 2) ; }
    { char const *e[] = { "d", "ld" } ; mt_check_get(FLAGS_DIR, e, 2) ; }   /* no socket */
    mt_check_get(FLAGS_LNK, 0, 0) ;                                        /* links followed */
    { char const *e[] = { "s" } ; mt_check_get(FLAGS_SOCK, e, 1) ; }
    { char const *e[] = { "p" } ; mt_check_get(FLAGS_FIFO, e, 1) ; }
    { char const *e[] = { "d", "f", "ld", "lr", "p", "s" } ; mt_check_get(FLAGS_ALL, e, 6) ; }
    mixed_tree_destroy() ;
    TEST_END() ;
}

static void test_dir_get_recursive_flags(void)
{
    TEST_START("test_dir_get_recursive_flags") ;
    mixed_tree_create() ;
    { char const *e[] = { "d", "f", "g" } ; mt_check_rec(FLAGS_REG | FLAGS_DIR, 0, e, 3) ; }
    /* the old bug: FLAGS_LNK must list links only, never f or g */
    { char const *e[] = { "l2", "ld", "lr" } ; mt_check_rec(FLAGS_LNK, 0, e, 3) ; }
    { char const *e[] = { "f", "g" } ; mt_check_rec(FLAGS_REG, 0, e, 2) ; }
    { char const *e[] = { "d" } ; mt_check_rec(FLAGS_DIR, 0, e, 1) ; }    /* no socket, ld not a dir */
    { char const *e[] = { "s" } ; mt_check_rec(FLAGS_SOCK, 0, e, 1) ; }
    { char const *e[] = { "d", "f", "g", "l2", "ld", "lr", "p", "s" } ; mt_check_rec(FLAGS_ALL, 0, e, 8) ; }
    {
        char a[256], b[256], c[256] ;
        mt_path(a, sizeof a, "d/l2") ; mt_path(b, sizeof b, "ld") ; mt_path(c, sizeof c, "lr") ;
        char const *e[] = { a, b, c } ;
        mt_check_rec(FLAGS_LNK, 1, e, 3) ;
    }
    mixed_tree_destroy() ;
    TEST_END() ;
}

static void test_dir_get_element_flags(void)
{
    TEST_START("test_dir_get_element_flags") ;
    mixed_tree_create() ;
    mt_check_elem("lr", FLAGS_REG, 0, "lr") ;           /* followed */
    mt_check_elem("lr", FLAGS_LNK, 0, 0) ;
    mt_check_elem("ld", FLAGS_DIR, 0, "ld") ;
    mt_check_elem("d", FLAGS_REG | FLAGS_DIR, 0, "d") ;
    mt_check_elem("f", FLAGS_REG | FLAGS_DIR, 0, "f") ;
    mt_check_elem("p", FLAGS_REG | FLAGS_DIR, 0, 0) ;
    mt_check_elem("s", FLAGS_DIR, 0, 0) ;
    mt_check_elem("s", FLAGS_REG, 0, 0) ;
    mt_check_elem("s", FLAGS_SOCK, 0, "s") ;
    mt_check_elem("p", FLAGS_ALL, 0, "p") ;
    {
        char a[256] ;
        mt_path(a, sizeof a, "lr") ;
        mt_check_elem("lr", FLAGS_REG, 1, a) ;
    }
    mixed_tree_destroy() ;
    TEST_END() ;
}

/* Invalid flags are rejected before opendir: EINVAL even on a missing dir, the
 * documented failure value, and sb untouched (same pointer, len, bytes; an
 * empty sb stays unallocated). */
static void test_dir_invalid_flags(void)
{
    TEST_START("test_dir_invalid_flags") ;
    mixed_tree_create() ;
    char missing[256] ;
    mt_path(missing, sizeof missing, "missing") ;
    char const *dirs[] = { mt_root, missing } ;
    uint32_t const bad[] = { 0, (uint32_t)S_IFREG, (uint32_t)S_IFDIR, FLAGS_ALL + (1u << 7) } ;
    char const *none[] = { 0 } ;
    for (size_t i = 0 ; i < sizeof bad / sizeof *bad ; i++) {
        for (size_t j = 0 ; j < 2 ; j++) {
            for (int fn = 0 ; fn < 3 ; fn++) {
                strbuf sb = STRBUF_ZERO ;
                TEST_ASSERT(sbl_add(&sb, "keep"), "seed") ;
                char *s0 = sb.s ;
                strbuf empty = STRBUF_ZERO ;
                int r = 0, re = 0 ;
                errno = 0 ;
                if (fn == 0) r = sbl_dir_get(&sb, dirs[j], none, bad[i]) ;
                else if (fn == 1) r = sbl_dir_get_recursive(&sb, dirs[j], none, bad[i], 1) ;
                else r = sbl_dir_get_element(&sb, dirs[j], "f", bad[i], 1) ;
                TEST_ASSERT(r == (fn == 2 ? -1 : 0), "documented failure value") ;
                TEST_ASSERT(errno == EINVAL, "EINVAL before opendir") ;
                TEST_ASSERT(sb.s == s0 && sb.len == 5 && !memcmp(sb.s, "keep", 5), "sb untouched") ;
                errno = 0 ;
                if (fn == 0) re = sbl_dir_get(&empty, dirs[j], none, bad[i]) ;
                else if (fn == 1) re = sbl_dir_get_recursive(&empty, dirs[j], none, bad[i], 0) ;
                else re = sbl_dir_get_element(&empty, dirs[j], "f", bad[i], 0) ;
                TEST_ASSERT(re == (fn == 2 ? -1 : 0), "documented failure value, empty sb") ;
                TEST_ASSERT(errno == EINVAL, "EINVAL, empty sb") ;
                TEST_ASSERT(!empty.s && !empty.len, "empty sb stays unallocated") ;
                strbuf_free(&sb) ;
            }
        }
    }
    mixed_tree_destroy() ;
    TEST_END() ;
}

/* A dangling link cannot be stat'ed by the following walkers: they fail with the
 * fstatat errno and restore sb. The non-following walker lists it as a LNK. */
static void test_dir_dangling_entry(void)
{
    TEST_START("test_dir_dangling_entry") ;
    char tmpl[] = "/tmp/sbl_dang_XXXXXX" ;
    char *dir = mkdtemp(tmpl) ;
    TEST_ASSERT(dir != 0, "mkdtemp") ;
    char lnk[256] ;
    snprintf(lnk, sizeof lnk, "%s/dang", dir) ;
    TEST_ASSERT(symlink("nowhere", lnk) == 0, "dangling symlink") ;
    char const *none[] = { 0 } ;
    {
        strbuf sb = STRBUF_ZERO ;
        TEST_ASSERT(sbl_add(&sb, "keep"), "seed") ;
        errno = 0 ;
        TEST_ASSERT(sbl_dir_get(&sb, dir, none, FLAGS_ALL) == 0, "sbl_dir_get fails") ;
        TEST_ASSERT(errno == ENOENT, "fstatat ENOENT") ;
        TEST_ASSERT(sb.len == 5 && !strcmp(sb.s, "keep"), "sb truncated back") ;
        strbuf_free(&sb) ;
    }
    {
        strbuf sb = STRBUF_ZERO ;
        errno = 0 ;
        TEST_ASSERT(sbl_dir_get_element(&sb, dir, "dang", FLAGS_ALL, 0) == -1, "element fails") ;
        TEST_ASSERT(errno == ENOENT, "fstatat ENOENT") ;
        TEST_ASSERT(!sb.s && !sb.len, "empty sb freed") ;
    }
    {
        char const *e[] = { "dang" } ;
        strbuf sb = STRBUF_ZERO ;
        TEST_ASSERT(sbl_dir_get_recursive(&sb, dir, none, FLAGS_LNK, 0) == 1, "recursive lists it") ;
        check_list(&sb, e, 1) ;
        strbuf_free(&sb) ;
    }
    unlink(lnk) ;
    rmdir(dir) ;
    TEST_END() ;
}

/* A missing srcdir with valid flags: the opendir errno, the documented failure
 * value, and sb untouched (seeded sb keeps its bytes, empty sb stays NULL). */
static void test_dir_opendir_failure(void)
{
    TEST_START("test_dir_opendir_failure") ;
    char const *none[] = { 0 } ;
    char const *missing = "/tmp/sbl_no_such_dir_for_test" ;
    for (int fn = 0 ; fn < 3 ; fn++) {
        strbuf sb = STRBUF_ZERO, empty = STRBUF_ZERO ;
        TEST_ASSERT(sbl_add(&sb, "keep"), "seed") ;
        int r, re ;
        errno = 0 ;
        if (fn == 0) r = sbl_dir_get(&sb, missing, none, FLAGS_ALL) ;
        else if (fn == 1) r = sbl_dir_get_recursive(&sb, missing, none, FLAGS_ALL, 1) ;
        else r = sbl_dir_get_element(&sb, missing, "f", FLAGS_ALL, 1) ;
        TEST_ASSERT(r == (fn == 2 ? -1 : 0), "documented failure value") ;
        TEST_ASSERT(errno == ENOENT, "opendir ENOENT") ;
        TEST_ASSERT(sb.len == 5 && !memcmp(sb.s, "keep", 5), "seeded sb untouched") ;
        errno = 0 ;
        if (fn == 0) re = sbl_dir_get(&empty, missing, none, FLAGS_ALL) ;
        else if (fn == 1) re = sbl_dir_get_recursive(&empty, missing, none, FLAGS_ALL, 0) ;
        else re = sbl_dir_get_element(&empty, missing, "f", FLAGS_ALL, 0) ;
        TEST_ASSERT(re == (fn == 2 ? -1 : 0), "documented failure value, empty sb") ;
        TEST_ASSERT(errno == ENOENT, "opendir ENOENT, empty sb") ;
        TEST_ASSERT(!empty.s && !empty.len, "empty sb stays unallocated") ;
        strbuf_free(&sb) ;
    }
    TEST_END() ;
}

/* Documented order of sbl_dir_get_recursive: a directory's descendants come
 * BEFORE the directory itself (raw, unsorted output). Also: sbl_dir_get_element
 * does not set errno when it finds the element. */
static void test_dir_recursive_order(void)
{
    TEST_START("test_dir_recursive_order") ;
    mixed_tree_create() ;
    char const *none[] = { 0 } ;
    strbuf sb = STRBUF_ZERO ;
    TEST_ASSERT(sbl_dir_get_recursive(&sb, mt_root, none, FLAGS_REG | FLAGS_DIR, 0) == 1, "recursive") ;
    ssize_t og = sbl_search(&sb, "g"), od = sbl_search(&sb, "d") ;
    TEST_ASSERT(og >= 0 && od >= 0, "g and d listed") ;
    TEST_ASSERT(og < od, "d/g appended before d") ;
    strbuf_free(&sb) ;
    strbuf e = STRBUF_ZERO ;
    errno = EBADF ;
    TEST_ASSERT(sbl_dir_get_element(&e, mt_root, "f", FLAGS_REG, 0) == 1, "element found") ;
    TEST_ASSERT(errno == EBADF, "errno untouched on success") ;
    strbuf_free(&e) ;
    mixed_tree_destroy() ;
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
    test_sort_empty() ;
    test_sort_duplicates() ;
    test_sort_cmp() ;
    test_sort_cmp_empty() ;
    test_dropdup_sortndrop() ;
    test_dir_drop_child() ;
    test_rebuild() ;
    test_to_char() ;
    test_insert() ;
    test_rebuild_with_delim() ;
    test_dir_get() ;
    test_dir_get_flags() ;
    test_dir_get_recursive_flags() ;
    test_dir_get_element_flags() ;
    test_dir_invalid_flags() ;
    test_dir_dangling_entry() ;
    test_dir_opendir_failure() ;
    test_dir_recursive_order() ;
    test_clean_string() ;
    test_split_nline() ;
    test_replace() ;
    test_replace_nline() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
