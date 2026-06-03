/*
 * sse_stream.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <oblibs/sse_stream.h>
#include <oblibs/sse.h>

#define TEST_SOCKET_PATH "/tmp/test_sse_stream.sock"
#define TEST_BUFFER_SIZE 4096

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

// Test state tracking
static int read_cb_called = 0;
static int write_cb_called = 0;
static int close_cb_called = 0;
static int error_cb_called = 0;
static int last_error = 0;

// Callback implementations for testing
static void test_read_cb(sse_stream_t *stream, void *data) {
    read_cb_called++;
}

static void test_write_cb(sse_stream_t *stream, void *data) {
    write_cb_called++;
}

static void test_close_cb(sse_stream_t *stream, void *data) {
    close_cb_called++;
}

static void test_error_cb(sse_stream_t *stream, int error, void *data) {
    error_cb_called++;
    last_error = error;
}

static void reset_callbacks(void) {
    read_cb_called = 0;
    write_cb_called = 0;
    close_cb_called = 0;
    error_cb_called = 0;
    last_error = 0;
}

// Helper to create a socketpair for testing
static int create_test_socketpair(int fds[2]) {
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds) < 0) {
        perror("socketpair");
        return -1;
    }
    return 0;
}

// Test 1: Basic initialization with sse_stream_new (heap-allocated)
static void test_stream_new(void) {
    TEST_START("test_stream_new");

    int fds[2];
    TEST_ASSERT(create_test_socketpair(fds) == 0, "Failed to create socketpair");

    sse_stream_t *stream = sse_stream_new(fds[0], test_read_cb, test_write_cb,
                                        test_close_cb, test_error_cb, NULL);

    TEST_ASSERT(stream != NULL, "sse_stream_new returned NULL");
    TEST_ASSERT(stream->fd == fds[0], "Stream fd mismatch");
    TEST_ASSERT(stream->closed == false, "Stream should not be closed initially");
    TEST_ASSERT(stream->error == 0, "Stream should have no error initially");

    // Close stream (will free it)
    sse_stream_close(stream);
    close(fds[1]);

    TEST_END();
}

// Test 2: Invalid initialization parameters
static void test_stream_init_invalid(void) {
    TEST_START("test_stream_init_invalid");

    sse_stream_t stream;
    int result;

    // NULL stream pointer
    errno = 0;
    result = sse_stream_init(NULL, 3, test_read_cb, test_write_cb,
                            test_close_cb, test_error_cb, NULL);
    TEST_ASSERT(result == 0, "Init with NULL stream should fail");
    TEST_ASSERT(errno == EINVAL, "NULL stream should set EINVAL");

    // Invalid fd
    errno = 0;
    result = sse_stream_init(&stream, -1, test_read_cb, test_write_cb,
                            test_close_cb, test_error_cb, NULL);
    TEST_ASSERT(result == 0, "Init with invalid fd should fail");
    TEST_ASSERT(errno == EINVAL, "Invalid fd should set EINVAL");

    TEST_END();
}

// Test 3: Basic data put/get
static void test_stream_data_put_get(void) {
    TEST_START("test_stream_data_put_get");

    int fds[2];
    TEST_ASSERT(create_test_socketpair(fds) == 0, "Failed to create socketpair");

    sse_stream_t *stream1 = sse_stream_new(fds[0], NULL, NULL, NULL, NULL, NULL);
    sse_stream_t *stream2 = sse_stream_new(fds[1], NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT(stream1 != NULL && stream2 != NULL, "Failed to create streams");

    const char *test_data = "Hello, SSE Stream!";
    size_t data_len = strlen(test_data);

    // Put data
    int result = sse_stream_put(stream1, test_data, data_len);
    TEST_ASSERT(result == 0, "sse_stream_put failed");

    // Check pending output
    size_t pending = sse_stream_pending_out(stream1);
    TEST_ASSERT(pending == data_len, "Pending output size mismatch");

    // Flush to socket
    ssize_t flushed = sse_stream_flush(stream1);
    TEST_ASSERT(flushed > 0, "sse_stream_flush failed");

    // Read from other end
    ssize_t read_result = sse_stream_read(stream2);
    TEST_ASSERT(read_result > 0, "sse_stream_read failed");

    // Get data
    char buffer[256] = {0};
    ssize_t got = sse_stream_get(stream2, buffer, data_len);
    TEST_ASSERT(got == (ssize_t)data_len, "sse_stream_get returned wrong size");
    TEST_ASSERT(strcmp(buffer, test_data) == 0, "Data mismatch");

    sse_stream_close(stream1);
    sse_stream_close(stream2);

    TEST_END();
}

// Test 4: Vectored I/O (putv)
static void test_stream_putv(void) {
    TEST_START("test_stream_putv");

    int fds[2];
    TEST_ASSERT(create_test_socketpair(fds) == 0, "Failed to create socketpair");

    sse_stream_t *stream1 = sse_stream_new(fds[0], NULL, NULL, NULL, NULL, NULL);
    sse_stream_t *stream2 = sse_stream_new(fds[1], NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT(stream1 != NULL && stream2 != NULL, "Failed to create streams");

    const char *part1 = "Hello, ";
    const char *part2 = "World";
    const char *part3 = "!";

    struct iovec iov[3];
    iov[0].iov_base = (void*)part1;
    iov[0].iov_len = strlen(part1);
    iov[1].iov_base = (void*)part2;
    iov[1].iov_len = strlen(part2);
    iov[2].iov_base = (void*)part3;
    iov[2].iov_len = strlen(part3);

    int result = sse_stream_putv(stream1, iov, 3);
    TEST_ASSERT(result == 0, "sse_stream_putv failed");

    size_t total_len = strlen(part1) + strlen(part2) + strlen(part3);
    size_t pending = sse_stream_pending_out(stream1);
    TEST_ASSERT(pending == total_len, "Pending output size mismatch");

    ssize_t flushed = sse_stream_flush(stream1);
    TEST_ASSERT(flushed > 0, "Flush failed");

    // Read and verify
    ssize_t read_result = sse_stream_read(stream2);
    TEST_ASSERT(read_result > 0, "Read failed");

    char buffer[256] = {0};
    ssize_t got = sse_stream_get(stream2, buffer, total_len);
    TEST_ASSERT(got == (ssize_t)total_len, "Get returned wrong size");
    TEST_ASSERT(strcmp(buffer, "Hello, World!") == 0, "Data mismatch");

    sse_stream_close(stream1);
    sse_stream_close(stream2);

    TEST_END();
}

// Test 5: File descriptor passing
static void test_stream_fd_passing(void) {
    TEST_START("test_stream_fd_passing");

    int fds[2];
    TEST_ASSERT(create_test_socketpair(fds) == 0, "Failed to create socketpair");

    sse_stream_t *stream1 = sse_stream_new(fds[0], NULL, NULL, NULL, NULL, NULL);
    sse_stream_t *stream2 = sse_stream_new(fds[1], NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT(stream1 != NULL && stream2 != NULL, "Failed to create streams");

    // Create a pipe to pass
    int pipe_fds[2];
    TEST_ASSERT(pipe(pipe_fds) == 0, "Failed to create pipe");

    // Send write end of pipe
    int result = sse_stream_put_fd(stream1, pipe_fds[1]);
    TEST_ASSERT(result == 0, "sse_stream_put_fd failed");

    size_t pending_fds = sse_stream_pending_fdout(stream1);
    // pending_fdout returns bytes, not count: 1 FD = sizeof(int) bytes
    TEST_ASSERT(pending_fds == sizeof(int), "Pending FDs size mismatch");

    // Flush with dummy data (FDs need data to be sent)
    const char *dummy = "X";
    sse_stream_put(stream1, dummy, 1);
    ssize_t flushed = sse_stream_flush(stream1);
    TEST_ASSERT(flushed > 0, "Flush failed");

    // Receive on other end
    ssize_t read_result = sse_stream_read(stream2);
    TEST_ASSERT(read_result > 0, "Read failed");

    int received_fd = -1;
    ssize_t got_fd = sse_stream_get_fd(stream2, &received_fd);
    TEST_ASSERT(got_fd == sizeof(int), "sse_stream_get_fd failed");
    TEST_ASSERT(received_fd >= 0, "Received invalid fd");

    // Verify the FD works
    const char *test_msg = "FD passing works!";
    write(received_fd, test_msg, strlen(test_msg));

    char verify_buf[256] = {0};
    ssize_t n = read(pipe_fds[0], verify_buf, sizeof(verify_buf));
    TEST_ASSERT(n == (ssize_t)strlen(test_msg), "Read wrong amount from passed FD");
    TEST_ASSERT(strcmp(verify_buf, test_msg) == 0, "Data via passed FD mismatch");

    close(received_fd);
    close(pipe_fds[0]);
    sse_stream_close(stream1);
    sse_stream_close(stream2);

    TEST_END();
}

// Test 6: Vectored I/O with FD passing
static void test_stream_putv_fd(void) {
    TEST_START("test_stream_putv_fd");

    int fds[2];
    TEST_ASSERT(create_test_socketpair(fds) == 0, "Failed to create socketpair");

    sse_stream_t *stream1 = sse_stream_new(fds[0], NULL, NULL, NULL, NULL, NULL);
    sse_stream_t *stream2 = sse_stream_new(fds[1], NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT(stream1 != NULL && stream2 != NULL, "Failed to create streams");

    // Create pipes to pass
    int pipe1[2], pipe2[2];
    TEST_ASSERT(pipe(pipe1) == 0, "Failed to create pipe1");
    TEST_ASSERT(pipe(pipe2) == 0, "Failed to create pipe2");

    const char *msg1 = "Part1";
    const char *msg2 = "Part2";

    struct iovec iov[2];
    iov[0].iov_base = (void*)msg1;
    iov[0].iov_len = strlen(msg1);
    iov[1].iov_base = (void*)msg2;
    iov[1].iov_len = strlen(msg2);

    int send_fds[2] = {pipe1[1], pipe2[1]};

    int result = sse_stream_putv_fd(stream1, iov, 2, send_fds, 2);
    TEST_ASSERT(result == 0, "sse_stream_putv_fd failed");

    ssize_t flushed = sse_stream_flush(stream1);
    TEST_ASSERT(flushed > 0, "Flush failed");

    // Receive
    ssize_t read_result = sse_stream_read(stream2);
    TEST_ASSERT(read_result > 0, "Read failed");

    // Get data
    char buffer[256] = {0};
    size_t total_len = strlen(msg1) + strlen(msg2);
    ssize_t got = sse_stream_get(stream2, buffer, total_len);
    TEST_ASSERT(got == (ssize_t)total_len, "Get returned wrong size");

    // Get FDs
    int fd1 = -1, fd2 = -1;
    TEST_ASSERT(sse_stream_get_fd(stream2, &fd1) == sizeof(int), "Failed to get fd1");
    TEST_ASSERT(sse_stream_get_fd(stream2, &fd2) == sizeof(int), "Failed to get fd2");
    TEST_ASSERT(fd1 >= 0 && fd2 >= 0, "Received invalid fds");

    close(fd1);
    close(fd2);
    close(pipe1[0]);
    close(pipe2[0]);
    sse_stream_close(stream1);
    sse_stream_close(stream2);

    TEST_END();
}

// Test 7: Stream status queries
static void test_stream_status(void) {
    TEST_START("test_stream_status");

    int fds[2];
    TEST_ASSERT(create_test_socketpair(fds) == 0, "Failed to create socketpair");

    sse_stream_t *stream = sse_stream_new(fds[0], NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT(stream != NULL, "Failed to create stream");

    // Initially no pending data
    TEST_ASSERT(sse_stream_pending_in(stream) == 0, "Initial pending_in should be 0");
    TEST_ASSERT(sse_stream_pending_out(stream) == 0, "Initial pending_out should be 0");
    TEST_ASSERT(sse_stream_pending_fdin(stream) == 0, "Initial pending_fdin should be 0");
    TEST_ASSERT(sse_stream_pending_fdout(stream) == 0, "Initial pending_fdout should be 0");

    // Add some data
    const char *data = "Test data";
    sse_stream_put(stream, data, strlen(data));

    size_t pending_out = sse_stream_pending_out(stream);
    TEST_ASSERT(pending_out == strlen(data), "Pending output size mismatch");

    // Check closed status
    TEST_ASSERT(sse_stream_is_closed(stream) == false, "Stream should not be closed");
    TEST_ASSERT(sse_stream_get_error(stream) == 0, "Initial error should be 0");

    // Close and check
    sse_stream_close(stream);
    close(fds[1]);

    TEST_END();
}

// Test 8: Unix domain socket helpers - test creation only
static void test_streamux_helpers(void) {
    TEST_START("test_streamux_helpers");

    unlink(TEST_SOCKET_PATH);

    // Test 1: Create server
    int server_fd = sse_streamux_create_server(TEST_SOCKET_PATH, 5);
    TEST_ASSERT(server_fd >= 0, "Failed to create server socket");

    // Verify socket exists
    struct stat st;
    TEST_ASSERT(stat(TEST_SOCKET_PATH, &st) == 0, "Socket file not created");
    TEST_ASSERT(S_ISSOCK(st.st_mode), "Not a socket file");

    close(server_fd);
    unlink(TEST_SOCKET_PATH);

    // Test 2: Test invalid parameters
    errno = 0;
    int invalid_fd = sse_streamux_create_server(NULL, 5);
    TEST_ASSERT(invalid_fd < 0, "Should fail with NULL path");
    TEST_ASSERT(errno == EINVAL, "Should set EINVAL for NULL path");

    errno = 0;
    invalid_fd = sse_streamux_create_client(NULL);
    TEST_ASSERT(invalid_fd < 0, "Should fail with NULL path");
    TEST_ASSERT(errno == EINVAL, "Should set EINVAL for NULL path");

    errno = 0;
    invalid_fd = sse_streamux_accept(-1);
    TEST_ASSERT(invalid_fd < 0, "Should fail with invalid fd");
    TEST_ASSERT(errno == EINVAL, "Should set EINVAL for invalid fd");

    TEST_END();
}

// Test 9: Error handling
static void test_stream_errors(void) {
    TEST_START("test_stream_errors");

    int fds[2];
    TEST_ASSERT(create_test_socketpair(fds) == 0, "Failed to create socketpair");

    sse_stream_t *stream = sse_stream_new(fds[0], NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT(stream != NULL, "Failed to create stream");

    // Test invalid operations on closed stream
    sse_stream_close(stream);

    // Note: After close, stream is freed, so we need a new one for error testing
    stream = sse_stream_new(fds[1], NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT(stream != NULL, "Failed to create second stream");

    // Close the peer to cause errors
    close(fds[0]);

    // Try to put on a stream whose peer is closed - should succeed locally
    errno = 0;
    int put_result = sse_stream_put(stream, "test", 4);
    // Put succeeds locally, flush would fail
    TEST_ASSERT(put_result == 0, "Put should succeed even if peer closed");

    sse_stream_close(stream);

    TEST_END();
}

// Test 10: Close FDs
static void test_stream_close_fds(void) {
    TEST_START("test_stream_close_fds");

    int fds[2];
    TEST_ASSERT(create_test_socketpair(fds) == 0, "Failed to create socketpair");

    sse_stream_t *stream = sse_stream_new(fds[0], NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT(stream != NULL, "Failed to create stream");

    // Queue some FDs
    int pipe1[2], pipe2[2];
    TEST_ASSERT(pipe(pipe1) == 0, "Failed to create pipe1");
    TEST_ASSERT(pipe(pipe2) == 0, "Failed to create pipe2");

    TEST_ASSERT(sse_stream_put_fd(stream, pipe1[1]) == 0, "Failed to queue fd1");
    TEST_ASSERT(sse_stream_put_fd(stream, pipe2[1]) == 0, "Failed to queue fd2");

    size_t pending = sse_stream_pending_fdout(stream);
    // pending_fdout returns bytes: 2 FDs = 2 * sizeof(int) bytes
    TEST_ASSERT(pending == 2 * sizeof(int), "Pending FD size mismatch");

    // Close all pending FDs
    sse_stream_close_fds(stream);

    close(pipe1[0]);
    close(pipe2[0]);
    sse_stream_close(stream);
    close(fds[1]);

    TEST_END();
}

// Test 11: Callbacks (if we can attach to an event loop)
static void test_stream_callbacks(void) {
    TEST_START("test_stream_callbacks");

    reset_callbacks();

    int fds[2];
    TEST_ASSERT(create_test_socketpair(fds) == 0, "Failed to create socketpair");

    sse_stream_t *stream = sse_stream_new(fds[0], test_read_cb, test_write_cb,
                                        test_close_cb, test_error_cb, NULL);
    TEST_ASSERT(stream != NULL, "Failed to create stream");

    // Note: Full callback testing requires an event loop (sse_epoll_t)
    // For now, just verify callbacks are set
    TEST_ASSERT(stream->read_cb == test_read_cb, "Read callback not set");
    TEST_ASSERT(stream->write_cb == test_write_cb, "Write callback not set");
    TEST_ASSERT(stream->close_cb == test_close_cb, "Close callback not set");
    TEST_ASSERT(stream->error_cb == test_error_cb, "Error callback not set");

    sse_stream_close(stream);
    close(fds[1]);

    TEST_END();
}

// Test 12: NULL stream handling (only safe operations)
static void test_stream_null_handling(void) {
    TEST_START("test_stream_null_handling");

    // These functions are documented to handle NULL gracefully
    TEST_ASSERT(sse_stream_is_closed(NULL) == true, "is_closed(NULL) should return true");
    TEST_ASSERT(sse_stream_get_error(NULL) == EINVAL, "get_error(NULL) should return EINVAL");

    // These should not crash (they check for NULL internally)
    sse_stream_close(NULL);
    sse_stream_close_fds(NULL);
    sse_stream_modify(NULL);

    // Note: The following functions will segfault with NULL because they need to
    // dereference stream->io. They should only be called with valid streams:
    // - sse_stream_pending_in(NULL)
    // - sse_stream_pending_out(NULL)
    // - sse_stream_pending_fdin(NULL)
    // - sse_stream_pending_fdout(NULL)
    // This is by design as they are performance-critical accessors

    TEST_END();
}

// Main test runner
int main(void) {

    printf("Starting sse_stream test suite...\n\n");
    test_stream_new();
    test_stream_init_invalid();
    test_stream_data_put_get();
    test_stream_putv();
    test_stream_fd_passing();
    test_stream_putv_fd();
    test_stream_status();
    test_streamux_helpers();
    test_stream_errors();
    test_stream_close_fds();
    test_stream_callbacks();
    test_stream_null_handling();
    printf("\nAll tests passed! ✓\n");

    return 0;
}