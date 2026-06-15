/*
 * file_create.c
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

/* Non-regression test for the oblibs-native file_create_empty
 * (io_open_mode + close), which dropped skalibs' open3 / close_fd. Covers
 * creation (mode, empty, regular), the O_EXCL refusal, the dst-must-be-absolute
 * guard, the trailing-slash handling, and errno preservation on success. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

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

static char tdir[] = "/tmp/oblibs_fc_XXXXXX" ;

/* build "<tdir>/<name>" into out */
static void path_in(char *out, char const *name)
{
    size_t dl = strlen(tdir), nl = strlen(name) ;
    memcpy(out, tdir, dl) ;
    out[dl] = '/' ;
    memcpy(out + dl + 1, name, nl + 1) ;
}

/* Case 1+2+5: create -> regular, size 0, mode honoured; second create -> EEXIST;
 * errno preserved on the successful create. */
static void test_create(void)
{
    TEST_START("test_create") ;

    char p[64] ;
    path_in(p, "f") ;

    errno = EXDEV ;   /* sentinel: a successful call must restore it */
    TEST_ASSERT(file_create_empty(tdir, "f", 0644) == 1, "create failed") ;
    TEST_ASSERT(errno == EXDEV, "errno not preserved on success") ;

    struct stat st ;
    TEST_ASSERT(stat(p, &st) == 0, "created file missing") ;
    TEST_ASSERT(S_ISREG(st.st_mode), "not a regular file") ;
    TEST_ASSERT(st.st_size == 0, "file not empty") ;
    TEST_ASSERT((st.st_mode & 07777) == (mode_t)(0644 & ~022), "wrong mode") ;

    /* O_EXCL: a second create on the same path must fail with EEXIST */
    errno = 0 ;
    TEST_ASSERT(file_create_empty(tdir, "f", 0644) == 0, "second create should fail (O_EXCL)") ;
    TEST_ASSERT(errno == EEXIST, "errno should be EEXIST") ;

    file_tryunlink(p) ;
    TEST_END() ;
}

/* Case 3: dst not starting with '/' is rejected. */
static void test_relative_rejected(void)
{
    TEST_START("test_relative_rejected") ;

    TEST_ASSERT(file_create_empty("relative/dir", "f", 0644) == 0, "relative dst should be rejected") ;

    TEST_END() ;
}

/* Case 4: a trailing slash on dst yields the same path (no '//'). */
static void test_trailing_slash(void)
{
    TEST_START("test_trailing_slash") ;

    char withslash[64] ;
    size_t dl = strlen(tdir) ;
    memcpy(withslash, tdir, dl) ;
    withslash[dl] = '/' ;
    withslash[dl + 1] = 0 ;

    char p[64] ;
    path_in(p, "g") ;

    TEST_ASSERT(file_create_empty(withslash, "g", 0644) == 1, "create with trailing slash failed") ;
    struct stat st ;
    TEST_ASSERT(stat(p, &st) == 0, "file not at expected path (// mishandled?)") ;

    file_tryunlink(p) ;
    TEST_END() ;
}

int main(void)
{
    umask(022) ;   /* deterministic resulting mode */

    TEST_ASSERT(mkdtemp(tdir) != NULL, "mkdtemp failed") ;

    printf("=== file_create_empty test suite ===\n\n") ;

    test_create() ;
    test_relative_rejected() ;
    test_trailing_slash() ;

    rmdir(tdir) ;

    printf("\nAll tests passed.\n") ;
    return 0 ;
}
