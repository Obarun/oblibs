/*
 * sse.h
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

#ifndef OB_SSE_H
#define OB_SSE_H

#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <stdint.h>
#include <sys/signalfd.h>

#include <oblibs/queue.h>
#include <oblibs/hash.h>
#include <oblibs/bits.h>

// earlier declaration of cron_t
struct cron_s
{
    bitset8_t seconds ; // 8 bytes
    bitset8_t minutes ; // 8 bytes
    bitset8_t hours ; // 3 bytes
    bitset8_t dom ; // days of month 4 bytes
    bitset8_t months ; // 2 bytes
    bitset8_t dow ; // days of week 1 bytes
    bitset8_t years ; // 29 bytes

    // Special flags
    bool last ; // L
    bool wday ; // W
    int hash ; // e.g. 6#3 dow = 6, hash = 3 ;
    bool dom_dow ; // false using dom, true using dow

    char tz[256] ; // store timezone, 256 should be sufficient
} ;
typedef struct cron_s cron_t ;

/* Missing defines in GLIBC <= 2.24 */
#ifndef TFD_TIMER_CANCEL_ON_SET
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#endif
#ifndef TFD_SETTIME_FLAGS
#define TFD_SETTIME_FLAGS (TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET)
#endif

#define SSE_NONE        0		        // normal loop
#define SSE_ERROR       EPOLLERR	    // error flag
#define SSE_READ        EPOLLIN		    // poll for reading
#define SSE_WRITE       EPOLLOUT	    // poll for writing
#define SSE_PRIORITY    EPOLLPRI	    // priority message
#define SSE_HUP         EPOLLHUP	    // hangup event
#define SSE_SHUT        EPOLLRDHUP	    // peer shutdown
#define SSE_EDGE        EPOLLET		    // edge triggered
#define SSE_ONESHOT     EPOLLONESHOT	// one-shot event

#define SSE_TIMEOUT_INFINITE -1
#define SSE_TIMEOUT_NOEVENT 0

#define SSE_MAX_EVENTS 128
#define SSE_MASK  (SSE_ERROR | SSE_READ | SSE_WRITE | SSE_PRIORITY |	\
			 SSE_HUP | SSE_SHUT | SSE_EDGE  | SSE_ONESHOT)

#define sse_err(w, val) do { \
    w->api_errno = (val) ? (val) : errno ; \
    errno = (val) ? (val) : errno ; \
} while (0)

#define sse_err_return(w, val) do { \
    w->api_errno = (val) ? (val) : errno ; \
    errno = (val) ? (val) : errno ; \
    return 0 ; \
} while (0)

#define sse_err_zeroing(err) *(err) = 0 ;
#define sse_err_zero(w) { w->api_errno = 0 ; }

enum sse_type_e
{
    SSE_TYPE_IO,        // Generic I/O (socket, pipe, file)
    SSE_TYPE_SIGNAL,    // Signal delivery via signalfd
    SSE_TYPE_CHILD,     // Child process monitoring via pidfd
    SSE_TYPE_INOTIFY,   // File/directory change monitoring
    SSE_TYPE_TIMER,     // One-shot or periodic timer
    SSE_TYPE_SCHEDULE,  // Cron-style scheduled event
    SSE_TYPE_EVENTFD    // Inter-thread/component signaling
} ;
typedef enum sse_type_e sse_type_t ;

struct sse_epoll_s
{
    int fd ; // epoll_fd
    int maxevents ;
    struct epoll_event ea[SSE_MAX_EVENTS] ;
    bool running ;
    queue_t *pending ; // event to process indexed by priority
    struct sse_watcher_hash_s *watchers ; // hash table of watchers
    queue_t *file ; // watchers reading stdin from file
    bool rerun_file ;
} ;
typedef struct sse_epoll_s sse_epoll_t ;
#define SSE_EPOLL_ZERO { -1, 0, {0}, false, NULL, NULL, NULL, false }

/**
 * @struct sse_time_s
 * @brief Timestamps for event lifecycle
 */
struct sse_time_s
{
    struct timespec created;      // When watcher was initialized
    struct timespec dispatched;   // When callback was called
} ;
typedef struct sse_time_s sse_time_t ;

struct sse_watcher_s ;
typedef void (sse_callback_t)(struct sse_watcher_s *w, void *cbdata, int event) ;

// The watcher is responsible to handle all errors
struct sse_watcher_s
{
    sse_epoll_t *p ; // pointer to the general epoll_t struct

    sse_type_t type ; // Type of event
    int fd ; // fd of the watcher
    int events ; // events type SSE_ERROR, SSE_READ, SSE_WRITE
    int revents ; // received epoll events

    int priority ; // priority number of the event
    bool active ; // watcher was stopped
    bool isfile ; // stdin reading from file

    sse_callback_t *cb ; // function callback
    void *cbdata ; // data to pass to the callback
    void *sdata ; // inners struct as data for specific watcher type e.g. sse_signal_t, sse_timer_t */

    int api_errno ; // errno flags returned by API functions
    sse_time_t time ; // stats of the event
} ;
typedef struct sse_watcher_s sse_watcher_t ;
#define SSE_WATCHER_ZERO { SSE_EPOLL_ZERO, 0, -1, 0, 0, 0, false, false, NULL, NULL, NULL, 0, 0 }

struct sse_watcher_hash_s
{
    sse_watcher_t *w ;
    UT_hash_handle hh ;
} ;

struct sse_timer_s
{
    struct itimerspec timeout ;
} ;
typedef struct sse_timer_s sse_timer_t ;

struct sse_schedule_s
{
    struct itimerspec when ;
    cron_t *expr ;
    bool clockchange ;
} ;
typedef struct sse_schedule_s sse_schedule_t ;

struct sse_child_s
{
    pid_t pid ;
    int status ;
} ;
typedef struct sse_child_s sse_child_t ;

struct sse_signal_s
{
    struct signalfd_siginfo si ;
} ;
typedef struct sse_signal_s sse_signal_t ;

int sse_watcher_init( \
    sse_epoll_t *p, \
    sse_watcher_t *w, \
    sse_type_t type, \
    sse_callback_t *cb, \
    void *cbdata, \
    int fd, int event, int priority) ;

/** inner helper */
extern struct sse_watcher_hash_s *sse_hash_search(sse_epoll_t *p, sse_watcher_t *w) ;
extern int sse_watcher_cmp(const void *a, const void *b) ;
extern int sse_watcher_rmcmp(const void *a, const void *b) ;
extern int sse_watcher_add(sse_watcher_t *w) ;
extern int sse_watcher_del(sse_watcher_t *w) ;
extern int sse_watcher_modify(sse_watcher_t *w) ;
extern bool sse_watcher_active(sse_watcher_t *w) ;
extern int sse_init(sse_epoll_t *p, uint32_t maxevents) ;
extern int sse_watcher_restart(sse_watcher_t *w) ;
extern int sse_watcher_stop(sse_watcher_t *w) ;
extern int sse_watcher_free(sse_watcher_t *w) ;

/** Public API */
extern int sse_new(sse_epoll_t *p, uint32_t maxevents) ;
extern void sse_sanitize(sse_epoll_t *p) ;
extern int sse_prepare(sse_epoll_t *p) ;
extern int sse_wait(sse_epoll_t *p, int timeout) ;
extern void sse_dispatch(sse_epoll_t *p) ;
extern int sse_run(sse_epoll_t *p, int timeout) ;
extern int sse_poll(sse_epoll_t *p) ;
extern void sse_free(sse_epoll_t *p) ;

extern int sse_start_signal(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, int priority) ;
extern int sse_attach_signal(sse_watcher_t *w, int signal) ;
extern int sse_detach_signal(sse_watcher_t *w, int signal) ;
extern int sse_ignore_signal(sse_watcher_t *w, int signal) ;
extern int sse_restart_signal(sse_watcher_t *w) ;
extern int sse_stop_signal(sse_watcher_t *w) ;
extern int sse_free_signal(sse_watcher_t *w) ;

extern int sse_start_io(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, int fd, int event, int priority) ;
extern int sse_modify_io(sse_watcher_t *w, int fd, int events) ;
extern int sse_restart_io(sse_watcher_t *w) ;
extern int sse_stop_io(sse_watcher_t *w) ;
extern int sse_free_io(sse_watcher_t *w) ;

extern int sse_start_inotify(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, int priority) ;
extern int sse_attach_inotify(sse_watcher_t *w, const char *what, uint32_t imask) ;
extern int sse_detach_inotify(sse_watcher_t *w, const char *what) ;
extern int sse_update_inotify(sse_watcher_t *w, const char *what, uint32_t imask) ;
extern int sse_restart_inotify(sse_watcher_t *w) ;
extern int sse_stop_inotify(sse_watcher_t *w) ;
extern int sse_free_inotify(sse_watcher_t *w) ;

extern int sse_start_eventfd(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, int priority) ;
extern int sse_restart_eventfd(sse_watcher_t *w) ;
extern int sse_stop_eventfd(sse_watcher_t *w) ;
extern int sse_free_eventfd(sse_watcher_t *w) ;
extern int sse_write_eventfd(sse_watcher_t *w) ;
extern int sse_read_eventfd(sse_watcher_t *w) ;

extern int sse_start_timer(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, int timeout, int period, int priority) ;
extern int sse_modify_timer(sse_watcher_t *w, int timeout, int period) ;
extern int sse_restart_timer(sse_watcher_t *w) ;
extern int sse_stop_timer(sse_watcher_t *w) ;
extern int sse_free_timer(sse_watcher_t *w) ;

extern int sse_start_schedule(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, cron_t *expr, int priority) ;
extern int sse_modify_schedule(sse_watcher_t *w, time_t when) ;
extern int sse_restart_schedule(sse_watcher_t *w) ;
extern int sse_stop_schedule(sse_watcher_t *w) ;
extern int sse_free_schedule(sse_watcher_t *w) ;
extern time_t sse_getfire_schedule(sse_watcher_t *w) ;

/**
 *  Running pidwait we can see the following epoll events:
 *     - When the child exits, EPOLLIN on the child is triggered. At this stage the child is a zombie.
 *     - When the parent calls wait(), then EPOLLIN | EPOLLHUP on the child is triggered.
 *     - When the parent exits, EPOLLIN then EPOLLIN | EPOLLHUP on the parent is triggered. That is, two events for the one thing.
 *  If you want to use epoll() to know when a process terminates, then you need to decide on what you mean by that:
 *     - If you mean it has exited, but not collected yet (e.g. a zombie possibly) then you need to select on EPOLLIN only.
 *     - If you mean the process is fully gone, then EPOLLHUP is a better choice. You can even change the epoll_ctl() call to use this instead.
 *  A “zombie trigger” (EPOLLIN with no subsequent EPOLLHUP) is a bit tricky to work out.
 *  There is no guarantee the two events have to be in the same epoll, especially if the parent is a bit tardy on their wait() call.
 */
extern int sse_start_child(sse_epoll_t *p, sse_watcher_t *w, sse_callback_t *cb, void *cbdata, pid_t pid, int priority, bool sigchild) ;
extern int sse_restart_child(sse_watcher_t *w) ;
extern int sse_stop_child(sse_watcher_t *w) ;
extern int sse_free_child(sse_watcher_t *w) ;

// cron

#define CRON_MIN_YEARS 1970
#define CRON_MAX_YEARS 2200
#define CRON_ZERO 0
#define CRON_MAX_SECONDS 60
#define CRON_MAX_MINUTES 60
#define CRON_MAX_HOURS 24
#define CRON_MAX_DOM 32
#define CRON_MAX_DOW 7
#define CRON_MAX_MONTHS 12
#define CRON_DAY_IN_SECONDS 24 * 60 * 60

#define CRON_BITSET_SECONDS 60
#define CRON_BITSET_MINUTES 60
#define CRON_BITSET_HOURS 24
#define CRON_BITSET_DOM 31
#define CRON_BITSET_MONTHS 12
#define CRON_BITSET_DOW 7
#define CRON_BITSET_YEARS 229

#define CRON_EXPR_ZERO { BITSET8_ZERO, BITSET8_ZERO, BITSET8_ZERO, BITSET8_ZERO, BITSET8_ZERO, BITSET8_ZERO, BITSET8_ZERO, false, false, -1, false, {0} }

enum cron_type_e {
    CRON_TYPE_SECOND,
    CRON_TYPE_MINUTE,
    CRON_TYPE_HOUR,
    CRON_TYPE_DOM,
    CRON_TYPE_MONTH,
    CRON_TYPE_DOW,
    CRON_TYPE_YEAR,
    CRON_TYPE_OTHER,
    CRON_TYPE_ENDOFKEY
} ;
typedef enum cron_type_e cron_type_t ;

/**
 * @brief Parses a cron expression string into a structured format.
 *
 * This function takes a standard cron expression string and populates a
 * `cron_t` structure with bitsets representing the scheduled times
 * for seconds, minutes, hours, day of month, month, day of week, and year.
 * It supports standard cron fields, special characters (?, *, /, -, L, W, #),
 * and predefined macros (@yearly, @monthly, etc.).
 *
 * The expected format is:
 * `seconds minutes hours day_of_month month day_of_week [year]`
 * or a macro like `@daily`.
 *
 * @param expression
 * The null-terminated cron expression string to parse.
 * This can be a standard expression or a predefined macro.
 *
 * @param expr
 * Pointer to a `cron_t` structure to be populated
 * with the parsed results. The structure should be
 * initialized (e.g., using `CRON_EXPR_ZERO`) before calling.
 *
 * @param tz
 * Array of char specifying the timezone for
 * the calculation (e.g., "UTC", "Europe/London", "America/New_York").
 * If `NULL` or an empty string, the calculation is performed in UTC.
 *
 * @return
 * 1 on successful parsing, 0 on failure.
 *
 * @retval
 * 1 Parsing was successful. The `expr` structure is populated.
 * 0 Parsing failed due to:
 *      - Invalid input (`expression` or `expr` is NULL).
 *      - Memory allocation failure (`ENOMEM`).
 *      - Incorrect number of fields (not 5, 6, or 7) (`EINVAL`).
 *      - Syntax error in any field (`EINVAL`).
 *      - Invalid use of special characters (e.g., both DOM and DOW specified without '?' (`EINVAL`).
 *
 * @note
 * The function uses `errno` to report specific error codes.
 *
 * If the year field is omitted, it defaults to all years within
 * the range `[CRON_MIN_YEARS, CRON_MAX_YEARS]`.
 *
 * Predefined macros are expanded into their equivalent standard expressions
 * before parsing.
 *
 * The parser enforces that either the Day of Month (DOM) or Day of Week (DOW)
 * field (but not both, unless one is '?') specifies the day.
 */
int parse_cron(const char *expression, cron_t *expr, const char *tz) ;

/**
 * @brief Calculates the next execution time based on a parsed cron expression.
 *
 * This function determines the next `time_t` value that matches the schedule
 * defined by the `expr` cron expression, starting from the time specified by `now`.
 * It correctly handles timezones specified by the `tz` parameter.
 *
 * The function iterates through potential future times, checking each component
 * (year, month, day, hour, minute, second) against the constraints defined in
 * the `cron_t` structure. It efficiently skips large periods of time that
 * cannot match, such as months or years not included in the schedule.
 *
 * Special cron features like `?`, `L` (last), `W` (weekday), and `#` (nth weekday)
 * are handled through the `handle_day_condition` function.
 *
 * @param expr
 * Pointer to a `cron_t` structure that has been successfully
 * populated by `cron_parse_expr`.
 *
 * @param now
 * Pointer to a `time_t` value representing the current time. The
 * search for the next execution time starts *after* this moment.
 * If `NULL`, the behavior is undefined.
 *
 * @return
 * The `time_t` value of the next scheduled execution time.
 * (time_t)-1 An error occurred during calculation (e.g., timezone handling
 * failure, internal time conversion error, or no valid time found
 * within a 100-year search limit).
 *
 * @note
 * The function uses `log_warn` to report issues.
 *
 * The search is limited to 100 years into the future to prevent potential
 * infinite loops. If no matching time is found within this window,
 * `(time_t)-1` is returned.
 */
time_t cron_next(cron_t *expr, const time_t *now) ;

/**
 * @brief Sets a range of bits in a bitset.
 *
 * This function sets bits in the specified bitset (`field`) starting from
 * the `min` index (inclusive) up to, but not including, the `max` index (exclusive),
 * incrementing by `step`. This is a private function.
 *
 * @param field
 * Pointer to the `bitset8_t` structure to modify.
 *
 * @param min
 * The starting index (inclusive) of the range to set. Must be less than `max`.
 *
 * @param max
 * The ending index (exclusive) of the range to set. Must be greater than `min`.
 *
 * @param step
 * The increment between bit indices to set. If `step` is less than or equal to 0,
 * it is treated as 1. Must be positive to make progress if `min` < `max`.
 *
 * @note If `min` is greater than or equal to `max`, the loop condition `pos < max` will be
 *       false initially, and no bits will be set.
 * @note
 * If `step` is 0 or negative, it defaults to 1 to prevent an infinite loop or no progress.
 */
void bitset_range(bitset8_t *field, uint32_t min, uint32_t max, int step) ;

#endif
