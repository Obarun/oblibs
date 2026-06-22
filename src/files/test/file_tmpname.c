/*
 * file_tmpname.c
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
 * Structural test for file_tmpname: the output is random, so we check the
 * contract (return value, charset, in-place fill, uniqueness) rather than exact
 * bytes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <oblibs/files.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            exit(1) ; \
        } \
    } while(0)

#define ALPHABET "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_"
#define N 6

int main (void)
{
    printf("Running file_tmpname...") ;

    /* fills exactly N bytes in place, all within the alphabet, no overrun. */
    {
        char s[] = "prefix-XXXXXX" ;
        char *x = s + (sizeof s - 1) - N ;   /* the 6 placeholder bytes */
        int r = file_tmpname(x, N) ;
        TEST_ASSERT(r == 1, "file_tmpname succeeds") ;
        TEST_ASSERT(!strncmp(s, "prefix-", 7), "the prefix is left untouched") ;
        for (size_t i = 0 ; i < N ; i++)
            TEST_ASSERT(strchr(ALPHABET, x[i]) != 0, "every byte is in the alphabet") ;
    }

    /* two consecutive calls yield different suffixes (uniqueness). */
    {
        char a[N + 1] = {0} , b[N + 1] = {0} ;
        TEST_ASSERT(file_tmpname(a, N) == 1, "first call succeeds") ;
        TEST_ASSERT(file_tmpname(b, N) == 1, "second call succeeds") ;
        TEST_ASSERT(memcmp(a, b, N) != 0, "two calls produce distinct names") ;
    }

    printf(" PASS\n\nAll tests passed.\n") ;
    return 0 ;
}
