/*
 * framework.h
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <oblibs/log.h>

int test_count = 0;
int test_failures = 0;

/* Test framework macros */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "ASSERTION FAILED: %s:%d: %s\n", __FILE__, __LINE__, message); \
            test_failures++; \
            return false; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "ASSERTION FAILED: %s:%d: %s (expected: %d, got: %d)\n", \
                __FILE__, __LINE__, message, (int)(expected), (int)(actual)); \
            test_failures++; \
            return false; \
        } \
    } while (0)

#define TEST_ASSERT_NEQ(unexpected, actual, message) \
    do { \
        if ((unexpected) == (actual)) { \
            fprintf(stderr, "ASSERTION FAILED: %s:%d: %s (unexpected value: %d)\n", \
                __FILE__, __LINE__, message, (int)(actual)); \
            test_failures++; \
            return false; \
        } \
    } while (0)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

#define TEST_ASSERT_ERRNO(expected_errno, message) \
    TEST_ASSERT_EQ(expected_errno, errno, message)

#define RUN_TEST(test_func) \
    do { \
        printf("Running %s... ", #test_func); \
        fflush(stdout); \
        test_count++; \
        if (test_func()) { \
            printf("PASSED\n"); \
        } else { \
            printf("FAILED\n"); \
        } \
    } while (0)

#define TEST_SUITE_BEGIN(suite_name) \
    int main(void) { \
        VERBOSITY = LOG_LEVEL_WARN; \
        PROG = suite_name; \
        printf("=== %s ===\n", suite_name); \
        test_count = 0; \
        test_failures = 0;

#define TEST_SUITE_END() \
        printf("\n=== Results ===\n"); \
        printf("Tests run: %d\n", test_count); \
        printf("Failures: %d\n", test_failures); \
        if (test_failures == 0) { \
            printf("ALL TESTS PASSED\n"); \
            return EXIT_SUCCESS; \
        } else { \
            printf("SOME TESTS FAILED\n"); \
            return EXIT_FAILURE; \
        } \
    }

/* Helper functions */
static inline void test_sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000 };
    nanosleep(&ts, NULL);
}

static inline pid_t test_spawn_child(void (*child_func)(void)) {
    pid_t parent = getpid();
    pid_t pid = fork();
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        /* The parent may have died between the fork and the prctl, in which case
         * the signal will never come: re-check, a reparented child sees another
         * ppid. */
        if (getppid() != parent)
            _exit(1);
        child_func();
        exit(0);
    }
    return pid;
}

/* Test callback data structure */
struct test_callback_data {
    int call_count;
    int last_event;
    int expected_event;
    bool callback_called;
    void *user_data;
};

static inline void test_callback_init(struct test_callback_data *data) {
    memset(data, 0, sizeof(*data));
}

#endif /* TEST_FRAMEWORK_H */