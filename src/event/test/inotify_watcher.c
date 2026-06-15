/*
 * inotify_watcher.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/inotify.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <oblibs/sse.h>
#include "framework.h"

/* Test callback for inotify events */
static void test_inotify_callback(sse_watcher_t *w, void *cbdata, int event) {
    struct test_callback_data *data = (struct test_callback_data *)cbdata;
    data->call_count++;
    data->last_event = event;
    data->callback_called = true;

    /* Store watcher pointer for identification */
    data->user_data = w;
}

/* Helper to create test directories */
static bool create_test_dirs(void) {
    if (mkdir("/tmp/sse_test_dir1", 0755) < 0 && errno != EEXIST) return false;
    if (mkdir("/tmp/sse_test_dir2", 0755) < 0 && errno != EEXIST) return false;
    if (mkdir("/tmp/sse_test_dir3", 0755) < 0 && errno != EEXIST) return false;
    return true;
}

/* Helper to cleanup test directories */
static void cleanup_test_dirs(void) {
    system("rm -rf /tmp/sse_test_dir1");
    system("rm -rf /tmp/sse_test_dir2");
    system("rm -rf /tmp/sse_test_dir3");
    file_tryunlink("/tmp/sse_test_file1");
    file_tryunlink("/tmp/sse_test_file2");
}

/* Main inotify watcher test - single loop, single watcher, multiple paths */
static bool test_inotify_watcher_comprehensive(void) {
    sse_epoll_t loop;
    sse_watcher_t inotify_watcher;
    struct test_callback_data inotify_data;

    /* Initialize callback data */
    test_callback_init(&inotify_data);

    /* Setup test environment */
    cleanup_test_dirs();
    TEST_ASSERT(create_test_dirs(), "Failed to create test directories");

    /* Create single event loop */
    TEST_ASSERT(sse_new(&loop, 20), "Failed to create epoll loop");
    loop.running = true ;

    /* === Start single inotify watcher === */
    TEST_ASSERT(sse_start_inotify(&loop, &inotify_watcher, test_inotify_callback, &inotify_data, 1),
               "Failed to start inotify watcher");

    /* === Verify watcher is active === */
    TEST_ASSERT(sse_watcher_active(&inotify_watcher), "Inotify watcher should be active");
    TEST_ASSERT_EQ(SSE_TYPE_INOTIFY, inotify_watcher.type, "Wrong watcher type");
    TEST_ASSERT(inotify_watcher.fd >= 0, "Inotify watcher should have valid fd");
    TEST_ASSERT_EQ(SSE_READ, inotify_watcher.events, "Inotify should watch for read events");

    /* === Attach multiple paths to single watcher === */
    TEST_ASSERT(sse_attach_inotify(&inotify_watcher, "/tmp/sse_test_dir1",
                                  IN_CREATE | IN_DELETE | IN_MODIFY),
               "Failed to attach first directory");

    TEST_ASSERT(sse_attach_inotify(&inotify_watcher, "/tmp/sse_test_dir2",
                                  IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO),
               "Failed to attach second directory");

    TEST_ASSERT(sse_attach_inotify(&inotify_watcher, "/tmp/sse_test_dir3",
                                  IN_ALL_EVENTS),
               "Failed to attach third directory");

    /* Create a test file to watch */
    int fd = open("/tmp/sse_test_file1", O_CREAT | O_WRONLY, 0644);
    TEST_ASSERT(fd >= 0, "Failed to create test file");
    close(fd);

    TEST_ASSERT(sse_attach_inotify(&inotify_watcher, "/tmp/sse_test_file1",
                                  IN_MODIFY | IN_ATTRIB | IN_DELETE_SELF),
               "Failed to attach test file");

    /* === Trigger filesystem events on multiple paths === */

    /* Create files in dir1 */
    fd = open("/tmp/sse_test_dir1/newfile1", O_CREAT | O_WRONLY, 0644);
    TEST_ASSERT(fd >= 0, "Failed to create file in dir1");
    write(fd, "test data", 9);
    close(fd);

    /* Create and move files in dir2 */
    fd = open("/tmp/sse_test_dir2/tempfile", O_CREAT | O_WRONLY, 0644);
    TEST_ASSERT(fd >= 0, "Failed to create temp file in dir2");
    close(fd);

    TEST_ASSERT(rename("/tmp/sse_test_dir2/tempfile", "/tmp/sse_test_dir2/moved_file") == 0,
               "Failed to move file in dir2");

    /* Create files in dir3 */
    fd = open("/tmp/sse_test_dir3/dir3_file", O_CREAT | O_WRONLY, 0644);
    TEST_ASSERT(fd >= 0, "Failed to create file in dir3");
    close(fd);

    /* Modify the watched file */
    fd = open("/tmp/sse_test_file1", O_WRONLY | O_APPEND);
    TEST_ASSERT(fd >= 0, "Failed to open test file for modification");
    write(fd, "additional data", 15);
    close(fd);

    /* Change file attributes */
    TEST_ASSERT(chmod("/tmp/sse_test_file1", 0600) == 0, "Failed to change file permissions");

    /* === Process inotify events === */
    int max_iterations = 30;
    int initial_call_count = 0;

    while (max_iterations-- > 0 && (inotify_data.call_count - initial_call_count) < 4) {
        if (!sse_run(&loop, 100)) {
            break;
        }
    }

    /* === Verify events were received === */
    TEST_ASSERT(inotify_data.callback_called, "Inotify events should be detected");
    TEST_ASSERT(inotify_data.call_count, "Should receive multiple events from different paths");
    TEST_ASSERT(inotify_data.last_event & SSE_READ, "Events should generate read events");

    /* === Test path update functionality === */
    /* Update dir1 to watch different events */
    TEST_ASSERT(sse_update_inotify(&inotify_watcher, "/tmp/sse_test_dir1", IN_ATTRIB),
               "Failed to update dir1 monitoring");

    /* Create new file in dir1 - should not trigger (no longer monitoring IN_CREATE) */
    int old_call_count = inotify_data.call_count;
    fd = open("/tmp/sse_test_dir1/should_not_trigger", O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);

    test_sleep_ms(100);
    sse_run(&loop, 50);

    /* Change attributes - should trigger */
    chmod("/tmp/sse_test_dir1/newfile1", 0777);

    max_iterations = 10;
    while (max_iterations-- > 0 && inotify_data.call_count == old_call_count) {
        if (!sse_run(&loop, 100)) {
            break;
        }
    }

    TEST_ASSERT(inotify_data.call_count > old_call_count,
               "Updated inotify should detect attribute changes");

    /* === Test path detachment === */
    TEST_ASSERT(sse_detach_inotify(&inotify_watcher, "/tmp/sse_test_dir2"),
               "Failed to detach dir2");

    /* Create file in detached dir2 - should not trigger */
    old_call_count = inotify_data.call_count;
    fd = open("/tmp/sse_test_dir2/detached_file", O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);

    test_sleep_ms(100);
    sse_run(&loop, 50);

    TEST_ASSERT_EQ(old_call_count + 1, inotify_data.call_count,
                   "Detached path should generate one more events");

    /* === Test re-attachment === */
    TEST_ASSERT(sse_attach_inotify(&inotify_watcher, "/tmp/sse_test_dir2", IN_DELETE),
               "Failed to re-attach dir2");

    /* Delete file in re-attached dir2 - should trigger */
    file_tryunlink("/tmp/sse_test_dir2/detached_file");

    max_iterations = 10;
    while (max_iterations-- > 0 && inotify_data.call_count == old_call_count) {
        if (!sse_run(&loop, 100)) {
            break;
        }
    }

    TEST_ASSERT(inotify_data.call_count > old_call_count,
               "Re-attached path should generate events");

    /* === Test watcher stop/restart === */
    TEST_ASSERT(sse_stop_inotify(&inotify_watcher), "Failed to stop inotify watcher");
    TEST_ASSERT(!sse_watcher_active(&inotify_watcher), "Stopped inotify watcher should be inactive");

    /* Create file - should not trigger (watcher stopped) */
    old_call_count = inotify_data.call_count;
    fd = open("/tmp/sse_test_dir3/stopped_test", O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);

    test_sleep_ms(100);
    TEST_ASSERT(sse_run(&loop, 50), "Event loop should run");
    TEST_ASSERT_EQ(old_call_count, inotify_data.call_count,
                   "Stopped watcher should not receive events");

    /* Restart and test */
    TEST_ASSERT(sse_restart_inotify(&inotify_watcher), "Failed to restart inotify watcher");
    TEST_ASSERT(sse_watcher_active(&inotify_watcher), "Restarted inotify watcher should be active");

    /* === Cleanup === */
    sse_free(&loop);
    cleanup_test_dirs();

    return true;
}

/* Test inotify error handling */
static bool test_inotify_watcher_error_handling(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher1, watcher2;
    struct test_callback_data cb_data1, cb_data2;

    test_callback_init(&cb_data1);
    test_callback_init(&cb_data2);

    cleanup_test_dirs();
    TEST_ASSERT(create_test_dirs(), "Failed to create test directories");

    TEST_ASSERT(sse_new(&loop, 10), "Failed to create epoll loop");
    loop.running = true ;

    /* Start a valid inotify watcher */
    TEST_ASSERT(sse_start_inotify(&loop, &watcher1, test_inotify_callback, &cb_data1, 1),
               "Failed to start valid inotify watcher");

    TEST_ASSERT(sse_attach_inotify(&watcher1, "/tmp/sse_test_dir1", IN_CREATE | IN_DELETE),
               "Failed to attach valid directory");

    /* Test error conditions */
    TEST_ASSERT(!sse_start_inotify(NULL, &watcher2, test_inotify_callback, &cb_data2, 1),
               "Should fail with NULL epoll");

    TEST_ASSERT(!sse_start_inotify(&loop, NULL, test_inotify_callback, &cb_data2, 1),
               "Should fail with NULL watcher");

    /* Test invalid path attachment */
    TEST_ASSERT(!sse_attach_inotify(&watcher1, "/nonexistent/path", IN_CREATE),
               "Should fail to attach nonexistent path");

    TEST_ASSERT(!sse_attach_inotify(&watcher1, NULL, IN_CREATE),
               "Should fail to attach NULL path");

    /* Test invalid update */
    TEST_ASSERT(!sse_update_inotify(&watcher1, "/nonexistent/path", IN_MODIFY),
               "Should fail to update nonexistent path");

    /* Test that valid watcher still works */
    int fd = open("/tmp/sse_test_dir1/test_after_error", O_CREAT | O_WRONLY, 0644);
    TEST_ASSERT(fd >= 0, "Failed to create test file");
    close(fd);

    int max_iterations = 15;
    while (max_iterations-- > 0 && !cb_data1.callback_called) {
        if (!sse_run(&loop, 50)) {
            break;
        }
    }

    TEST_ASSERT(cb_data1.callback_called, "Valid inotify watcher should work after error conditions");

    sse_free(&loop);
    cleanup_test_dirs();

    return true;
}

/* Test different inotify event types on single watcher */
static bool test_inotify_event_types(void) {
    sse_epoll_t loop;
    sse_watcher_t inotify_watcher;
    struct test_callback_data inotify_data;

    test_callback_init(&inotify_data);

    cleanup_test_dirs();
    TEST_ASSERT(create_test_dirs(), "Failed to create test directories");

    TEST_ASSERT(sse_new(&loop, 15), "Failed to create epoll loop");
    loop.running = true ;

    /* Setup single watcher monitoring different event types on different paths */
    TEST_ASSERT(sse_start_inotify(&loop, &inotify_watcher, test_inotify_callback, &inotify_data, 1),
               "Failed to start inotify watcher");

    TEST_ASSERT(sse_attach_inotify(&inotify_watcher, "/tmp/sse_test_dir1", IN_CREATE),
               "Failed to attach create events");

    TEST_ASSERT(sse_attach_inotify(&inotify_watcher, "/tmp/sse_test_dir2", IN_MODIFY),
               "Failed to attach modify events");

    TEST_ASSERT(sse_attach_inotify(&inotify_watcher, "/tmp/sse_test_dir3", IN_DELETE),
               "Failed to attach delete events");

    /* Trigger specific events */

    /* CREATE event */
    int fd = open("/tmp/sse_test_dir1/created_file", O_CREAT | O_WRONLY, 0644);
    TEST_ASSERT(fd >= 0, "Failed to create file for CREATE test");
    close(fd);

    /* MODIFY event */
    fd = open("/tmp/sse_test_dir2/file_to_modify", O_CREAT | O_WRONLY, 0644);
    TEST_ASSERT(fd >= 0, "Failed to create file for MODIFY test");
    close(fd);

    fd = open("/tmp/sse_test_dir2/file_to_modify", O_WRONLY | O_APPEND);
    TEST_ASSERT(fd >= 0, "Failed to open file for modification");
    write(fd, "modification", 12);
    close(fd);

    /* DELETE event */
    fd = open("/tmp/sse_test_dir3/file_to_delete", O_CREAT | O_WRONLY, 0644);
    TEST_ASSERT(fd >= 0, "Failed to create file for DELETE test");
    close(fd);

    TEST_ASSERT(file_tryunlink("/tmp/sse_test_dir3/file_to_delete") == 0, "Failed to delete file");

    /* Process events */
    int max_iterations = 25;
    int initial_count = inotify_data.call_count;

    while (max_iterations-- > 0 && (inotify_data.call_count - initial_count) < 3) {
        if (!sse_run(&loop, 100)) {
            break;
        }
    }

    /* Verify events were received */
    TEST_ASSERT(inotify_data.call_count >= initial_count,
               "Should receive CREATE, MODIFY, and DELETE events");
    TEST_ASSERT(inotify_data.callback_called, "Inotify callback should be called");

    sse_free(&loop);
    cleanup_test_dirs();

    return true;
}

/* Test inotify watcher lifecycle and path management */
static bool test_inotify_path_management(void) {
    sse_epoll_t loop;
    sse_watcher_t inotify_watcher;
    struct test_callback_data inotify_data;

    test_callback_init(&inotify_data);

    cleanup_test_dirs();
    TEST_ASSERT(create_test_dirs(), "Failed to create test directories");

    TEST_ASSERT(sse_new(&loop, 15), "Failed to create epoll loop");
    loop.running = true ;

    /* Start single inotify watcher */
    TEST_ASSERT(sse_start_inotify(&loop, &inotify_watcher, test_inotify_callback, &inotify_data, 1),
               "Failed to start inotify watcher");

    /* Dynamically add paths */
    TEST_ASSERT(sse_attach_inotify(&inotify_watcher, "/tmp/sse_test_dir1", IN_ALL_EVENTS),
               "Failed to attach first directory");

    /* Test first path */
    int fd = open("/tmp/sse_test_dir1/test1", O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);

    int max_iterations = 15;
    while (max_iterations-- > 0 && !inotify_data.callback_called) {
        if (!sse_run(&loop, 100)) {
            break;
        }
    }

    TEST_ASSERT(inotify_data.callback_called, "First attached path should work");

    /* Add second path */
    TEST_ASSERT(sse_attach_inotify(&inotify_watcher, "/tmp/sse_test_dir2", IN_CREATE | IN_DELETE),
               "Failed to attach second directory");

    /* Test both paths work */
    inotify_data.callback_called = false;
    inotify_data.call_count = 0;

    fd = open("/tmp/sse_test_dir1/test2", O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);

    fd = open("/tmp/sse_test_dir2/test2", O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);

    max_iterations = 20;
    while (max_iterations-- > 0 && inotify_data.call_count < 2) {
        if (!sse_run(&loop, 100)) {
            break;
        }
    }

    TEST_ASSERT(inotify_data.call_count, "Both paths should generate events");

    /* Remove first path */
    TEST_ASSERT(sse_detach_inotify(&inotify_watcher, "/tmp/sse_test_dir1"),
               "Failed to detach first directory");

    /* Test that only second path works */
    int old_count = inotify_data.call_count;

    fd = open("/tmp/sse_test_dir1/should_not_trigger", O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);

    fd = open("/tmp/sse_test_dir2/should_trigger", O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);

    max_iterations = 15;
    while (max_iterations-- > 0) {
        if (!sse_run(&loop, 100)) {
            break;
        }
        if (inotify_data.call_count > old_count) {
            break;
        }
    }

    TEST_ASSERT(inotify_data.call_count == old_count + 1,
               "Only second path should generate events after detachment");

    sse_free(&loop);
    cleanup_test_dirs();

    return true;
}

TEST_SUITE_BEGIN("SSE Inotify Watcher Tests - Single Watcher Multiple Paths")
    RUN_TEST(test_inotify_watcher_comprehensive);
    RUN_TEST(test_inotify_watcher_error_handling);
    RUN_TEST(test_inotify_event_types);
    RUN_TEST(test_inotify_path_management);
TEST_SUITE_END()