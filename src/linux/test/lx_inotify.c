/*
 * test_lx_inotify.c
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/inotify.h>
#include <stdbool.h>

#include <oblibs/linux.h>
#include <oblibs/hash2.h>
#include <oblibs/io.h>
#include <oblibs/files.h>

// Helper: Create a temporary file
static const char *create_temp_file(void) {
    static const char *template = "/tmp/lx_inotify_test_XXXXXX";
    char *tmp = strdup(template);
    if (!tmp) {
        perror("strdup");
        return NULL;
    }
    int fd = mkstemp(tmp);
    if (fd < 0) {
        perror("mkstemp");
        free(tmp);
        return NULL;
    }
    close(fd);
    return tmp;
}

// Helper: Create a temporary directory
static const char *create_temp_dir(void) {
    static const char *template = "/tmp/lx_inotify_dir_XXXXXX";
    char *tmp = strdup(template);
    if (!tmp) {
        perror("strdup");
        return NULL;
    }
    if (mkdtemp(tmp) == NULL) {
        perror("mkdtemp");
        free(tmp);
        return NULL;
    }
    return tmp;
}

// Test lx_inotify_init
void test_lx_inotify_init(void) {
    printf("Testing lx_inotify_init...\n");
    int fd = lx_inotify_init();
    assert(fd >= 0 && "Failed to initialize inotify");
    assert(lx_inotify_get_fd() == fd && "lx_inotify_get_fd does not match initialized fd");
    // Test reinitialization
    int fd2 = lx_inotify_init();
    assert(fd2 == fd && "Reinitialization returned different fd");
    lx_inotify_end();
}

// Test lx_inotify_get_fd
void test_lx_inotify_get_fd(void) {
    printf("Testing lx_inotify_get_fd...\n");
    int fd = lx_inotify_init();
    assert(fd >= 0 && "Failed to initialize inotify for get_fd test");
    assert(lx_inotify_get_fd() == fd && "lx_inotify_get_fd returned incorrect fd");
    lx_inotify_end();
    assert(lx_inotify_get_fd() == -1 && "lx_inotify_get_fd should return -1 after end");
}

// Test lx_inotify_get_buffer
void test_lx_inotify_get_buffer(void) {
    printf("Testing lx_inotify_get_buffer...\n");
    int fd = lx_inotify_init();
    assert(fd >= 0 && "Failed to initialize inotify for get_buffer test");
    char *buf = lx_inotify_get_buffer();
    assert(buf != NULL && "lx_inotify_get_buffer returned NULL");
    lx_inotify_end();
}

// Test lx_inotify_get_blength
void test_lx_inotify_get_blength(void) {
    printf("Testing lx_inotify_get_blength...\n");
    int fd = lx_inotify_init();
    assert(fd >= 0 && "Failed to initialize inotify for get_blength test");
    assert(lx_inotify_get_blength() == 0 && "Initial blen should be 0");
    lx_inotify_end();
}

// Test lx_inotify_add
void test_lx_inotify_add(void) {
    printf("Testing lx_inotify_add...\n");
    const char *path = create_temp_file();
    assert(path != NULL && "Failed to create temp file for add test");

    int fd = lx_inotify_init();
    assert(fd >= 0 && "Failed to initialize inotify for add test");

    // Add watch
    int wd = lx_inotify_add(path, IN_MODIFY | IN_CREATE);
    assert(wd > 0 && "Failed to add inotify watch");

    // Verify watch in both hash tables
    struct lx_inotify_wd_s *w = lx_inotify_get_wd(wd);
    assert(w != NULL && "Failed to retrieve watch descriptor");
    assert(w->wd == wd && "Watch descriptor mismatch");
    assert(strcmp(w->path, path) == 0 && "Watch path mismatch");

    // Verify path-based lookup
    struct lx_inotify_wd_s *w_path;
    w_path = hash_find(&iwd, path, strlen(path));
    assert(w_path == w && "Path-based hash table lookup failed");

    // Test adding same path again
    int wd2 = lx_inotify_add(path, IN_MODIFY | IN_CREATE);
    assert(wd2 == wd && "Adding same path should return same wd");

    // Test invalid path
    errno = 0;
    int wd_invalid = lx_inotify_add("/nonexistent/path", IN_MODIFY);
    assert(wd_invalid == 0 && errno == ENOENT && "Expected ENOENT for nonexistent path");

    // Test with uninitialized fd
    lx_inotify_end();
    errno = 0;
    int wd_uninit = lx_inotify_add(path, IN_MODIFY);
    assert(wd_uninit == 0 && errno == EBADF && "Expected EBADF with uninitialized fd");

    file_tryunlink(path);
    free((void *)path);
}

// Test lx_inotify_update
void test_lx_inotify_update(void) {
    printf("Testing lx_inotify_update...\n");
    const char *path = create_temp_file();
    assert(path != NULL && "Failed to create temp file for update test");

    int fd = lx_inotify_init();
    assert(fd >= 0 && "Failed to initialize inotify for update test");

    // Add watch
    int wd = lx_inotify_add(path, IN_MODIFY);
    assert(wd > 0 && "Failed to add inotify watch for update test");

    // Update watch mask
    int wd_updated = lx_inotify_update(path, IN_MODIFY | IN_DELETE);
    assert(wd_updated > 0 && "Failed to update watch mask");

    // Verify watch in both hash tables
    struct lx_inotify_wd_s *w = lx_inotify_get_wd(wd_updated);
    assert(w != NULL && "Failed to retrieve updated watch descriptor");
    assert(w->wd == wd_updated && "Updated watch descriptor mismatch");
    assert(strcmp(w->path, path) == 0 && "Watch path mismatch after update");

    // Test update with non-existent path
    errno = 0;
    int wd_nonexistent = lx_inotify_update("/nonexistent/path", IN_MODIFY);
    assert(wd_nonexistent == 0 && errno == ENOENT && "Expected ENOENT for non-existent path");

    // Test update with uninitialized fd
    lx_inotify_end();
    errno = 0;
    int wd_uninit = lx_inotify_update(path, IN_MODIFY);
    assert(wd_uninit == 0 && errno == EBADF && "Expected EBADF with uninitialized fd");

    file_tryunlink(path);
    free((void *)path);
}

// Test lx_inotify_del
void test_lx_inotify_del(void) {
    printf("Testing lx_inotify_del...\n");
    const char *path = create_temp_file();
    assert(path != NULL && "Failed to create temp file for del test");

    int fd = lx_inotify_init();
    assert(fd >= 0 && "Failed to initialize inotify for del test");

    int wd = lx_inotify_add(path, IN_MODIFY);
    assert(wd > 0 && "Failed to add inotify watch for del test");

    // Delete watch
    assert(lx_inotify_del(path) == 1 && "Failed to delete watch");

    // Verify hash tables are empty
    struct lx_inotify_wd_s *w = NULL ;
    assert(lx_inotify_search(&w, path) == 0 && "Watch should not exist after deletion");
    struct lx_inotify_wd_s *w_wd;
    w_wd = hash_find(&iwd_by_wd, &wd, sizeof(wd));
    assert(w_wd == NULL && "wd-based hash table should be empty after deletion");

    // Test deleting non-existent watch
    assert(lx_inotify_del(path) == 1 && "Deleting non-existent watch should return 1");

    // Test with invalid fd
    lx_inotify_end();
    errno = 0;
    assert(lx_inotify_del(path) == 0 && errno == EBADF && "Expected EBADF with invalid fd");

    file_tryunlink(path);
    free((void *)path);
}

// Test lx_inotify_read and lx_inotify_iterator
void test_lx_inotify_read_iterator(void) {
    printf("Testing lx_inotify_read and lx_inotify_iterator...\n");
    const char *path = create_temp_file();
    assert(path != NULL && "Failed to create temp file for read/iterator test");

    int fd = lx_inotify_init();
    assert(fd >= 0 && "Failed to initialize inotify for read test");

    int wd = lx_inotify_add(path, IN_MODIFY);
    assert(wd > 0 && "Failed to add inotify watch for read test");

    // Trigger an event
    int file_fd = open(path, O_WRONLY);
    assert(file_fd >= 0 && "Failed to open temp file");
    assert(write(file_fd, "test", 4) == 4 && "Failed to write to temp file");
    close(file_fd);

    // Read events with retry
    int ret = 0;
    int retries = 5;
    bool found_event = false;
    while (retries-- > 0 && !found_event) {
        ret = lx_inotify_read();
        assert(ret >= 0 && "lx_inotify_read failed or returned error");

        // Iterate over events
        FOREACH_INOTIFY() {
            if (event->mask & IN_IGNORED) {
                continue;
            }
            assert(event->wd == wd && "Event watch descriptor mismatch");
            assert(event->mask & IN_MODIFY && "Expected IN_MODIFY event");
            assert(!event->len && "DEBUG: Unexpected non-empty event->name for file watch");

            found_event = true;
        }
        if (!found_event)
            usleep(100000); // 100ms delay

    }

    assert(found_event && "No valid events found");

    // Test io_read_result with EAGAIN
    errno = EAGAIN;
    assert(io_read_result(-1) == 0 && errno == 0 && "io_read_result failed for EAGAIN");

    // Test io_read_result with EPIPE
    assert(io_read_result(0) == -1 && errno == EPIPE && "io_read_result failed for EPIPE");

    file_tryunlink(path);
    free((void *)path);
}

// Test lx_inotify_read and lx_inotify_iterator for directory
void test_lx_inotify_read_iterator_dir(void) {
    printf("Testing lx_inotify_read and lx_inotify_iterator for directory...\n");
    // Ensure clean state
    lx_inotify_end();

    const char *dir_path = create_temp_dir();
    assert(dir_path != NULL && "Failed to create temp directory for read/iterator test");

    // Create a file in the directory
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/testfile", dir_path);
    int file_fd = open(file_path, O_WRONLY | O_CREAT, 0644);
    assert(file_fd >= 0 && "Failed to create test file in directory");
    close(file_fd);

    int fd = lx_inotify_init();
    assert(fd >= 0 && "Failed to initialize inotify for directory read test");

    int wd = lx_inotify_add(dir_path, IN_MODIFY);
    assert(wd > 0 && "Failed to add inotify watch for directory");

    // Trigger an event by modifying the file
    file_fd = open(file_path, O_WRONLY);
    assert(file_fd >= 0 && "Failed to open test file for modification");
    assert(write(file_fd, "test", 4) == 4 && "Failed to write to test file");
    close(file_fd);

    // Read events with retry
    int ret = 0;
    int retries = 5;
    bool found_event = false;
    while (retries-- > 0 && !found_event) {
        ret = lx_inotify_read();
        assert(ret >= 0 && "lx_inotify_read failed or returned error");

        // Iterate over events
        FOREACH_INOTIFY() {
            if (event->mask & IN_IGNORED)
                continue;
            assert(event->wd == wd && "Event watch descriptor mismatch");
            assert(event->mask & IN_MODIFY && "Expected IN_MODIFY event");
            assert(event->len > 0 && "Expected non-empty event->name for directory watch");
            assert(strcmp(event->name, "testfile") == 0 && "Event name mismatch");
            found_event = true;
        }
        if (!found_event)
            usleep(100000); // 100ms delay
    }
    assert(found_event && "No valid events found");

    // Test io_read_result with EAGAIN
    errno = EAGAIN;
    assert(io_read_result(-1) == 0 && errno == 0 && "io_read_result failed for EAGAIN");

    // Test io_read_result with EPIPE
    assert(io_read_result(0) == -1 && errno == EPIPE && "io_read_result failed for EPIPE");

    // Cleanup
    file_tryunlink(file_path);
    rmdir(dir_path);
    free((void *)dir_path);
}

// Test lx_inotify_end
void test_lx_inotify_end(void) {
    printf("Testing lx_inotify_end...\n");
    const char *path = create_temp_file();
    assert(path != NULL && "Failed to create temp file for end test");

    int fd = lx_inotify_init();
    assert(fd >= 0 && "Failed to initialize inotify for end test");

    int wd = lx_inotify_add(path, IN_MODIFY);
    assert(wd > 0 && "Failed to add inotify watch for end test");

    lx_inotify_end();
    assert(lx_inotify_get_fd() == -1 && "fd should be -1 after end");
    assert(lx_inotify_get_blength() == 0 && "blen should be 0 after end");
    assert(lx_inotify_get_wd(wd) == NULL && "Watch should be cleared after end");

    // Verify hash tables are empty
    assert(iwd.buckets == NULL && "Path-based hash table should be torn down after end");
    assert(iwd_by_wd.buckets == NULL && "wd-based hash table should be torn down after end");

    file_tryunlink(path);
    free((void *)path);
}

// Main test runner
int main(void) {
    test_lx_inotify_init();
    test_lx_inotify_get_fd();
    test_lx_inotify_get_buffer();
    test_lx_inotify_get_blength();
    test_lx_inotify_add();
    test_lx_inotify_update();
    test_lx_inotify_del();
    test_lx_inotify_read_iterator();
    test_lx_inotify_read_iterator_dir();
    test_lx_inotify_end();
    printf("All tests passed successfully!\n");
    return 0;
}