/*
 * cron.c
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

#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <oblibs/bits.h>
#include <oblibs/log.h>

#include <oblibs/sse.h>

static bool local = true ;

static int find_next(const bitset8_t *bitset, int from, int max)
{
    for (int start = from ; start < max ; start++)
        if (bitset8_isvalid(bitset, start))
            return start ;

    return -1 ;
}

static void redo_tz(const char *old_tz)
{
    if (old_tz)
        setenv("TZ", old_tz, 1) ;
    else unsetenv("TZ") ;

    tzset() ;
}

static int handle_tz(const time_t *input_time, const char *tz, struct tm *calendar)
{
    const char *old_tz = getenv("TZ") ;

    if (tz && *tz) {
        if (setenv("TZ", tz, 1) < 0)
            return 0 ;
    } else {
        if (getenv("TZ") == NULL)
            if (setenv("TZ", "UTC", 1) < 0)
                return 0 ;
    }

    tzset() ;

    if (!localtime_r(input_time, calendar)) {
        redo_tz(old_tz) ;
        return 0 ;
    }

    redo_tz(old_tz) ;

    return 1 ;
}

// Return -1 on fail*/
static time_t calendar_to_time(struct tm *calendar)
{
    if (!local)
        return timegm(calendar) ;

    return mktime(calendar) ;
}

// return NULL on fail*/
static struct tm *time_to_calendar(time_t *date, struct tm *calendar)
{
    if (!local)
        return gmtime_r(date, calendar) ;

    return localtime_r(date, calendar) ;
}

static int last_dom(int month, int year, bool weekday)
{
    struct tm calendar ;
    time_t t ;
    memset(&calendar, 0, sizeof(struct tm)) ;
    calendar.tm_mon = month + 1 ; /* next month */
    calendar.tm_year = year ; /* years since 1900 */
    t = calendar_to_time(&calendar) ;

    if (weekday) {
        // deal with Saturday (6) or Sunday (0)
        while (time_to_calendar(&t, &calendar)->tm_wday == 6 || time_to_calendar(&t, &calendar)->tm_wday == 0)
            t -= CRON_DAY_IN_SECONDS ;
    }
    /** If tm_mday == 0, the time functions treat this as
     * "zero-th day of the month", which is the last day
     * of the previous month. */
    return time_to_calendar(&t, &calendar)->tm_mday ;
}

static int closest_weekday(int dom, int month, int year)
{
    struct tm calendar ;
    time_t t ;
    int wday ;
    memset(&calendar, 0, sizeof(struct tm)) ;
    calendar.tm_mon = month ;
    calendar.tm_mday = dom ;
    calendar.tm_year = year ;
    t = calendar_to_time(&calendar);

    wday = time_to_calendar(&t, &calendar)->tm_wday ;

    /* Sunday ? */
    if (wday == 0) {
        /* last day of the month ? go to the previous Friday : go to the next Monday */
        if (dom + 1 == last_dom(month, year, 0)) t -= 2 * CRON_DAY_IN_SECONDS ;
        else t += CRON_DAY_IN_SECONDS ;
    /* Saturday ? */
    } else if (wday == 6) {
        // first day of the month ? go to the next Monday : go to the previous Friday
        if (dom == 0) t += 2 * CRON_DAY_IN_SECONDS ;
        else t -= CRON_DAY_IN_SECONDS ;
    }

    return time_to_calendar(&t, &calendar)->tm_mday ;
}

static int nth_weekday_of_month(int n, int target_wday, int month, int year)
{
    struct tm calendar ;
    time_t t ;
    int day, first_wday ;

    memset(&calendar, 0, sizeof(struct tm)) ;
    calendar.tm_year = year ;
    calendar.tm_mon = month ;
    calendar.tm_mday = 1 ;

    t = calendar_to_time(&calendar) ;
    time_to_calendar(&t, &calendar) ;

    first_wday = calendar.tm_wday ;

    // Calculate the offset from the 1st to the first target weekday
    int offset = (target_wday - first_wday + 7) % 7 ;
    day = 1 + offset + (n - 1) * 7 ;

    // Check if the calculated day exceeds the last day of the month
    int last_day = last_dom(month, year, false) ;

    if (day > last_day)
        return -1 ;  // Invalid: nth weekday does not exist

    return day ;
}

static int last_weekday_of_month(int year, int month, int target_wday)
{
    struct tm calendar = {0} ;
    time_t time ;
    int days_back ;

    // Set to 1st day of *next* month, then subtract 1 second/day
    calendar.tm_year = year ;
    calendar.tm_mon = month + 1;
    calendar.tm_mday = 0 ;
    calendar.tm_hour = 12 ; // Noon to avoid potential DST issues
    calendar.tm_min = 0 ;
    calendar.tm_sec = 0 ;
    calendar.tm_isdst = -1 ; // Let calendar_to_time determine DST

    time = calendar_to_time(&calendar) ;
    if (time == (time_t)-1)
        return -1 ;

    days_back = calendar.tm_wday - target_wday ;
    if (days_back < 0)
        days_back += 7 ;

    return calendar.tm_mday - days_back ;
}

static int handle_day_condition(bitset8_t *days, cron_t *expr, struct tm *calendar, int ldom)
{
    bool using_dom = expr->dom_dow ;

    if (expr->dom.count && expr->dow.count)
        log_warn_return(LOG_EXIT_ZERO, "invalid cron expression: dom and dow both set") ;

    if ((!expr->dom.count && !expr->last) && !expr->dow.count)
        log_warn_return(LOG_EXIT_ZERO, "invalid cron expression: neither dom nor dow set") ;

    if (!using_dom) {

        if (expr->last || expr->wday) {

            int target = -1 ;
            if (expr->last) {
                // L combinaison
                if (expr->wday) {
                    // LW
                    target = last_dom(calendar->tm_mon, calendar->tm_year, true) ;

                } else {

                    if (!expr->dom.count) {
                        // L alone
                        target = last_dom(calendar->tm_mon, calendar->tm_year, false) ;

                    } else {
                        // L-offset
                        int offset = find_next(&expr->dom, 0, CRON_MAX_DOM) ;
                        /** It should be already handled properly at parse time.
                         * If no value is found the parser failed at some point
                         */
                        if (offset == -1)
                            return (errno = EINVAL, -1) ;

                        bitset_range(days, offset, CRON_MAX_DOM, 0) ;
                    }
                }

            } else if (expr->wday) {
                // W combinaison
                int weekd = find_next(&expr->dom, 0, CRON_MAX_DOM) ;
                target = closest_weekday(weekd, calendar->tm_mon, calendar->tm_year) ;

            }

            if (target >= 0)
                bitset8_set(days, target) ;

        } else {
            // Normal dom
            for (int i = 0 ; i <= ldom ; i++) {
                if (bitset8_isvalid(&expr->dom, i))
                    bitset8_set(days, i) ;
            }
        }

    } else if (using_dom) {

        int target = -1 ;

        if (expr->hash > -1) {
            // 6#3: the 3rd Friday of the month.
            int dweek = find_next(&expr->dow, CRON_ZERO, CRON_MAX_DOW + 1) ;

            target = nth_weekday_of_month(expr->hash, dweek, calendar->tm_mon, calendar->tm_year) ;
            if (target < 0)
                log_warnu_return(LOG_EXIT_LESSONE, "find the nth weekday of the month") ;

        } else if (expr->last) {

            int day = find_next(&expr->dow, CRON_ZERO, CRON_MAX_DOW + 1) ;

            if (day == 7) {
                /** L: on Saturday, the 7th day of the week.
                 * The parser already handle it.
                 */
                target = day ;

            } else {
                // 2L: at the last Monday of the month.
                target = last_weekday_of_month(calendar->tm_year, calendar->tm_mon, day) ;
                if (target < 0)
                    return 0 ;
            }

        } else {

            for (int i = 1; i <= ldom; i++) {
                struct tm tmp = *calendar;  // Copy current calendar
                tmp.tm_mday = i;

                time_t t = calendar_to_time(&tmp);
                struct tm temp;
                time_to_calendar(&t, &temp);
                int wday = temp.tm_wday; // Sunday = 0

                if (bitset8_isvalid(&expr->dow, wday)) {
                    bitset8_set(days, i);
                }
            }

        }

        if (target >= 0)
            bitset8_set(days, target) ;
    }

    return 1 ;
}

static time_t cron_next_locked(cron_t *expr, const time_t *now)
{

    local = expr->tz[0] == '\0' ? false : true ;
    char *tz = expr->tz[0] == '\0' ? 0 : expr->tz ;
    int r ;
    struct tm calendar ;
    time_t local_time ;

    // avoid infinite loop
    time_t max_time = *now + 100LL * 365 * 24 * 3600 ;

    // Handle timezone: skip handle_tz if tz is NULL
    if (local) {
        if (!handle_tz(now, tz, &calendar)) {
            log_warn("Failed to set timezone") ;
            return (time_t)-1 ;
        }
    } else {
        // UTC
        if (!gmtime_r(now, &calendar)) {
            log_warn("Failed to get local time") ;
            return (time_t)-1 ;
        }
    }

    local_time = *now + 1 ;
    time_to_calendar(&local_time, &calendar) ;

    while (local_time < max_time) {

        bitset8_t days = bitset8_init(CRON_BITSET_DOM) ;

        if (expr->years.count && !bitset8_isvalid(&expr->years, calendar.tm_year)) {
            int next = find_next(&expr->years, calendar.tm_year + 1, CRON_MAX_YEARS) ;
            if (next == -1)
                return (time_t)-1 ;

            calendar.tm_year = next ;
            calendar.tm_mon = find_next(&expr->months, 0, CRON_MAX_MONTHS + 1) - 1 ;
            calendar.tm_mday = 1 ;
            calendar.tm_hour = 0 ;
            calendar.tm_min = 0 ;
            calendar.tm_sec = 0 ;
            continue ;
        }

        if (!bitset8_isvalid(&expr->months, calendar.tm_mon + 1)) {
            int next = find_next(&expr->months, calendar.tm_mon + 1, CRON_MAX_MONTHS + 1) - 1 ;
            if (next < 0) {
                calendar.tm_year++ ;
                calendar.tm_mon = find_next(&expr->months, 0, CRON_MAX_MONTHS + 1) - 1 ;
            } else {
                calendar.tm_mon = next ;
            }
            calendar.tm_mday = 1 ;
            calendar.tm_hour = 0 ;
            calendar.tm_min = 0 ;
            calendar.tm_sec = 0 ;
            continue ;
        }

        local_time = calendar_to_time(&calendar) ;
        if (local_time == (time_t)-1) {
            log_warn("Failed to convert time") ;
            return (time_t)-1 ;
        }
        time_to_calendar(&local_time, &calendar) ;

        int ldom = last_dom(calendar.tm_mon, calendar.tm_year, false) ;

        r = handle_day_condition(&days, expr, &calendar, ldom) ;
        if (r <= 0) {
            /** if r < 0:
             * nth_weekday_of_month doesn't found it
             * e.g. 1#5 where that month have only 4 monday.
             * User may have picked only one month and year.
             * This cron expression is obsoleted */
            log_warn("No valid next time found within limit") ;
            return (time_t)-1 ;
        }

        if (!bitset8_isvalid(&days, calendar.tm_mday)) {
            int next_d = find_next(&days, calendar.tm_mday, ldom + 1) ;

            if (next_d == -1) {
                // go to next month
                calendar.tm_mon = find_next(&expr->months, calendar.tm_mon + 2, CRON_MAX_MONTHS + 1) - 1 ;
                if (calendar.tm_mon < 0) {
                    calendar.tm_year++ ;
                    calendar.tm_mon = find_next(&expr->months, 0, CRON_MAX_MONTHS + 1) - 1 ;
                }
                calendar.tm_mday = 1 ;
                calendar.tm_hour = 0 ;
                calendar.tm_min = 0 ;
                calendar.tm_sec = 0 ;
                continue ;
            }
            calendar.tm_mday = next_d ;
            calendar.tm_hour = 0 ;
            calendar.tm_min = 0 ;
            calendar.tm_sec = 0 ;
            continue ;
        }

        if (!bitset8_isvalid(&expr->hours, calendar.tm_hour)) {
            int next = find_next(&expr->hours, calendar.tm_hour + 1, CRON_MAX_HOURS) ;
            if (next == -1) {
                calendar.tm_mday++;
                calendar.tm_hour = find_next(&expr->hours, 0, CRON_MAX_HOURS) ;
            } else {
                calendar.tm_hour = next ;
            }
            calendar.tm_min = 0 ;
            calendar.tm_sec = 0 ;
            continue ;
        }

        if (!bitset8_isvalid(&expr->minutes, calendar.tm_min)) {
            int next = find_next(&expr->minutes, calendar.tm_min + 1, CRON_MAX_MINUTES) ;
            if (next == -1) {
                calendar.tm_hour++ ;
                calendar.tm_min = find_next(&expr->minutes, 0, CRON_MAX_MINUTES) ;
            } else {
                calendar.tm_min = next ;
            }
            calendar.tm_sec = 0 ;
            continue ;
        }

        if (!bitset8_isvalid(&expr->seconds, calendar.tm_sec)) {
            int next = find_next(&expr->seconds, calendar.tm_sec + 1, CRON_MAX_SECONDS) ;
            if (next == -1) {
                calendar.tm_min++ ;
                calendar.tm_sec = find_next(&expr->seconds, 0, CRON_MAX_SECONDS) ;
            } else {
                calendar.tm_sec = next ;
            }
            continue ;
        }

        // All constraints satisfied
        local_time = calendar_to_time(&calendar) ;
        if (local_time == (time_t)-1) {
            log_warn("Failed to convert time") ;
            return (time_t)-1 ;
        }

        time_to_calendar(&local_time, &calendar) ;

        return local_time ;
    }

    log_warn("No valid next time found within limit") ;
    return (time_t)-1 ;
}

time_t cron_next(cron_t *expr, const time_t *now)
{
    /* When a timezone is set, EVERY mktime()/localtime() of the search must run
     * under it, not just the first conversion. Pin TZ for the whole computation
     * and restore it once. With no tz the computation runs in UTC (timegm/
     * gmtime) and needs no environment change. */
    if (expr->tz[0] == '\0')
        return cron_next_locked(expr, now) ;

    char const *cur = getenv("TZ") ;
    char *saved = cur ? strdup(cur) : 0 ;   /* setenv may invalidate getenv's pointer */
    if (cur && !saved)
        return (time_t)-1 ;

    if (setenv("TZ", expr->tz, 1) < 0) {
        free(saved) ;
        return (time_t)-1 ;
    }
    tzset() ;

    time_t result = cron_next_locked(expr, now) ;

    if (saved)
        setenv("TZ", saved, 1) ;
    else
        unsetenv("TZ") ;
    tzset() ;
    free(saved) ;

    return result ;
}
