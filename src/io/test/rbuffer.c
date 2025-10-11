#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/uio.h>

#include <oblibs/rbuffer.h>

// Test helper macros
#define TEST_START(name) \
    do { \
        printf("Testing %s...\n", name); \
        test_count++; \
    } while(0)

#define TEST_PASS(name) \
    do { \
        printf("✓ %s PASSED\n\n", name); \
        pass_count++; \
    } while(0)

// Global counters
static int test_count = 0;
static int pass_count = 0;

// Test basic initialization and state functions
void test_init_and_state(void) {
    TEST_START("rbuffer_init and state functions");
    uint8_t storage[10];
    rbuffer_t buf;
    // Test initialization
    rbuffer_init(&buf, storage, 10);
    assert(buf.b == storage);
    assert(buf.a == 10);
    assert(buf.r == 0);
    assert(buf.w == 0);
    // Test state functions on empty buffer
    assert(rbuffer_capacity(&buf) == 9);  // size - 1
    assert(rbuffer_used(&buf) == 0);
    assert(rbuffer_available(&buf) == 9);
    assert(rbuffer_isempty(&buf) == true);
    assert(rbuffer_isfull(&buf) == false);


    TEST_PASS("rbuffer_init and state functions");
}

// Test helper functions
void test_helper_functions(void) {
    TEST_START("helper functions");
    // Test rbuffer_wrap
    assert(rbuffer_wrap(5, 10) == 5);   // no wrap needed
    assert(rbuffer_wrap(10, 10) == 0);  // wrap to 0
    assert(rbuffer_wrap(15, 10) == 5);  // wrap around
    TEST_PASS("helper functions");
}

// Test basic write operations
void test_write_operations(void) {
    TEST_START("write operations");
    uint8_t storage[8];
    rbuffer_t buf;
    rbuffer_init(&buf, storage, 8);
    // Test single write
    uint8_t data1[] = {1, 2, 3};
    size_t written = rbuffer_write(&buf, data1, 3);
    assert(written == 3);
    assert(rbuffer_used(&buf) == 3);
    assert(rbuffer_available(&buf) == 4);
    // Test write that fills buffer
    uint8_t data2[] = {4, 5, 6, 7};
    written = rbuffer_write(&buf, data2, 4);
    assert(written == 4);
    assert(rbuffer_used(&buf) == 7);
    assert(rbuffer_available(&buf) == 0);
    assert(rbuffer_isfull(&buf) == true);
    // Test write to full buffer (should write 0 bytes)
    uint8_t data3[] = {8, 9};
    written = rbuffer_write(&buf, data3, 2);
    assert(written == 0);
    assert(rbuffer_used(&buf) == 7);
    // Test partial write
    rbuffer_init(&buf, storage, 8);
    uint8_t data4[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    written = rbuffer_write(&buf, data4, 10);
    assert(written == 7);  // capacity is 7
    assert(rbuffer_isfull(&buf) == true);
    TEST_PASS("write operations");
}

// Test write string function
void test_write_string(void) {
    TEST_START("rbuffer_writes");
    uint8_t storage[20];
    rbuffer_t buf;
    rbuffer_init(&buf, storage, 20);
    // Test writing string
    size_t written = rbuffer_writes(&buf, "hello");
    assert(written == 5);
    assert(rbuffer_used(&buf) == 5);
    // Verify content
    uint8_t read_data[10];
    size_t read_bytes = rbuffer_read(&buf, read_data, 10);
    assert(read_bytes == 5);
    assert(memcmp(read_data, "hello", 5) == 0);
    TEST_PASS("rbuffer_writes");
}

// Test writev operations
void test_writev_operations(void) {
    TEST_START("rbuffer_writev");
    uint8_t storage[20];
    rbuffer_t buf;
    rbuffer_init(&buf, storage, 20);
    // Test writev with multiple iovecs
    uint8_t data1[] = {1, 2, 3};
    uint8_t data2[] = {4, 5};
    uint8_t data3[] = {6, 7, 8, 9};
    struct iovec iov[3];
    iov[0].iov_base = data1;
    iov[0].iov_len = 3;
    iov[1].iov_base = data2;
    iov[1].iov_len = 2;
    iov[2].iov_base = data3;
    iov[2].iov_len = 4;
    size_t written = rbuffer_writev(&buf, iov, 3);
    assert(written == 9);
    assert(rbuffer_used(&buf) == 9);
    // Verify content
    uint8_t expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    uint8_t read_data[10];
    size_t read_bytes = rbuffer_read(&buf, read_data, 10);
    assert(read_bytes == 9);
    assert(memcmp(read_data, expected, 9) == 0);
    // Test writev with buffer becoming full
    rbuffer_init(&buf, storage, 8);  // capacity = 7
    uint8_t big_data1[] = {1, 2, 3, 4};
    uint8_t big_data2[] = {5, 6, 7, 8, 9, 10};  // Won't fully fit
    struct iovec iov2[2];
    iov2[0].iov_base = big_data1;
    iov2[0].iov_len = 4;
    iov2[1].iov_base = big_data2;
    iov2[1].iov_len = 6;
    written = rbuffer_writev(&buf, iov2, 2);
    assert(written == 7);  // Only 7 bytes fit
    assert(rbuffer_isfull(&buf) == true);
    TEST_PASS("rbuffer_writev");
}

// Test read operations
void test_read_operations(void) {
    TEST_START("read operations");
    uint8_t storage[10];
    rbuffer_t buf;
    rbuffer_init(&buf, storage, 10);
    // Fill buffer with test data
    uint8_t write_data[] = {1, 2, 3, 4, 5, 6, 7};
    rbuffer_write(&buf, write_data, 7);
    // Test partial read
    uint8_t read_data[10];
    size_t read_bytes = rbuffer_read(&buf, read_data, 3);
    assert(read_bytes == 3);
    assert(read_data[0] == 1);
    assert(read_data[1] == 2);
    assert(read_data[2] == 3);
    assert(rbuffer_used(&buf) == 4);
    // Test reading more than available
    read_bytes = rbuffer_read(&buf, read_data, 10);
    assert(read_bytes == 4);
    assert(read_data[0] == 4);
    assert(read_data[3] == 7);
    assert(rbuffer_isempty(&buf) == true);
    // Test read from empty buffer
    read_bytes = rbuffer_read(&buf, read_data, 5);
    assert(read_bytes == 0);
    TEST_PASS("read operations");
}

// Test readv operations
void test_readv_operations(void) {
    TEST_START("rbuffer_readv");
    uint8_t storage[15];
    rbuffer_t buf;
    rbuffer_init(&buf, storage, 15);
    // Fill buffer
    uint8_t write_data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    rbuffer_write(&buf, write_data, 10);
    // Test readv
    uint8_t buf1[4], buf2[3], buf3[5];
    struct iovec iov[3];
    iov[0].iov_base = buf1;
    iov[0].iov_len = 4;
    iov[1].iov_base = buf2;
    iov[1].iov_len = 3;
    iov[2].iov_base = buf3;
    iov[2].iov_len = 5;
    size_t total_read = rbuffer_readv(&buf, iov, 3);
    assert(total_read == 10);
    // Verify data
    assert(memcmp(buf1, write_data, 4) == 0);
    assert(memcmp(buf2, write_data + 4, 3) == 0);
    assert(memcmp(buf3, write_data + 7, 3) == 0);  // Only 3 bytes in buf3
    assert(rbuffer_isempty(&buf) == true);
    TEST_PASS("rbuffer_readv");
}

// Test wrap-around scenarios
void test_wrap_around(void) {
    TEST_START("wrap-around scenarios");
    uint8_t storage[8];
    rbuffer_t buf;
    rbuffer_init(&buf, storage, 8);
    // Fill buffer partially
    uint8_t data1[] = {1, 2, 3, 4, 5};
    rbuffer_write(&buf, data1, 5);
    // Read some data to advance read pointer
    uint8_t temp[3];
    rbuffer_read(&buf, temp, 3);
    // Now write more data that will wrap around
    uint8_t data2[] = {6, 7, 8, 9, 10};
    size_t written = rbuffer_write(&buf, data2, 5);
    assert(written == 5);  // Should fit exactly
    assert(rbuffer_isfull(&buf) == true);
    // Read all data and verify order
    uint8_t result[10];
    size_t total_read = rbuffer_read(&buf, result, 10);
    assert(total_read == 7);
    uint8_t expected[] = {4, 5, 6, 7, 8, 9, 10};
    assert(memcmp(result, expected, 7) == 0);
    TEST_PASS("wrap-around scenarios");
}

// Test iovec functions for direct buffer access
void test_iovec_functions(void) {
    TEST_START("iovec functions");
    uint8_t storage[8];
    rbuffer_t buf;
    rbuffer_init(&buf, storage, 8);
    // Test wiovec on empty buffer
    struct iovec wvec[2];
    unsigned wcount = rbuffer_wiovec(&buf, wvec);
    assert(wcount == 1);
    assert(wvec[0].iov_base == storage);
    assert(wvec[0].iov_len == 7);  // capacity
    // Fill buffer partially and test wiovec with wrap
    uint8_t data[] = {1, 2, 3, 4, 5};
    rbuffer_write(&buf, data, 5);
    uint8_t temp[3];
    rbuffer_read(&buf, temp, 3);  // r=3, w=5
    wcount = rbuffer_wiovec(&buf, wvec);
    assert(wcount == 2);
    assert(wvec[0].iov_base == storage + 5);
    assert(wvec[0].iov_len == 3);  // to end of buffer
    assert(wvec[1].iov_base == storage);
    assert(wvec[1].iov_len == 2);  // from start to r-1
    // Test riovec
    rbuffer_init(&buf, storage, 8);
    rbuffer_write(&buf, data, 5);
    struct iovec rvec[2];
    unsigned rcount = rbuffer_riovec(&buf, rvec);
    assert(rcount == 1);
    assert(rvec[0].iov_base == storage);
    assert(rvec[0].iov_len == 5);
    // Test riovec with wrap
    rbuffer_read(&buf, temp, 3);
    rbuffer_write(&buf, data, 5);  // This should wrap
    rcount = rbuffer_riovec(&buf, rvec);
    assert(rcount == 2);
    assert(rvec[0].iov_base == storage + 3);
    assert(rvec[0].iov_len == 5);  // from r to end
    assert(rvec[1].iov_base == storage);
    assert(rvec[1].iov_len == 2);  // from start to w
    TEST_PASS("iovec functions");
}

// Test advance and rewind functions
void test_advance_rewind(void) {
    TEST_START("advance and rewind functions");
    uint8_t storage[10];
    rbuffer_t buf;
    rbuffer_init(&buf, storage, 10);
    // Test write advance/rewind
    rbuffer_wadvance(&buf, 5);
    assert(buf.w == 5);
    assert(rbuffer_used(&buf) == 5);
    rbuffer_wrewind(&buf, 2);
    assert(buf.w == 3);
    assert(rbuffer_used(&buf) == 3);
    // Test wreserve
    size_t reserved = rbuffer_wreserve(&buf, 4);
    assert(reserved == 4);
    assert(buf.w == 7);
    reserved = rbuffer_wreserve(&buf, 10);  // More than available
    assert(reserved == 2);  // Only 2 slots left
    assert(rbuffer_isfull(&buf) == true);
    // Test wrewind_clamp
    size_t rewound = rbuffer_wrewind_safe(&buf, 20);
    assert(rewound == 9);  // All used data
    assert(rbuffer_isempty(&buf) == true);
    // Test read advance/rewind
    rbuffer_write(&buf, "hello", 5);
    rbuffer_radvance(&buf, 2);
    assert(buf.r == 2);
    assert(rbuffer_used(&buf) == 3);
    rbuffer_rrewind(&buf, 1);
    assert(buf.r == 1);
    assert(rbuffer_used(&buf) == 4);
    // Test rconsume_up_to
    size_t consumed = rbuffer_rskip(&buf, 2);
    assert(consumed == 2);
    assert(buf.r == 3);
    consumed = rbuffer_rskip(&buf, 10);
    assert(consumed == 2);  // Only 2 bytes left
    assert(rbuffer_isempty(&buf) == true);
    // Test rrewind_clamp with corrected implementation
    rbuffer_init(&buf, storage, 10); // Fresh buffer, capacity=9
    // Write some data: "hello"
    rbuffer_write(&buf, "hello", 5);  // r=0, w=5, used=5
    assert(rbuffer_used(&buf) == 5);
    // Read part of it
    uint8_t temp_read[3];
    rbuffer_read(&buf, temp_read, 3); // r=3, w=5, used=2
    assert(rbuffer_used(&buf) == 2);
    // Test small rewind (make 1 byte available for reading again)
    size_t rr = rbuffer_rrewind_safe(&buf, 1);
    assert(rr == 1);
    assert(rbuffer_used(&buf) == 3);  // r=2, w=5, used=3
    // Test larger rewind
    // Current state: r=2, w=5, used=3, capacity=9
    // max_rewind = capacity - used = 9 - 3 = 6
    rr = rbuffer_rrewind_safe(&buf, 10);  // Request more than possible
    assert(rr == 6);  // Should be limited by max_rewind calculation
    assert(rbuffer_used(&buf) == 9);  // Final: r=6, w=5, used = forward_distance(6,5,10) = 9
    // Test exact rewind limit
    rbuffer_init(&buf, storage, 10);
    rbuffer_write(&buf, "test", 4);   // r=0, w=4, used=4
    rbuffer_read(&buf, temp_read, 2); // r=2, w=4, used=2
    // max_rewind = capacity - used = 9 - 2 = 7
    rr = rbuffer_rrewind_safe(&buf, 7);
    assert(rr == 7);
    assert(rbuffer_used(&buf) == 9);  // Back to original state
    // Test rewind when buffer is full
    rbuffer_init(&buf, storage, 10);
    uint8_t full_data[] = {1,2,3,4,5,6,7,8,9};
    rbuffer_write(&buf, full_data, 9); // Fill to capacity
    rbuffer_read(&buf, temp_read, 3);  // r=3, w=9, used=6
    // max_rewind = capacity - used = 9 - 6 = 3
    rr = rbuffer_rrewind_safe(&buf, 5);  // Try to rewind 5
    assert(rr == 3);  // Should be limited to 3
    assert(rbuffer_used(&buf) == 9);  // Back to full
    TEST_PASS("advance and rewind functions");
}

// Test edge cases and error conditions
void test_edge_cases(void) {
    TEST_START("edge cases and error conditions");
    // Test minimum size buffer (size 2, capacity 1)
    uint8_t small_storage[2];
    rbuffer_t small_buf;
    rbuffer_init(&small_buf, small_storage, 2);
    assert(rbuffer_capacity(&small_buf) == 1);
    assert(rbuffer_available(&small_buf) == 1);
    uint8_t byte = 42;
    size_t written = rbuffer_write(&small_buf, &byte, 1);
    assert(written == 1);
    assert(rbuffer_isfull(&small_buf) == true);
    uint8_t read_byte;
    size_t read_bytes = rbuffer_read(&small_buf, &read_byte, 1);
    assert(read_bytes == 1);
    assert(read_byte == 42);
    assert(rbuffer_isempty(&small_buf) == true);
    // Test zero-length operations
    uint8_t storage[10];
    rbuffer_t buf;
    rbuffer_init(&buf, storage, 10);
    written = rbuffer_write(&buf, NULL, 0);
    assert(written == 0);
    read_bytes = rbuffer_read(&buf, NULL, 0);
    assert(read_bytes == 0);
    // Test wrap-around edge case
    rbuffer_init(&buf, storage, 10);
    // Write to almost full, then wrap exactly
    uint8_t data[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    rbuffer_write(&buf, data, 9);  // Fill to capacity
    assert(rbuffer_isfull(&buf) == true);
    assert(buf.w == 9);
    // Read one byte
    uint8_t temp;
    rbuffer_read(&buf, &temp, 1);
    assert(buf.r == 1);
    assert(rbuffer_available(&buf) == 1);
    // Write one byte (should wrap w to 0)
    uint8_t new_byte = 10;
    written = rbuffer_write(&buf, &new_byte, 1);
    assert(written == 1);
    assert(buf.w == 0);  // Wrapped
    assert(rbuffer_isfull(&buf) == true);
    // Test multiple wrap scenarios
    rbuffer_init(&buf, storage, 5);  // Small buffer for easy testing

    // Fill, read, write pattern multiple times
    {
        uint8_t test_data[] = {1, 2, 3, 4};
        written = rbuffer_write(&buf, test_data, 4);
        assert(written == 4);  // Should succeed

        uint8_t read_data[4];
        read_bytes = rbuffer_read(&buf, read_data, 2);  // Partial read
        assert(read_bytes == 2);
        assert(memcmp(read_data, test_data, 2) == 0);
        // Now buffer has 2 bytes remaining, 2 slots available
    }

    // Second cycle - buffer has 2 bytes, 2 slots available
    {
        uint8_t test_data[] = {11, 12, 13, 14};
        written = rbuffer_write(&buf, test_data, 4);
        assert(written == 2);  // Can only write 2 bytes

        uint8_t read_data[4];
        read_bytes = rbuffer_read(&buf, read_data, 2);  // Read remaining from first cycle
        assert(read_bytes == 2);
        uint8_t expected[] = {3, 4};  // Last 2 bytes from first cycle
        assert(memcmp(read_data, expected, 2) == 0);
        // Now buffer has 2 bytes from second cycle, 2 slots available
    }

    // Third cycle - demonstrate wrap-around
    {
        uint8_t test_data[] = {21, 22, 23, 24};
        written = rbuffer_write(&buf, test_data, 4);
        assert(written == 2);  // Can only write 2 bytes

        // Read all remaining data
        uint8_t read_data[4];
        read_bytes = rbuffer_read(&buf, read_data, 4);
        assert(read_bytes == 4);
        uint8_t expected[] = {11, 12, 21, 22};  // Data from cycles 2 and 3
        assert(memcmp(read_data, expected, 4) == 0);
        assert(rbuffer_isempty(&buf));
    }
    TEST_PASS("edge cases and error conditions");
}

// Test complex scenarios combining multiple operations
void test_complex_scenarios(void) {
    TEST_START("complex scenarios");
    uint8_t storage[16];
    rbuffer_t buf;
    rbuffer_init(&buf, storage, 16);
    // Scenario: Producer-consumer with varying sizes
    uint8_t expected_sequence[100];
    uint8_t actual_sequence[100];
    size_t expected_pos = 0, actual_pos = 0;
    // Simulate multiple write/read cycles
    for (int round = 0; round < 10; round++) {
        // Write varying amounts
        size_t write_len = (round % 5) + 1;  // 1-5 bytes
        uint8_t write_data[5];

        for (size_t i = 0; i < write_len; i++) {
            write_data[i] = (uint8_t)(expected_pos + i);
        }

        size_t written = rbuffer_write(&buf, write_data, write_len);

        // Record what we expect to read
        for (size_t i = 0; i < written; i++) {
            expected_sequence[expected_pos++] = write_data[i];
        }

        // Read varying amounts
        if (rbuffer_used(&buf) > 0 && (round % 2 == 0)) {
            size_t read_len = (round % 3) + 1;
            uint8_t read_data[5];
            size_t read_bytes = rbuffer_read(&buf, read_data, read_len);

            // Record what we actually read
            for (size_t i = 0; i < read_bytes; i++) {
                actual_sequence[actual_pos++] = read_data[i];
            }
        }
    }
    // Read remaining data
    while (!rbuffer_isempty(&buf)) {
        uint8_t byte;
        size_t read_bytes = rbuffer_read(&buf, &byte, 1);
        if (read_bytes > 0) {
            actual_sequence[actual_pos++] = byte;
        }
    }
    // Verify all data matches
    assert(actual_pos == expected_pos);
    assert(memcmp(expected_sequence, actual_sequence, actual_pos) == 0);
    printf("✓ Complex producer-consumer scenario works\n");

    TEST_PASS("complex scenarios");
}

int main(void) {
    printf("=== Ring Buffer Test Suite ===\n\n");
    test_init_and_state();
    test_helper_functions();
    test_write_operations();
    test_write_string();
    test_writev_operations();
    test_read_operations();
    test_readv_operations();
    test_wrap_around();
    test_iovec_functions();
    test_advance_rewind();
    test_edge_cases();
    test_complex_scenarios();
    printf("=== Test Summary ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", pass_count);
    printf("Failed: %d\n", test_count - pass_count);
    if (pass_count == test_count) {
        printf("\nall test passed\n");
        return 0;
    } else {
        printf("\nsome test\n");
        return 1;
    }
}