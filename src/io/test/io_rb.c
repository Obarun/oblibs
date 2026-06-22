/*
 * io_rb.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <fcntl.h>

#include <oblibs/io_rb.h>
#include <oblibs/rbuffer.h>

// Test framework macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            printf("      Line %d: %s\n", __LINE__, #condition); \
            if (errno) printf("      errno: %d (%s)\n", errno, strerror(errno)); \
            exit(1); \
        } \
    } while(0)

#define TEST_ASSERT_ERRNO(condition, expected_errno, message) \
    do { \
        errno = 0; \
        int result = (condition); \
        if (!result) { \
            TEST_ASSERT(errno == expected_errno, message); \
        } \
    } while(0)

#define TEST_START(name) \
    printf("Running %s...", name)

#define TEST_END() \
    printf(" PASS\n")

// Helper function to create a socketpair
static int create_socketpair(int fds[2]) {
    return socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
}

// Helper function to make fd non-blocking
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Test: io_rb_init with valid parameters
void test_init_valid() {
    TEST_START("test_init_valid");

    io_rb_t io;
    int result = io_rb_init(&io, NULL, 5);

    TEST_ASSERT(result == 1, "init should succeed");
    TEST_ASSERT(io.fd == 5, "fd should be set");
    TEST_ASSERT(io.ops == NULL, "ops should be NULL");
    TEST_ASSERT(io.want_flush == false, "want_flush should be false");
    TEST_ASSERT(io.refcount == 1, "refcount should be 1");
    TEST_ASSERT(io_rb_pending_in(&io) == 0, "input buffer should be empty");
    TEST_ASSERT(io_rb_pending_out(&io) == 0, "output buffer should be empty");

    TEST_END();
}

// Test: io_rb_init with NULL pointer
void test_init_null() {
    TEST_START("test_init_null");

    int result = io_rb_init(NULL, NULL, 5);

    TEST_ASSERT(result == 0, "init with NULL should fail");
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL");

    TEST_END();
}

// Test: io_rb_put basic functionality
void test_put_basic() {
    TEST_START("test_put_basic");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    const char *data = "Hello, World!";
    int result = io_rb_put(&io, data, strlen(data), NULL);

    TEST_ASSERT(result == 0, "put should succeed");
    TEST_ASSERT(io.want_flush == true, "want_flush should be true");
    TEST_ASSERT(io_rb_pending_out(&io) == strlen(data), "output buffer should contain data");

    TEST_END();
}

// Test: io_rb_put with NULL data
void test_put_null_data() {
    TEST_START("test_put_null_data");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    int result = io_rb_put(&io, NULL, 10, NULL);

    TEST_ASSERT(result == -1, "put with NULL data should fail");
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL");

    TEST_END();
}

// Test: io_rb_putv basic functionality
void test_putv_basic() {
    TEST_START("test_putv_basic");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    const char *data1 = "Hello";
    const char *data2 = ", World!";
    struct iovec iov[2];
    iov[0].iov_base = (void *)data1;
    iov[0].iov_len = strlen(data1);
    iov[1].iov_base = (void *)data2;
    iov[1].iov_len = strlen(data2);

    int result = io_rb_putv(&io, iov, 2, NULL);

    TEST_ASSERT(result == 0, "putv should succeed");
    TEST_ASSERT(io.want_flush == true, "want_flush should be true");
    TEST_ASSERT(io_rb_pending_out(&io) == strlen(data1) + strlen(data2),
                "output buffer should contain all data");

    TEST_END();
}

// Test: io_rb_get basic functionality
void test_get_basic() {
    TEST_START("test_get_basic");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    // Manually add data to input buffer
    const char *data = "Test data";
    rbuffer_write(&io.in.buf, data, strlen(data));

    char buffer[20];
    ssize_t result = io_rb_get(&io, buffer, strlen(data));

    TEST_ASSERT(result == (ssize_t)strlen(data), "get should return correct size");
    TEST_ASSERT(memcmp(buffer, data, strlen(data)) == 0, "data should match");
    TEST_ASSERT(io_rb_pending_in(&io) == 0, "input buffer should be empty");

    TEST_END();
}

// Test: io_rb_get with insufficient data
void test_get_insufficient() {
    TEST_START("test_get_insufficient");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    // Add less data than requested
    const char *data = "Test";
    rbuffer_write(&io.in.buf, data, strlen(data));

    char buffer[20];
    ssize_t result = io_rb_get(&io, buffer, 10);

    TEST_ASSERT(result == -1, "get should fail with insufficient data");
    TEST_ASSERT(errno == EAGAIN, "errno should be EAGAIN");

    TEST_END();
}

// Test: io_rb_put_fd basic functionality
void test_put_fd_basic() {
    TEST_START("test_put_fd_basic");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    int test_fd = 42;
    int result = io_rb_put_fd(&io, test_fd);

    TEST_ASSERT(result == 0, "put_fd should succeed");
    TEST_ASSERT(io.want_flush == true, "want_flush should be true");
    TEST_ASSERT(io_rb_pending_fdout(&io) == sizeof(int), "fd buffer should contain one fd");

    TEST_END();
}

// Test: io_rb_get_fd basic functionality
void test_get_fd_basic() {
    TEST_START("test_get_fd_basic");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    // Manually add fd to input buffer
    int test_fd = 42;
    rbuffer_write(&io.fdin.buf, &test_fd, sizeof(int));

    int retrieved_fd;
    ssize_t result = io_rb_get_fd(&io, &retrieved_fd);

    TEST_ASSERT(result == sizeof(int), "get_fd should return sizeof(int)");
    TEST_ASSERT(retrieved_fd == test_fd, "retrieved fd should match");
    TEST_ASSERT(io_rb_pending_fdin(&io) == 0, "fd input buffer should be empty");

    TEST_END();
}

// Test: io_rb_putv_fd basic functionality
void test_putv_fd_basic() {
    TEST_START("test_putv_fd_basic");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    const char *data = "Test data";
    struct iovec iov[1];
    iov[0].iov_base = (void *)data;
    iov[0].iov_len = strlen(data);

    int fds[2] = {10, 20};
    int result = io_rb_putv_fd(&io, iov, 1, fds, 2, NULL);

    TEST_ASSERT(result == 0, "putv_fd should succeed");
    TEST_ASSERT(io.want_flush == true, "want_flush should be true");
    TEST_ASSERT(io_rb_pending_out(&io) == strlen(data), "data buffer should contain data");
    TEST_ASSERT(io_rb_pending_fdout(&io) == 2 * sizeof(int), "fd buffer should contain fds");

    TEST_END();
}

// Test: io_rb_putv_fd with no data, only fds
void test_putv_fd_no_data() {
    TEST_START("test_putv_fd_no_data");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    struct iovec iov[1];
    iov[0].iov_base = NULL;
    iov[0].iov_len = 0;

    int fds[1] = {10};
    int result = io_rb_putv_fd(&io, iov, 1, fds, 1, NULL);

    TEST_ASSERT(result == 0, "putv_fd with no data should succeed");
    TEST_ASSERT(io_rb_pending_out(&io) == 0, "data buffer should be empty");
    TEST_ASSERT(io_rb_pending_fdout(&io) == sizeof(int), "fd buffer should contain fd");

    TEST_END();
}

// Test: io_rb_restore functionality
void test_restore() {
    TEST_START("test_restore");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    const char *data = "Test data";   /* 9 bytes */
    rbuffer_write(&io.in.buf, data, strlen(data));

    // Read the first 4 bytes ("Test")
    char buffer[5];
    ssize_t g4 = io_rb_get(&io, buffer, 4);
    TEST_ASSERT(g4 == 4, "first get returns 4");
    TEST_ASSERT(memcmp(buffer, "Test", 4) == 0, "first get content is 'Test'");

    size_t remaining = io_rb_pending_in(&io);
    TEST_ASSERT(remaining == 5, "5 bytes remain after reading 4");

    // Restore 2 bytes: re-expose the last 2 consumed ("st"), back ahead of " data"
    io_rb_restore(&io, 2);

    TEST_ASSERT(io_rb_pending_in(&io) == remaining + 2, "buffer should have 2 more bytes");

    /* Content check: the re-exposed bytes must be exactly the last two read,
     * so reading the now-7 pending bytes yields "st data" -- not just a count. */
    char back[8];
    ssize_t g7 = io_rb_get(&io, back, 7);
    TEST_ASSERT(g7 == 7, "get of the restored 7 bytes");
    TEST_ASSERT(memcmp(back, "st data", 7) == 0, "restored bytes re-expose the exact content 'st data'");
    TEST_ASSERT(io_rb_pending_in(&io) == 0, "input drained after the full read");

    TEST_END();
}

// Test: io_rb_reserve_output
void test_reserve_output() {
    TEST_START("test_reserve_output");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    ssize_t reserved = io_rb_reserve_output(&io, 100);

    /* 100 fits well within the 8191-byte capacity, so the reserve is exact, not
     * just "positive": reserved == requested, and pending_out advances by it. */
    TEST_ASSERT(reserved == 100, "reserve returns exactly the requested count when it fits");
    TEST_ASSERT(io_rb_pending_out(&io) == 100, "reserved bytes are now pending in out");
    TEST_ASSERT(io.want_flush == true, "want_flush should be set");

    /* A reserve larger than the remaining space is capped at what is available
     * (capacity 8191 minus the 100 already reserved == 8091), never more. */
    ssize_t capped = io_rb_reserve_output(&io, (size_t)IO_RB_BUFFER_SIZE * 2);
    TEST_ASSERT(capped == (ssize_t)(rbuffer_capacity(&io.out.buf) - 100),
                "oversized reserve is capped at the remaining space");
    TEST_ASSERT(io_rb_pending_out(&io) == rbuffer_capacity(&io.out.buf),
                "out is now full to capacity after the capped reserve");

    TEST_END();
}

// Test: io_rb_ref and io_rb_unref
void test_ref_counting() {
    TEST_START("test_ref_counting");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    TEST_ASSERT(io.refcount == 1, "initial refcount should be 1");

    io_rb_ref(&io);
    TEST_ASSERT(io.refcount == 2, "refcount should be 2 after ref");

    io_rb_unref(&io);
    TEST_ASSERT(io.refcount == 1, "refcount should be 1 after unref");

    TEST_END();
}

// Test: io_rb_iovec_len
void test_iovec_len() {
    TEST_START("test_iovec_len");

    const char *data1 = "Hello";
    const char *data2 = " World";
    struct iovec iov[2];
    iov[0].iov_base = (void *)data1;
    iov[0].iov_len = strlen(data1);
    iov[1].iov_base = (void *)data2;
    iov[1].iov_len = strlen(data2);

    size_t total = io_rb_iovec_len(iov, 2);

    TEST_ASSERT(total == strlen(data1) + strlen(data2), "total length should be correct");

    TEST_END();
}

// Test: io_rb_read and io_rb_flush integration
void test_read_flush_integration() {
    TEST_START("test_read_flush_integration");

    int fds[2];
    TEST_ASSERT(create_socketpair(fds) == 0, "socketpair creation should succeed");
    TEST_ASSERT(set_nonblocking(fds[0]) == 0, "set_nonblocking should succeed");
    TEST_ASSERT(set_nonblocking(fds[1]) == 0, "set_nonblocking should succeed");

    io_rb_t io_writer, io_reader;
    io_rb_init(&io_writer, NULL, fds[0]);
    io_rb_init(&io_reader, NULL, fds[1]);

    // Write data
    const char *test_data = "Integration test data";
    int put_result = io_rb_put(&io_writer, test_data, strlen(test_data), NULL);
    TEST_ASSERT(put_result == 0, "put should succeed");

    // Flush data
    ssize_t flush_result = io_rb_flush(&io_writer, NULL);
    TEST_ASSERT(flush_result == (ssize_t)strlen(test_data), "flush should write all data");

    // Read data
    ssize_t read_result = io_rb_read(&io_reader, NULL);
    TEST_ASSERT(read_result == (ssize_t)strlen(test_data), "read should read all data");

    // Get data
    char buffer[50];
    ssize_t get_result = io_rb_get(&io_reader, buffer, strlen(test_data));
    TEST_ASSERT(get_result == (ssize_t)strlen(test_data), "get should retrieve all data");
    TEST_ASSERT(memcmp(buffer, test_data, strlen(test_data)) == 0, "data should match");

    close(fds[0]);
    close(fds[1]);

    TEST_END();
}

// Test: Buffer overflow handling
void test_buffer_overflow() {
    TEST_START("test_buffer_overflow");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    // Try to write more than buffer capacity
    char large_data[IO_RB_BUFFER_SIZE + 100];
    memset(large_data, 'A', sizeof(large_data));

    errno = 0;
    int result = io_rb_put(&io, large_data, sizeof(large_data), NULL);

    TEST_ASSERT(result == -1, "put with oversized data should fail");
    /* Deterministic EAGAIN, independent of fd: io_rb_put has no upfront capacity
     * check, so it sets want_flush and calls io_rb_flush. The out buffer is still
     * EMPTY at this point (data not written yet), so flush is a no-op returning 0
     * (no ops->write, fdout empty -> returns before touching the fd). Back in
     * io_rb_put, available (8191) is still < count (8292), so it returns EAGAIN.
     * The fd (5) is never used. */
    TEST_ASSERT(errno == EAGAIN, "errno should be exactly EAGAIN");

    /* io_rb_putv DOES have an upfront capacity check, so oversized data there
     * is rejected with EOVERFLOW before any flush attempt -- a distinct path. */
    io_rb_t io2;
    io_rb_init(&io2, NULL, 5);
    struct iovec ov;
    ov.iov_base = large_data;
    ov.iov_len = sizeof(large_data);
    errno = 0;
    int rv = io_rb_putv(&io2, &ov, 1, NULL);
    TEST_ASSERT(rv == -1, "putv with oversized data should fail");
    TEST_ASSERT(errno == EOVERFLOW, "putv oversized errno should be exactly EOVERFLOW");

    TEST_END();
}

// Test: Empty putv
void test_putv_empty() {
    TEST_START("test_putv_empty");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    struct iovec iov[1];
    iov[0].iov_base = NULL;
    iov[0].iov_len = 0;

    int result = io_rb_putv(&io, iov, 1, NULL);

    TEST_ASSERT(result == 0, "putv with empty iovec should succeed");
    TEST_ASSERT(io_rb_pending_out(&io) == 0, "no data should be queued");

    TEST_END();
}

// Test: Multiple puts before flush
void test_multiple_puts() {
    TEST_START("test_multiple_puts");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    const char *data1 = "First";
    const char *data2 = "Second";
    const char *data3 = "Third";

    TEST_ASSERT(io_rb_put(&io, data1, strlen(data1), NULL) == 0, "first put should succeed");
    TEST_ASSERT(io_rb_put(&io, data2, strlen(data2), NULL) == 0, "second put should succeed");
    TEST_ASSERT(io_rb_put(&io, data3, strlen(data3), NULL) == 0, "third put should succeed");

    size_t expected = strlen(data1) + strlen(data2) + strlen(data3);
    TEST_ASSERT(io_rb_pending_out(&io) == expected, "all data should be queued");

    TEST_END();
}

// Test: File descriptor passing integration
void test_fd_passing_integration() {
    TEST_START("test_fd_passing_integration");

    int fds[2];
    TEST_ASSERT(create_socketpair(fds) == 0, "socketpair creation should succeed");
    TEST_ASSERT(set_nonblocking(fds[0]) == 0, "set_nonblocking should succeed");
    TEST_ASSERT(set_nonblocking(fds[1]) == 0, "set_nonblocking should succeed");

    io_rb_t io_writer, io_reader;
    io_rb_init(&io_writer, NULL, fds[0]);
    io_rb_init(&io_reader, NULL, fds[1]);

    // Create a pipe to pass
    int pipe_fds[2];
    TEST_ASSERT(pipe(pipe_fds) == 0, "pipe creation should succeed");

    // Write data with fd
    const char *test_data = "Data with FD";
    struct iovec iov[1];
    iov[0].iov_base = (void *)test_data;
    iov[0].iov_len = strlen(test_data);

    int put_result = io_rb_putv_fd(&io_writer, iov, 1, &pipe_fds[0], 1, NULL);
    TEST_ASSERT(put_result == 0, "putv_fd should succeed");

    // Flush
    ssize_t flush_result = io_rb_flush(&io_writer, NULL);
    TEST_ASSERT(flush_result > 0, "flush should succeed");

    // Read
    ssize_t read_result = io_rb_read(&io_reader, NULL);
    TEST_ASSERT(read_result > 0, "read should succeed");

    // Get data
    char buffer[50];
    ssize_t get_result = io_rb_get(&io_reader, buffer, strlen(test_data));
    TEST_ASSERT(get_result == (ssize_t)strlen(test_data), "should get data");
    TEST_ASSERT(memcmp(buffer, test_data, strlen(test_data)) == 0, "data should match");

    // Get fd
    int received_fd;
    ssize_t get_fd_result = io_rb_get_fd(&io_reader, &received_fd);
    TEST_ASSERT(get_fd_result == sizeof(int), "should get fd");
    TEST_ASSERT(received_fd >= 0, "received fd should be valid");

    // Cleanup
    close(received_fd);
    close(pipe_fds[1]);
    close(fds[0]);
    close(fds[1]);

    TEST_END();
}

// Test: Two flushes on the same stream, first carrying an fd, second carrying none.
// Regression: the second flush used to emit phantom SCM_RIGHTS fds because fdout
// was advanced twice per flush (peek-consume in encode_cmsg + radvance in flush).
void test_fd_passing_double_flush() {
    TEST_START("test_fd_passing_double_flush");

    int fds[2];
    TEST_ASSERT(create_socketpair(fds) == 0, "socketpair creation should succeed");
    TEST_ASSERT(set_nonblocking(fds[0]) == 0, "set_nonblocking should succeed");
    TEST_ASSERT(set_nonblocking(fds[1]) == 0, "set_nonblocking should succeed");

    io_rb_t io_writer, io_reader;
    io_rb_init(&io_writer, NULL, fds[0]);
    io_rb_init(&io_reader, NULL, fds[1]);

    int pipe_fds[2];
    TEST_ASSERT(pipe(pipe_fds) == 0, "pipe creation should succeed");

    // --- First exchange: data + 1 fd ---
    const char *msg1 = "first";
    struct iovec iov1[1];
    iov1[0].iov_base = (void *)msg1;
    iov1[0].iov_len = strlen(msg1);

    TEST_ASSERT(io_rb_putv_fd(&io_writer, iov1, 1, &pipe_fds[0], 1, NULL) == 0, "putv_fd should succeed");
    TEST_ASSERT(io_rb_flush(&io_writer, NULL) > 0, "first flush should succeed");
    // fdout must be fully drained, exactly once.
    TEST_ASSERT(io_rb_pending_fdout(&io_writer) == 0, "fdout must be empty after first flush");

    TEST_ASSERT(io_rb_read(&io_reader, NULL) > 0, "first read should succeed");
    char buf1[16];
    TEST_ASSERT(io_rb_get(&io_reader, buf1, strlen(msg1)) == (ssize_t)strlen(msg1), "should get first data");
    TEST_ASSERT(memcmp(buf1, msg1, strlen(msg1)) == 0, "first data should match");

    int received_fd;
    TEST_ASSERT(io_rb_get_fd(&io_reader, &received_fd) == sizeof(int), "should get exactly one fd");
    TEST_ASSERT(received_fd >= 0, "received fd should be valid");
    TEST_ASSERT(io_rb_pending_fdin(&io_reader) == 0, "no extra fd should be buffered after first exchange");
    close(received_fd);

    // --- Second exchange: data, NO fd ---
    const char *msg2 = "second";
    struct iovec iov2[1];
    iov2[0].iov_base = (void *)msg2;
    iov2[0].iov_len = strlen(msg2);

    TEST_ASSERT(io_rb_putv(&io_writer, iov2, 1, NULL) == 0, "second putv should succeed");
    TEST_ASSERT(io_rb_pending_fdout(&io_writer) == 0, "fdout must still be empty before second flush");
    TEST_ASSERT(io_rb_flush(&io_writer, NULL) > 0, "second flush should succeed");

    TEST_ASSERT(io_rb_read(&io_reader, NULL) > 0, "second read should succeed");
    char buf2[16];
    TEST_ASSERT(io_rb_get(&io_reader, buf2, strlen(msg2)) == (ssize_t)strlen(msg2), "should get second data");
    TEST_ASSERT(memcmp(buf2, msg2, strlen(msg2)) == 0, "second data should match");

    // The core assertion: the second message must NOT carry any fd.
    TEST_ASSERT(io_rb_pending_fdin(&io_reader) == 0, "second message must carry no phantom fds");

    close(pipe_fds[1]);
    close(fds[0]);
    close(fds[1]);

    TEST_END();
}

// Test: enqueue guards reject going past IO_RB_MAX_FD pending fds.
void test_fd_enqueue_cap() {
    TEST_START("test_fd_enqueue_cap");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    // Fill fdout up to exactly IO_RB_MAX_FD with single put_fd calls.
    for (int i = 0 ; i < IO_RB_MAX_FD ; i++)
        TEST_ASSERT(io_rb_put_fd(&io, i) == 0, "put_fd within cap should succeed");

    TEST_ASSERT(io_rb_pending_fdout(&io) == IO_RB_MAX_FD * sizeof(int), "fdout should hold IO_RB_MAX_FD fds");

    // One more must be rejected.
    errno = 0;
    TEST_ASSERT(io_rb_put_fd(&io, 999) == -1, "put_fd past cap should fail");
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL on overflow of fd cap");

    // putv_fd with a batch larger than the cap must be rejected outright.
    io_rb_t io2;
    io_rb_init(&io2, NULL, 6);
    const char *d = "x";
    struct iovec iov[1];
    iov[0].iov_base = (void *)d;
    iov[0].iov_len = 1;
    int big[IO_RB_MAX_FD + 1];
    for (int i = 0 ; i < IO_RB_MAX_FD + 1 ; i++) big[i] = i;

    errno = 0;
    TEST_ASSERT(io_rb_putv_fd(&io2, iov, 1, big, IO_RB_MAX_FD + 1, NULL) == -1, "putv_fd over cap should fail");
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL on oversized fd batch");
    TEST_ASSERT(io_rb_pending_out(&io2) == 0, "no data should be queued on rejected batch");
    TEST_ASSERT(io_rb_pending_fdout(&io2) == 0, "no fd should be queued on rejected batch");

    TEST_END();
}

// Test: flushing fds with no payload to carry them fails with EINVAL instead of
// silently dropping them and busy-looping.
void test_flush_orphan_fd() {
    TEST_START("test_flush_orphan_fd");

    int fds[2];
    TEST_ASSERT(create_socketpair(fds) == 0, "socketpair creation should succeed");
    TEST_ASSERT(set_nonblocking(fds[0]) == 0, "set_nonblocking should succeed");

    io_rb_t io;
    io_rb_init(&io, NULL, fds[0]);

    int pipe_fds[2];
    TEST_ASSERT(pipe(pipe_fds) == 0, "pipe creation should succeed");

    // Queue a fd but no data, then flush: SCM_RIGHTS needs a data carrier.
    TEST_ASSERT(io_rb_put_fd(&io, pipe_fds[0]) == 0, "put_fd should succeed");
    TEST_ASSERT(io_rb_pending_out(&io) == 0, "no data should be queued");

    errno = 0;
    TEST_ASSERT(io_rb_flush(&io, NULL) == -1, "flush of orphan fd should fail");
    TEST_ASSERT(errno == EINVAL, "errno should be EINVAL for orphan fd flush");

    close(pipe_fds[0]);
    close(pipe_fds[1]);
    close(fds[0]);
    close(fds[1]);

    TEST_END();
}

// Test: Pending functions with NULL
void test_pending_null() {
    TEST_START("test_pending_null");

    TEST_ASSERT(io_rb_pending_in(NULL) == 0, "pending_in with NULL should return 0");
    TEST_ASSERT(io_rb_pending_out(NULL) == 0, "pending_out with NULL should return 0");
    TEST_ASSERT(io_rb_pending_fdin(NULL) == 0, "pending_fdin with NULL should return 0");
    TEST_ASSERT(io_rb_pending_fdout(NULL) == 0, "pending_fdout with NULL should return 0");

    TEST_END();
}

// Test: io_rb_read with invalid fd
void test_read_invalid_fd() {
    TEST_START("test_read_invalid_fd");

    io_rb_t io;
    io_rb_init(&io, NULL, -1);

    ssize_t result = io_rb_read(&io, NULL);

    TEST_ASSERT(result == -1, "read with invalid fd should fail");
    TEST_ASSERT(errno == EBADF, "errno should be EBADF");

    TEST_END();
}

// Helper: is this fd still open?
static int fd_is_open(int fd) {
    return fcntl(fd, F_GETFD) != -1;
}

// Test: io_rb_decode_cmsg overflow path -- fdin has no room for the incoming fds.
// The received fds MUST be closed (no leak) and the call MUST report EOVERFLOW.
// This is the ancillary overflow branch that real recvmsg traffic almost never
// exercises; we forge the msghdr to drive it deterministically.
void test_decode_overflow_closes_fds() {
    TEST_START("test_decode_overflow_closes_fds");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    // Saturate fdin so rbuffer_available(fdin) < sizeof(int): any incoming fd
    // overflows and must be closed instead of stored.
    size_t cap = rbuffer_capacity(&io.fdin.buf);
    char *junk = malloc(cap);
    TEST_ASSERT(junk != NULL, "alloc junk");
    memset(junk, 0, cap);
    rbuffer_write(&io.fdin.buf, junk, cap);
    free(junk);
    TEST_ASSERT(rbuffer_available(&io.fdin.buf) < sizeof(int), "fdin saturated");

    // Two real fds carried by a forged SCM_RIGHTS cmsg.
    int a = dup(2), b = dup(2);
    TEST_ASSERT(a >= 0 && b >= 0, "dup test fds");
    TEST_ASSERT(fd_is_open(a) && fd_is_open(b), "fds open before decode");

    char cbuf[CMSG_SPACE(2 * sizeof(int))];
    memset(cbuf, 0, sizeof(cbuf));
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_control = cbuf;
    msg.msg_controllen = sizeof(cbuf);
    struct cmsghdr *cm = CMSG_FIRSTHDR(&msg);
    cm->cmsg_level = SOL_SOCKET;
    cm->cmsg_type = SCM_RIGHTS;
    cm->cmsg_len = CMSG_LEN(2 * sizeof(int));
    int fds[2] = { a, b };
    memcpy(CMSG_DATA(cm), fds, sizeof(fds));
    msg.msg_controllen = cm->cmsg_len;

    errno = 0;
    int r = io_rb_decode_cmsg(&io, &msg);
    TEST_ASSERT(r == -1, "decode signals failure on overflow");
    TEST_ASSERT(errno == EOVERFLOW, "errno is exactly EOVERFLOW on overflow");
    // The crux: no fd leak. Both received fds must have been closed.
    TEST_ASSERT(!fd_is_open(a) && !fd_is_open(b), "overflowed fds are CLOSED (no leak)");
    TEST_ASSERT(rbuffer_used(&io.fdin.buf) == cap, "fdin unchanged; nothing stored");

    TEST_END();
}

// Test: io_rb_decode_cmsg corruption path -- cmsg_len whose fd region is not a
// whole number of ints. The identifiable fds are closed (no leak) and the entry
// is skipped; this is NOT flagged as overflow, so the call returns 0.
void test_decode_corrupt_closes_fds() {
    TEST_START("test_decode_corrupt_closes_fds");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);   // fdin empty -> ample room; corruption is the trigger

    int a = dup(2);
    TEST_ASSERT(a >= 0, "dup test fd");
    TEST_ASSERT(fd_is_open(a), "fd open before decode");

    // fd region = sizeof(int) + 1 bytes: fdbytes % sizeof(int) != 0 -> corrupt.
    size_t bad = sizeof(int) + 1;
    char cbuf[CMSG_SPACE(2 * sizeof(int))];
    memset(cbuf, 0, sizeof(cbuf));
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_control = cbuf;
    msg.msg_controllen = sizeof(cbuf);
    struct cmsghdr *cm = CMSG_FIRSTHDR(&msg);
    cm->cmsg_level = SOL_SOCKET;
    cm->cmsg_type = SCM_RIGHTS;
    cm->cmsg_len = CMSG_LEN(bad);
    memcpy(CMSG_DATA(cm), &a, sizeof(int));   // first 4 bytes = the real fd
    msg.msg_controllen = cm->cmsg_len;

    errno = 0;
    int r = io_rb_decode_cmsg(&io, &msg);
    TEST_ASSERT(r == 0, "corrupt cmsg is skipped, not an overflow error");
    // The fdbytes/sizeof(int) == 1 identifiable fd must be closed (no leak).
    TEST_ASSERT(!fd_is_open(a), "the identifiable corrupt fd is CLOSED (no leak)");
    TEST_ASSERT(io_rb_pending_fdin(&io) == 0, "nothing stored from a corrupt cmsg");

    TEST_END();
}

// Test: io_rb_close_fds drains a fd buffer and closes every fd in it.
void test_close_fds() {
    TEST_START("test_close_fds");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    int a = dup(2), b = dup(2), c = dup(2);
    TEST_ASSERT(a >= 0 && b >= 0 && c >= 0, "dup three fds");
    TEST_ASSERT(io_rb_put_fd(&io, a) == 0, "queue a");
    TEST_ASSERT(io_rb_put_fd(&io, b) == 0, "queue b");
    TEST_ASSERT(io_rb_put_fd(&io, c) == 0, "queue c");
    TEST_ASSERT(io_rb_pending_fdout(&io) == 3 * sizeof(int), "three fds queued");

    io_rb_close_fds(&io.fdout.buf);

    TEST_ASSERT(!fd_is_open(a) && !fd_is_open(b) && !fd_is_open(c), "all queued fds closed");
    TEST_ASSERT(rbuffer_used(&io.fdout.buf) == 0, "fd buffer drained");

    TEST_END();
}

// Test: ring wrap-around. A payload written after the ring's write pointer has
// advanced near the end forces rbuffer_riovec to return TWO regions; flush then
// goes out as a 2-iovec sendmsg. Verify byte-for-byte integrity end to end --
// the small ~20-byte payloads elsewhere never exercise the split-region path.
void test_ring_wraparound() {
    TEST_START("test_ring_wraparound");

    int fds[2];
    TEST_ASSERT(create_socketpair(fds) == 0, "socketpair");
    TEST_ASSERT(set_nonblocking(fds[0]) == 0, "nonblock writer");
    TEST_ASSERT(set_nonblocking(fds[1]) == 0, "nonblock reader");
    // Enlarge the socket buffers so a single flush of ~8 KB drains in one pass.
    int sz = 1 << 20;
    setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz));
    setsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz));

    io_rb_t w, r;
    io_rb_init(&w, NULL, fds[0]);
    io_rb_init(&r, NULL, fds[1]);

    // Deterministic pattern so any mis-ordered region is caught.
    char *blk = malloc(IO_RB_BUFFER_SIZE);
    TEST_ASSERT(blk != NULL, "alloc pattern");
    for (int i = 0 ; i < IO_RB_BUFFER_SIZE ; i++) blk[i] = (char)(i & 0xff);

    // Phase 1: push the out ring's r/w pointers to offset 6000, then drain, so
    // both sit near the end of the 8192-slot ring with the buffer empty.
    TEST_ASSERT(io_rb_put(&w, blk, 6000, NULL) == 0, "put 6000");
    TEST_ASSERT(io_rb_flush(&w, NULL) == 6000, "flush 6000");
    TEST_ASSERT(io_rb_read(&r, NULL) == 6000, "read 6000");
    char drain[6000];
    TEST_ASSERT(io_rb_get(&r, drain, 6000) == 6000, "drain 6000");
    TEST_ASSERT(io_rb_pending_out(&w) == 0, "out empty, pointers parked near end");

    // Phase 2: 4000 bytes now wrap past the ring boundary -> two iovec regions.
    TEST_ASSERT(io_rb_put(&w, blk, 4000, NULL) == 0, "put 4000 (wraps)");
    struct iovec iov[2];
    unsigned n = rbuffer_riovec(&w.out.buf, iov);
    TEST_ASSERT(n == 2, "wrapped payload yields a 2-region readable iovec");
    TEST_ASSERT(iov[0].iov_len + iov[1].iov_len == 4000, "two regions cover the whole payload");

    // Flush (2-iovec sendmsg) and read back; bytes must match exactly.
    TEST_ASSERT(io_rb_flush(&w, NULL) == 4000, "flush wrapped 4000 in one pass");
    TEST_ASSERT(io_rb_read(&r, NULL) == 4000, "read wrapped 4000");
    char out[4000];
    TEST_ASSERT(io_rb_get(&r, out, 4000) == 4000, "get wrapped 4000");
    TEST_ASSERT(memcmp(out, blk, 4000) == 0, "wrapped payload is byte-for-byte intact");

    free(blk);
    close(fds[0]);
    close(fds[1]);

    TEST_END();
}

// Test: io_rb_flush with empty buffer
void test_flush_empty() {
    TEST_START("test_flush_empty");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    ssize_t result = io_rb_flush(&io, NULL);

    TEST_ASSERT(result == 0, "flush with empty buffer should return 0");

    TEST_END();
}

int main(void) {
    printf("=== io_rb Test Suite ===\n\n");

    // Initialization tests
    test_init_valid();
    test_init_null();

    // Put tests
    test_put_basic();
    test_put_null_data();
    test_putv_basic();
    test_putv_empty();
    test_multiple_puts();

    // Get tests
    test_get_basic();
    test_get_insufficient();

    // File descriptor tests
    test_put_fd_basic();
    test_get_fd_basic();
    test_putv_fd_basic();
    test_putv_fd_no_data();
    test_fd_enqueue_cap();
    test_flush_orphan_fd();

    // Ancillary fd-handling edge cases (no-leak guarantees)
    test_decode_overflow_closes_fds();
    test_decode_corrupt_closes_fds();
    test_close_fds();

    // Utility function tests
    test_restore();
    test_reserve_output();
    test_ref_counting();
    test_iovec_len();
    test_pending_null();

    // Edge case tests
    test_buffer_overflow();
    test_read_invalid_fd();
    test_flush_empty();
    test_ring_wraparound();

    // Integration tests
    test_read_flush_integration();
    test_fd_passing_integration();
    test_fd_passing_double_flush();

    printf("\n=== All tests passed! ===\n");
    return 0;
}