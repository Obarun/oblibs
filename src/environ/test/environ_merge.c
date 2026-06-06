/*
 * environ_merge.c
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

/* Tests for environ_merge (oblibs-native, replacing skalibs env_merge). Each
 * expected result was CAPTURED from a byte-exact differential run against
 * env_merge and is now pinned as a literal, so the test is self-contained (no
 * skalibs). A modification entry "K=V" sets/overrides, a bare "K" unsets. The
 * returned count includes the terminating NULL slot. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <oblibs/environ.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            exit(1) ; \
        } \
    } while(0)

/*
 * Run environ_merge and assert the result matches the captured golden:
 * @exp_ret is the full return (entries + 1 for the NULL), @exp[0..exp_ret-1)
 * the expected entries in order.
 */
static void check(char const *name,
                  char const *const *envp, size_t envlen,
                  char const *modifs, size_t mlen,
                  size_t vmax,
                  size_t exp_ret, char const *const *exp)
{
    printf("Running %-22s ...", name) ;

    char const *v[64] ;
    TEST_ASSERT(vmax <= 64, "vmax fits the fixed array") ;

    size_t r = environ_merge(v, vmax, envp, envlen, modifs, mlen) ;

    TEST_ASSERT(r == exp_ret, "return count") ;
    if (r) {
        for (size_t k = 0 ; k + 1 < r ; k++)
            TEST_ASSERT(!strcmp(v[k], exp[k]), exp[k]) ;
        TEST_ASSERT(v[r - 1] == 0, "NULL-terminated") ;
    }

    printf(" PASS\n") ;
}

int main(void)
{
    static char const *const env[] = { "A=1", "B=2", "C=3", 0 } ;
    size_t envlen = 3 ;

    printf("=== environ_merge test suite ===\n\n") ;

    { static char const m[] = "D=4\0" ;
      static char const *const e[] = { "A=1", "B=2", "C=3", "D=4" } ;
      check("add new", env, envlen, m, sizeof m - 1, 16, 5, e) ; }

    { static char const m[] = "B=20\0" ;
      static char const *const e[] = { "A=1", "C=3", "B=20" } ;   /* swap-with-last removal */
      check("override existing", env, envlen, m, sizeof m - 1, 16, 4, e) ; }

    { static char const m[] = "B\0" ;
      static char const *const e[] = { "A=1", "C=3" } ;
      check("unset existing", env, envlen, m, sizeof m - 1, 16, 3, e) ; }

    { static char const m[] = "Z\0" ;
      static char const *const e[] = { "A=1", "B=2", "C=3" } ;
      check("unset missing", env, envlen, m, sizeof m - 1, 16, 4, e) ; }

    { static char const m[] = "E=\0" ;
      static char const *const e[] = { "A=1", "B=2", "C=3", "E=" } ;
      check("set empty value", env, envlen, m, sizeof m - 1, 16, 5, e) ; }

    { static char const m[] = "A=10\0D=4\0C\0" ;
      static char const *const e[] = { "D=4", "B=2", "A=10" } ;
      check("mixed batch", env, envlen, m, sizeof m - 1, 16, 4, e) ; }

    { static char const m[] = "B=x\0B=y\0" ;
      static char const *const e[] = { "A=1", "C=3", "B=y" } ;
      check("dup key in batch", env, envlen, m, sizeof m - 1, 16, 4, e) ; }

    { static char const m[] = "D=4\0" ;
      check("overflow vmax", env, envlen, m, sizeof m - 1, 2, 0, 0) ; }

    { static char const *const e[] = { "A=1", "B=2", "C=3" } ;
      check("empty modifs", env, envlen, "", 0, 16, 4, e) ; }

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
