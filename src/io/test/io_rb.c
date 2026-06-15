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
 * except according to the terms contained in the LICENSE file./
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

    const char *data = "Test data";
    rbuffer_write(&io.in.buf, data, strlen(data));

    // Read some data
    char buffer[5];
    io_rb_get(&io, buffer, 4);

    size_t remaining = io_rb_pending_in(&io);

    // Restore 2 bytes
    io_rb_restore(&io, 2);

    TEST_ASSERT(io_rb_pending_in(&io) == remaining + 2, "buffer should have 2 more bytes");

    TEST_END();
}

// Test: io_rb_reserve_output
void test_reserve_output() {
    TEST_START("test_reserve_output");

    io_rb_t io;
    io_rb_init(&io, NULL, 5);

    ssize_t reserved = io_rb_reserve_output(&io, 100);

    TEST_ASSERT(reserved > 0, "reserve should return positive value");
    TEST_ASSERT(io.want_flush == true, "want_flush should be set");

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

    int result = io_rb_put(&io, large_data, sizeof(large_data), NULL);

    TEST_ASSERT(result == -1, "put with oversized data should fail");
    // When buffer is full, it tries to flush. Since fd=5 is likely invalid,
    // flush fails with EAGAIN (non-blocking would block) or other error
    TEST_ASSERT(errno == EAGAIN || errno == EOVERFLOW || errno == EBADF,
                "errno should be EAGAIN, EOVERFLOW, or EBADF");

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

    // Integration tests
    test_read_flush_integration();
    test_fd_passing_integration();
    test_fd_passing_double_flush();

    printf("\n=== All tests passed! ===\n");
    return 0;
}