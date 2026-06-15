#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <oblibs/linux.h>

// Helper: Send a signal to the process
static void send_signal(int sig) {
    if (kill(getpid(), sig) < 0) {
        perror("kill");
        assert(0 && "Failed to send signal");
    }
}

// Test lx_signalfd_init
void test_lx_signalfd_init(void) {
    printf("Testing lx_signalfd_init...\n");
    int fd = lx_signalfd_init();
    assert(fd >= 0 && "Failed to initialize signalfd");
    assert(lx_signalfd_get_fd() == fd && "lx_signalfd_get_fd does not match initialized fd");

    // Verify mask is empty: inspect the mask actually returned, not the result
    // of sigemptyset() (which clears it in place and always returns 0).
    sigset_t mask = lx_signalfd_get_mask();
    assert(sigisemptyset(&mask) && "Initial mask should be empty");

    // Test reinitialization
    int fd2 = lx_signalfd_init();
    assert(fd2 == fd && "Reinitialization returned different fd");

    lx_signalfd_end();
}

// Test lx_signalfd_get_fd
void test_lx_signalfd_get_fd(void) {
    printf("Testing lx_signalfd_get_fd...\n");
    int fd = lx_signalfd_init();
    assert(fd >= 0 && "Failed to initialize signalfd for get_fd test");
    assert(lx_signalfd_get_fd() == fd && "lx_signalfd_get_fd returned incorrect fd");

    lx_signalfd_end();
    assert(lx_signalfd_get_fd() == -1 && "lx_signalfd_get_fd should return -1 after end");
}

// Test lx_signalfd_get_mask
void test_lx_signalfd_get_mask(void) {
    printf("Testing lx_signalfd_get_mask...\n");
    int fd = lx_signalfd_init();
    assert(fd >= 0 && "Failed to initialize signalfd for get_mask test");

    // Initial mask should be empty
    sigset_t mask = lx_signalfd_get_mask();
    assert(sigisemptyset(&mask) && "Initial mask should be empty");

    // Add a signal and verify mask
    assert(lx_signalfd_add(SIGUSR1) == 1 && "Failed to add SIGUSR1");
    mask = lx_signalfd_get_mask();
    assert(sigismember(&mask, SIGUSR1) == 1 && "Mask should contain SIGUSR1");

    lx_signalfd_end();
}

// Test lx_signalfd_add
void test_lx_signalfd_add(void) {
    printf("Testing lx_signalfd_add...\n");
    int fd = lx_signalfd_init();
    assert(fd >= 0 && "Failed to initialize signalfd for add test");

    // Add SIGUSR1
    assert(lx_signalfd_add(SIGUSR1) == 1 && "Failed to add SIGUSR1");
    sigset_t mask = lx_signalfd_get_mask();
    assert(sigismember(&mask, SIGUSR1) == 1 && "Mask should contain SIGUSR1");

    // Add same signal again
    assert(lx_signalfd_add(SIGUSR1) == 1 && "Adding existing signal should return 1");

    // Add another signal
    assert(lx_signalfd_add(SIGUSR2) == 1 && "Failed to add SIGUSR2");
    mask = lx_signalfd_get_mask();
    assert(sigismember(&mask, SIGUSR2) == 1 && "Mask should contain SIGUSR2");

    // Test with uninitialized fd
    lx_signalfd_end();
    errno = 0;
    assert(lx_signalfd_add(SIGUSR1) == 0 && errno == EBADF && "Expected EBADF with uninitialized fd");

    // Test invalid signal
    errno = 0;
    int fd2 = lx_signalfd_init();
    assert(fd2 >= 0 && "Failed to initialize signalfd for invalid signal test");
    assert(lx_signalfd_add(-1) == 0 && errno == EINVAL && "Expected EINVAL for invalid signal");
    lx_signalfd_end();
}

// Test lx_signalfd_del
void test_lx_signalfd_del(void) {
    printf("Testing lx_signalfd_del...\n");
    int fd = lx_signalfd_init();
    assert(fd >= 0 && "Failed to initialize signalfd for del test");

    // Add SIGUSR1
    assert(lx_signalfd_add(SIGUSR1) == 1 && "Failed to add SIGUSR1 for del test");

    // Delete SIGUSR1
    assert(lx_signalfd_del(SIGUSR1) == 1 && "Failed to delete SIGUSR1");
    sigset_t mask = lx_signalfd_get_mask();
    assert(sigismember(&mask, SIGUSR1) == 0 && "Mask should not contain SIGUSR1 after deletion");

    // Delete non-existent signal
    assert(lx_signalfd_del(SIGUSR2) == 1 && "Deleting non-existent signal should return 1");

    // Test with uninitialized fd
    lx_signalfd_end();
    errno = 0;
    assert(lx_signalfd_del(SIGUSR1) == 0 && errno == EBADF && "Expected EBADF with uninitialized fd");

    // Test invalid signal
    errno = 0;
    int fd2 = lx_signalfd_init();
    assert(fd2 >= 0 && "Failed to initialize signalfd for invalid signal test");
    assert(lx_signalfd_del(-1) == 0 && errno == EINVAL && "Expected EINVAL for invalid signal");
    lx_signalfd_end();
}

// Test lx_signalfd_ignore
void test_lx_signalfd_ignore(void) {
    printf("Testing lx_signalfd_ignore...\n");

    // Test ignoring a catchable signal (SIGUSR1)
    assert(lx_signalfd_ignore(SIGUSR1) == 1 && "Failed to ignore SIGUSR1");

    // Verify SIGUSR1 is ignored by sending it
    pid_t pid = getpid();
    if (kill(pid, SIGUSR1) < 0)
        assert(0 && "Failed to send SIGUSR1");

    // No assertion on signal receipt, as it’s ignored (no-op handler)

    // Test ignoring a non-catchable signal (SIGKILL): sigaction() rejects it with
    // EINVAL and lx_signalfd_ignore reports success (1) precisely because the
    // signal number is valid but uncatchable. Parenthesize the errno alternative
    // so it is actually asserted (the bare `a || b && msg` form binds as
    // `a || (b && msg)`, which is true whenever a holds and never checks msg).
    errno = 0;
    int killret = lx_signalfd_ignore(SIGKILL);
    assert(killret == 1 && "Expected success for non-catchable SIGKILL");
    assert(errno == EINVAL && "SIGKILL is uncatchable: sigaction must leave EINVAL");

    // Test invalid signal number
    errno = 0;
    assert(lx_signalfd_ignore(-1) == 0 && errno == EINVAL && "Expected EINVAL for invalid signal");
}

// Test lx_signalfd_read
void test_lx_signalfd_read(void) {
    printf("Testing lx_signalfd_read...\n");
    struct signalfd_siginfo si ;
    int fd = lx_signalfd_init();
    assert(fd >= 0 && "Failed to initialize signalfd for read test");

    // Add SIGUSR1
    assert(lx_signalfd_add(SIGUSR1) == 1 && "Failed to add SIGUSR1 for read test");

    // Send SIGUSR1
    send_signal(SIGUSR1);

    // Read signal
    assert(lx_signalfd_read(&si));
    assert(si.ssi_signo == SIGUSR1 && "Expected SIGUSR1 from lx_signalfd_read");

    // Test non-blocking behavior (no pending signals)
    errno = 0;
    assert(lx_signalfd_read(&si) == 0 && errno == 0 && "Expected 0 for no pending signals (EAGAIN)");

    // Test with uninitialized fd
    lx_signalfd_end();
    errno = 0;
    assert(lx_signalfd_read(&si) < 0 && errno == EBADF && "Expected EBADF with uninitialized fd");
}

// Test lx_signalfd_end
void test_lx_signalfd_end(void) {
    printf("Testing lx_signalfd_end...\n");
    int fd = lx_signalfd_init();
    assert(fd >= 0 && "Failed to initialize signalfd for end test");

    // Add SIGUSR1
    assert(lx_signalfd_add(SIGUSR1) == 1 && "Failed to add SIGUSR1 for end test");

    lx_signalfd_end();
    assert(lx_signalfd_get_fd() == -1 && "fd should be -1 after end");
    sigset_t mask = lx_signalfd_get_mask();
    assert(sigisemptyset(&mask) && "Mask should be empty after end");

    // Verify signals are unblocked
    sigset_t proc_mask;
    sigprocmask(SIG_BLOCK, NULL, &proc_mask);
    assert(sigismember(&proc_mask, SIGUSR1) == 0 && "SIGUSR1 should be unblocked after end");
}

// Main test runner
int main(void) {
    test_lx_signalfd_init();
    test_lx_signalfd_get_fd();
    test_lx_signalfd_get_mask();
    test_lx_signalfd_add();
    test_lx_signalfd_ignore();
    test_lx_signalfd_del();
    test_lx_signalfd_read();
    test_lx_signalfd_end();
    printf("All tests passed successfully!\n");
    return 0;
}