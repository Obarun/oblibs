/*
 * str_search.c
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

/* Test for str_search: index of the first @c, or strlen(s) when absent
 * (the same contract skalibs str_chr provided). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <oblibs/string.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            exit(1) ; \
        } \
    } while(0)

int main(void)
{
    printf("Running str_search...") ;

    TEST_ASSERT(str_search("a=b", '=') == 1, "middle") ;
    TEST_ASSERT(str_search("ab=", '=') == 2, "at end") ;
    TEST_ASSERT(str_search("==x", '=') == 0, "first occurrence") ;
    TEST_ASSERT(str_search("abc", '=') == 3, "absent -> strlen") ;
    TEST_ASSERT(str_search("", '=') == 0, "empty -> 0") ;
    TEST_ASSERT(str_search("KEY=val", '=') == 3, "env split") ;

    printf(" PASS\n\nAll tests passed.\n") ;
    return 0 ;
}
