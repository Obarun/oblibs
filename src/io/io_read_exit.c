#include <sys/types.h>
#include <errno.h>

/**
 * @brief Checks if an error code represents a non-blocking I/O retry condition.
 *
 * This macro evaluates whether the given error code corresponds to `EAGAIN`
 * or `EWOULDBLOCK`, which both indicate that a non-blocking read or write
 * operation would block and should be retried later.
 *
 * @param e The error code to check (typically `errno`).
 * @return Non-zero (true) if the error is EAGAIN or EWOULDBLOCK, 0 otherwise.
 */
#define error_isagain(e) (((e) == EAGAIN) || ((e) == EWOULDBLOCK))

ssize_t io_read_exit (ssize_t r)
{
  return r == -1 ? error_isagain(errno) ? (errno = 0, 0) : -1 : !r ? (errno = EPIPE, -1) : r ;
}
