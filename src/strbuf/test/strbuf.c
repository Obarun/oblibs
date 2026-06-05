/*
 * strbuf.c
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
 */

/* Rigorous tests for the strbuf growable buffer. Emphasis on the pointer /
 * ownership contract (s NULL vs non-NULL, seed vs heap, content survival across
 * realloc and across the borrowed->heap migration), capacity invariants
 * (cap >= len, geometric growth, reserve as writable space), every edge
 * (offset > len, n == 0, fill-to-capacity, macro fallback), and the lifecycle
 * traps: never free() a borrowed (stack) buffer, double-free safety, reuse
 * after free. The contract is "bytes in [len, cap) are undefined", so nothing
 * is ever asserted past len. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/uio.h>

#include <oblibs/strbuf.h>

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

/* ---- zero state, free safety, reuse ---- */

static void test_zero_and_free(void)
{
    TEST_START("test_zero_and_free") ;

    strbuf sb = STRBUF_ZERO ;
    TEST_ASSERT(sb.s == NULL, "ZERO: s NULL") ;
    TEST_ASSERT(sb.len == 0 && sb.cap == 0 && sb.heap == 0, "ZERO: fields") ;

    /* free on a pristine zero buffer must be a safe no-op */
    strbuf_free(&sb) ;
    TEST_ASSERT(sb.s == NULL && sb.len == 0 && sb.cap == 0 && sb.heap == 0, "free ZERO") ;

    /* allocate, free, then reuse the same struct */
    TEST_ASSERT(strbuf_cats(&sb, "hello"), "cats") ;
    TEST_ASSERT(sb.s != NULL && sb.heap == 1, "alloc'd") ;
    strbuf_free(&sb) ;
    TEST_ASSERT(sb.s == NULL && sb.heap == 0, "free resets") ;

    /* double free: second call is a no-op (heap == 0 now) */
    strbuf_free(&sb) ;
    TEST_ASSERT(sb.s == NULL, "double free safe") ;

    /* reuse after free */
    TEST_ASSERT(strbuf_cats(&sb, "again"), "reuse") ;
    TEST_ASSERT(sb.len == 5 && memcmp(sb.s, "again", 5) == 0, "reuse content") ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

/* ---- reserve: no-op when it fits, grows otherwise, gives writable space ---- */

static void test_reserve(void)
{
    TEST_START("test_reserve") ;

    strbuf sb = STRBUF_ZERO ;

    /* reserve(0) on empty: no allocation */
    TEST_ASSERT(strbuf_reserve(&sb, 0), "reserve 0") ;
    TEST_ASSERT(sb.s == NULL && sb.cap == 0, "reserve 0 no alloc") ;

    /* reserve N: heap, cap >= N, len untouched, writable */
    TEST_ASSERT(strbuf_reserve(&sb, 50), "reserve 50") ;
    TEST_ASSERT(sb.s != NULL && sb.heap == 1, "reserve heaps") ;
    TEST_ASSERT(sb.cap >= 50, "cap >= 50") ;
    TEST_ASSERT(sb.len == 0, "reserve keeps len") ;

    /* use the reserved space directly, as a read-into-buffer would */
    memset(sb.s, 'Q', 50) ;
    sb.len = 50 ;
    for (int i = 0 ; i < 50 ; i++)
        TEST_ASSERT(sb.s[i] == 'Q', "reserved space writable") ;

    /* reserve <= cap is a no-op: same pointer, same cap */
    char *before = sb.s ;
    size_t cap_before = sb.cap ;
    TEST_ASSERT(strbuf_reserve(&sb, 10), "reserve smaller") ;
    TEST_ASSERT(sb.s == before && sb.cap == cap_before, "reserve <= cap no-op") ;

    strbuf_free(&sb) ;
    TEST_END() ;
}

/* ---- growth is geometric, content survives every realloc ---- */

static void test_geometric_growth(void)
{
    TEST_START("test_geometric_growth") ;

    strbuf sb = STRBUF_ZERO ;
    size_t reallocs = 0, last_cap = 0 ;
    const int N = 20000 ;

    for (int i = 0 ; i < N ; i++) {
        char c = (char)('a' + (i % 26)) ;
        TEST_ASSERT(strbuf_catb(&sb, &c, 1), "catb byte") ;
        TEST_ASSERT(sb.cap >= sb.len, "cap >= len invariant") ;
        if (sb.cap != last_cap) { reallocs++ ; last_cap = sb.cap ; }
    }

    TEST_ASSERT(sb.len == (size_t)N, "len == N") ;
    /* geometric (1.5x) => O(log N) reallocs, NOT O(N). 20000 bytes must take
     * far fewer than, say, 60 growth steps; a linear policy would take ~20000. */
    TEST_ASSERT(reallocs < 60, "growth must be geometric, not linear") ;

    for (int i = 0 ; i < N ; i++)
        TEST_ASSERT(sb.s[i] == (char)('a' + (i % 26)), "content survived reallocs") ;

    strbuf_free(&sb) ;
    TEST_END() ;
}

/* ---- borrowed storage: fill to 100%, migrate on overflow, pointers ---- */

static void test_borrowed_and_migration(void)
{
    TEST_START("test_borrowed_and_migration") ;

    char seed[8] ;
    strbuf sb ;
    TEST_ASSERT(strbuf_with(&sb, seed, sizeof seed), "with") ;
    TEST_ASSERT(sb.s == seed && sb.heap == 0 && sb.cap == 8, "seeded") ;

    /* fill to exactly capacity: still borrowed, no heap */
    TEST_ASSERT(strbuf_catb(&sb, "01234567", 8), "fill to cap") ;
    TEST_ASSERT(sb.len == 8 && sb.heap == 0 && sb.s == seed, "100% borrowed") ;

    /* one more byte: migrate to heap, content preserved, pointer moved off seed */
    TEST_ASSERT(strbuf_catb(&sb, "8", 1), "overflow") ;
    TEST_ASSERT(sb.heap == 1, "migrated") ;
    TEST_ASSERT(sb.s != seed, "moved off seed") ;
    TEST_ASSERT(sb.len == 9 && memcmp(sb.s, "012345678", 9) == 0, "content kept") ;

    /* keep growing on the heap */
    TEST_ASSERT(strbuf_cats(&sb, "9abc"), "grow on heap") ;
    TEST_ASSERT(sb.len == 13 && memcmp(sb.s, "0123456789abc", 13) == 0, "heap content") ;

    strbuf_free(&sb) ;
    TEST_ASSERT(sb.s == NULL, "free after migration") ;
    TEST_END() ;
}

/* freeing a still-borrowed strbuf must NOT free the stack buffer (would crash);
 * it just resets the struct. */
static void test_free_borrowed_does_not_free_stack(void)
{
    TEST_START("test_free_borrowed_does_not_free_stack") ;

    char seed[16] ;
    strbuf sb ;
    TEST_ASSERT(strbuf_with(&sb, seed, sizeof seed), "with") ;
    TEST_ASSERT(strbuf_cats(&sb, "abc"), "cats") ;
    TEST_ASSERT(sb.heap == 0, "still borrowed") ;

    strbuf_free(&sb) ;   /* must NOT call free(seed) */
    TEST_ASSERT(sb.s == NULL && sb.heap == 0, "reset, no crash") ;
    seed[0] = 'x' ;      /* seed is still our valid stack memory */
    TEST_ASSERT(seed[0] == 'x', "seed still usable") ;
    TEST_END() ;
}

/* ---- copyb replaces (grow then shrink len), empty copies ---- */

static void test_copyb(void)
{
    TEST_START("test_copyb") ;

    strbuf sb = STRBUF_ZERO ;

    TEST_ASSERT(strbuf_copys(&sb, "a longer string"), "copy long") ;
    TEST_ASSERT(sb.len == 15 && memcmp(sb.s, "a longer string", 15) == 0, "long") ;
    size_t cap_after_long = sb.cap ;

    TEST_ASSERT(strbuf_copys(&sb, "hi"), "copy short") ;   /* len shrinks, cap stays */
    TEST_ASSERT(sb.len == 2 && memcmp(sb.s, "hi", 2) == 0, "short") ;
    TEST_ASSERT(sb.cap == cap_after_long, "copy shorter keeps cap") ;

    /* empty copy resets len to 0 */
    TEST_ASSERT(strbuf_copyb(&sb, "x", 0), "copy empty") ;
    TEST_ASSERT(sb.len == 0, "empty copy len 0") ;

    strbuf_free(&sb) ;
    TEST_END() ;
}

/* empty catb on a pristine buffer allocates nothing */
static void test_catb_empty(void)
{
    TEST_START("test_catb_empty") ;

    strbuf sb = STRBUF_ZERO ;
    TEST_ASSERT(strbuf_catb(&sb, "ignored", 0), "catb 0") ;
    TEST_ASSERT(sb.s == NULL && sb.len == 0, "no alloc on empty catb") ;
    strbuf_free(&sb) ;
    TEST_END() ;
}

/* ---- insertb: prepend / append / middle / bounds / n==0 ---- */

static void test_insertb(void)
{
    TEST_START("test_insertb") ;

    strbuf sb = STRBUF_ZERO ;
    TEST_ASSERT(strbuf_copys(&sb, "abc"), "seed abc") ;

    TEST_ASSERT(strbuf_inserts(&sb, 0, "XY"), "prepend") ;          /* XYabc */
    TEST_ASSERT(sb.len == 5 && memcmp(sb.s, "XYabc", 5) == 0, "prepend ok") ;

    TEST_ASSERT(strbuf_inserts(&sb, sb.len, "Z"), "append via insert") ; /* XYabcZ */
    TEST_ASSERT(sb.len == 6 && memcmp(sb.s, "XYabcZ", 6) == 0, "append ok") ;

    TEST_ASSERT(strbuf_inserts(&sb, 2, "--"), "middle") ;          /* XY--abcZ */
    TEST_ASSERT(sb.len == 8 && memcmp(sb.s, "XY--abcZ", 8) == 0, "middle ok") ;

    /* offset > len: EINVAL, buffer unchanged */
    errno = 0 ;
    TEST_ASSERT(!strbuf_insertb(&sb, 99, "no", 2), "offset>len fails") ;
    TEST_ASSERT(errno == EINVAL, "EINVAL") ;
    TEST_ASSERT(sb.len == 8 && memcmp(sb.s, "XY--abcZ", 8) == 0, "unchanged on fail") ;

    /* n == 0: no-op */
    TEST_ASSERT(strbuf_insertb(&sb, 3, "", 0), "insert 0") ;
    TEST_ASSERT(sb.len == 8 && memcmp(sb.s, "XY--abcZ", 8) == 0, "insert 0 no-op") ;

    strbuf_free(&sb) ;
    TEST_END() ;
}

/* ---- catv: multi, zero-length entries, empty array ---- */

static void test_catv(void)
{
    TEST_START("test_catv") ;

    strbuf sb = STRBUF_ZERO ;

    struct iovec v[3] = { { "ab", 2 }, { "", 0 }, { "cde", 3 } } ;
    TEST_ASSERT(strbuf_catv(&sb, v, 3), "catv mixed") ;
    TEST_ASSERT(sb.len == 5 && memcmp(sb.s, "abcde", 5) == 0, "catv content") ;

    /* empty array: no-op */
    TEST_ASSERT(strbuf_catv(&sb, v, 0), "catv 0") ;
    TEST_ASSERT(sb.len == 5, "catv 0 no-op") ;

    strbuf_free(&sb) ;
    TEST_END() ;
}

/* ---- append + end produces a valid C string ---- */

static void test_terminate_cstring(void)
{
    TEST_START("test_terminate_cstring") ;

    strbuf sb = STRBUF_ZERO ;
    for (char c = 'A' ; c <= 'E' ; c++)
        TEST_ASSERT(strbuf_catb(&sb, &c, 1), "catb byte") ;
    TEST_ASSERT(sb.len == 5, "len 5") ;

    TEST_ASSERT(strbuf_terminate(&sb), "terminate") ;   /* append the NUL */
    TEST_ASSERT(sb.len == 6, "len includes NUL") ;
    TEST_ASSERT(sb.s[5] == 0, "NUL byte present") ;
    TEST_ASSERT(strlen(sb.s) == 5, "valid C string") ;
    TEST_ASSERT(strcmp(sb.s, "ABCDE") == 0, "string value") ;

    strbuf_free(&sb) ;
    TEST_END() ;
}

/* ---- shrink: borrowed no-op, heap shrinks, zero-length ---- */

static void test_shrink(void)
{
    TEST_START("test_shrink") ;

    /* borrowed: shrink is a no-op (we own nothing) */
    char seed[32] ;
    strbuf bb ;
    TEST_ASSERT(strbuf_with(&bb, seed, sizeof seed), "with") ;
    TEST_ASSERT(strbuf_cats(&bb, "abc"), "cats") ;
    TEST_ASSERT(strbuf_shrink(&bb), "shrink borrowed") ;
    TEST_ASSERT(bb.heap == 0 && bb.s == seed && bb.cap == 32, "borrowed shrink no-op") ;

    /* heap: cap drops to len, content kept */
    strbuf sb = STRBUF_ZERO ;
    TEST_ASSERT(strbuf_reserve(&sb, 200), "reserve big") ;
    TEST_ASSERT(strbuf_cats(&sb, "data"), "cats") ;
    TEST_ASSERT(sb.cap >= 200, "big cap") ;
    TEST_ASSERT(strbuf_shrink(&sb), "shrink heap") ;
    TEST_ASSERT(sb.cap == sb.len, "cap == len") ;
    TEST_ASSERT(memcmp(sb.s, "data", 4) == 0, "content after shrink") ;
    strbuf_free(&sb) ;

    /* heap, len 0: shrink reallocs to 1 (never realloc(p,0)) */
    strbuf z = STRBUF_ZERO ;
    TEST_ASSERT(strbuf_reserve(&z, 100), "reserve") ;
    TEST_ASSERT(z.len == 0 && z.heap == 1, "empty heap") ;
    TEST_ASSERT(strbuf_shrink(&z), "shrink empty heap") ;
    TEST_ASSERT(z.cap == 1 && z.heap == 1, "shrink to 1") ;
    strbuf_free(&z) ;

    TEST_END() ;
}

/* ---- _alloc_strbuf_: stack seed, auto-free, and the > THRESHOLD fallback ---- */

static void test_alloc_macro(void)
{
    TEST_START("test_alloc_macro") ;

    {
        _alloc_strbuf_(sb, 16) ;
        TEST_ASSERT(strbuf_cats(&sb, "small"), "small") ;
        TEST_ASSERT(sb.heap == 0, "small stays off-heap") ;

        char big[64] ;
        memset(big, 'Z', sizeof big) ;
        TEST_ASSERT(strbuf_catb(&sb, big, sizeof big), "big") ;
        TEST_ASSERT(sb.heap == 1 && sb.len == 5 + 64, "big migrated") ;
        TEST_ASSERT(memcmp(sb.s, "small", 5) == 0, "prefix preserved") ;
        /* auto-freed here */
    }

    {
        /* boundary: n == STRBUF_THRESHOLD is allowed on the stack (inclusive cap) */
        _alloc_strbuf_(edge, STRBUF_THRESHOLD) ;
        char blob[STRBUF_THRESHOLD] ;
        memset(blob, 'k', sizeof blob) ;
        TEST_ASSERT(strbuf_catb(&edge, blob, STRBUF_THRESHOLD), "fill to cap") ;
        TEST_ASSERT(edge.heap == 0, "n == THRESHOLD stays on stack") ;
        /* auto-freed here */
    }

    {
        /* n > STRBUF_THRESHOLD -> 1-byte seed, heaps on first real write */
        _alloc_strbuf_(huge, 100000) ;
        TEST_ASSERT(strbuf_cats(&huge, "xy"), "write") ;
        TEST_ASSERT(huge.heap == 1, "fallback heaps immediately") ;
        TEST_ASSERT(huge.len == 2 && memcmp(huge.s, "xy", 2) == 0, "content") ;
        /* auto-freed here */
    }

    TEST_END() ;
}

/* ---- a mixed sequence checked against an independent reference ---- */

static void test_integrity_mixed(void)
{
    TEST_START("test_integrity_mixed") ;

    strbuf sb = STRBUF_ZERO ;
    char ref[4096] ;
    size_t rl = 0 ;

    for (int round = 0 ; round < 200 ; round++) {
        char piece[17] ;
        size_t pl = (round % 16) + 1 ;
        for (size_t i = 0 ; i < pl ; i++) piece[i] = (char)('!' + ((round + i) % 90)) ;

        if (round % 5 == 0) {
            /* insert near the front */
            size_t off = rl < 3 ? rl : 3 ;
            TEST_ASSERT(strbuf_insertb(&sb, off, piece, pl), "insert") ;
            memmove(ref + off + pl, ref + off, rl - off) ;
            memcpy(ref + off, piece, pl) ;
            rl += pl ;
        } else {
            TEST_ASSERT(strbuf_catb(&sb, piece, pl), "cat") ;
            memcpy(ref + rl, piece, pl) ;
            rl += pl ;
        }
        TEST_ASSERT(sb.len == rl, "len tracks reference") ;
        TEST_ASSERT(sb.cap >= sb.len, "cap invariant") ;
    }

    TEST_ASSERT(memcmp(sb.s, ref, rl) == 0, "full content matches reference") ;

    strbuf_free(&sb) ;
    TEST_END() ;
}

int main(void)
{
    printf("=== strbuf test suite ===\n\n") ;

    test_zero_and_free() ;
    test_reserve() ;
    test_geometric_growth() ;
    test_borrowed_and_migration() ;
    test_free_borrowed_does_not_free_stack() ;
    test_copyb() ;
    test_catb_empty() ;
    test_insertb() ;
    test_catv() ;
    test_terminate_cstring() ;
    test_shrink() ;
    test_alloc_macro() ;
    test_integrity_mixed() ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
