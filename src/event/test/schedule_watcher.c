/*
 * schedule_watcher.c
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

#include <time.h>
#include <unistd.h>

#include <oblibs/sse.h>

#include "framework.h"

/* Test callback for schedule events */
static void test_schedule_callback(sse_watcher_t *w, void *cbdata, int event) {
    struct test_callback_data *data = (struct test_callback_data *)cbdata;
    data->call_count++;
    data->last_event = event;
    data->callback_called = true;

    /* Store watcher pointer and schedule info */
    data->user_data = w;

    /* Store if this was a clock change event */
    if (w->sdata) {
        sse_schedule_t *schedule_data = (sse_schedule_t *)w->sdata;
        if (schedule_data->clockchange) {
            data->expected_event = 1; /* Use this field to flag clock change */
        }
    }
}

/* Main schedule watcher test - single loop handling multiple cron schedules */
static bool test_schedule_watchers_comprehensive(void) {
    sse_epoll_t loop;

    /* Multiple schedule watchers */
    sse_watcher_t every_minute, every_5min, hourly, daily;
    struct test_callback_data minute_data, min5_data, hourly_data, daily_data;

    /* Cron expressions */
    cron_t every_minute_expr = CRON_EXPR_ZERO;
    cron_t every_5min_expr = CRON_EXPR_ZERO;
    cron_t hourly_expr = CRON_EXPR_ZERO;
    cron_t daily_expr = CRON_EXPR_ZERO;
    cron_t test_expr = CRON_EXPR_ZERO;

    /* Initialize callback data */
    test_callback_init(&minute_data);
    test_callback_init(&min5_data);
    test_callback_init(&hourly_data);
    test_callback_init(&daily_data);

    /* Create single event loop */
    TEST_ASSERT(sse_new(&loop, 20), "Failed to create epoll loop");
    loop.running = true ;

    /* === Parse cron expressions === */
    /* For testing, we'll use shorter intervals to avoid long test runs */

    // Every minute: "* * * * ?"
    int r = parse_cron("* * * * ?", &every_minute_expr, NULL);
    TEST_ASSERT(r, "Failed to parse every minute cron expression");

    // Every 5 minutes: "*/5 * * * ?"
    r = parse_cron("*/5 * * * ?", &every_5min_expr, NULL);
    TEST_ASSERT(r, "Failed to parse every 5 minutes cron expression");

    /* Every hour: "0 * * * ?" */
    r = parse_cron("0 * * * ?", &hourly_expr, NULL);
    TEST_ASSERT(r, "Failed to parse hourly cron expression");

    /* Daily at midnight: "0 0 * * ?" */
    r = parse_cron("0 0 * * ?", &daily_expr, NULL);
    TEST_ASSERT(r, "Failed to parse daily cron expression");

    /* === Start schedule watchers === */
    TEST_ASSERT(sse_start_schedule(&loop, &every_minute, test_schedule_callback, &minute_data,
                                  &every_minute_expr, 1),
               "Failed to start every minute schedule");

    TEST_ASSERT(sse_start_schedule(&loop, &every_5min, test_schedule_callback, &min5_data,
                                  &every_5min_expr, 2),
               "Failed to start every 5 minutes schedule");

    TEST_ASSERT(sse_start_schedule(&loop, &hourly, test_schedule_callback, &hourly_data,
                                  &hourly_expr, 3),
               "Failed to start hourly schedule");

    TEST_ASSERT(sse_start_schedule(&loop, &daily, test_schedule_callback, &daily_data,
                                  &daily_expr, 4),
               "Failed to start daily schedule");

    /* === Verify all watchers are active === */
    TEST_ASSERT(sse_watcher_active(&every_minute), "Every minute schedule should be active");
    TEST_ASSERT(sse_watcher_active(&every_5min), "Every 5min schedule should be active");
    TEST_ASSERT(sse_watcher_active(&hourly), "Hourly schedule should be active");
    TEST_ASSERT(sse_watcher_active(&daily), "Daily schedule should be active");

    /* === Verify schedule watcher setup === */
    TEST_ASSERT_EQ(SSE_TYPE_SCHEDULE, every_minute.type, "Wrong watcher type");
    TEST_ASSERT_NOT_NULL(every_minute.sdata, "Schedule watcher should have sdata");
    TEST_ASSERT(every_minute.fd >= 0, "Schedule watcher should have valid fd");

    sse_schedule_t *sched_data = (sse_schedule_t *)every_minute.sdata;
    TEST_ASSERT_NOT_NULL(sched_data->expr, "Schedule should have cron expression");
    TEST_ASSERT(!sched_data->clockchange, "Clock change should initially be false");

    /* === Test next fire time calculation === */
    time_t current_time = time(NULL);
    time_t next_fire = sse_getfire_schedule(&every_minute);

    TEST_ASSERT(next_fire > current_time, "Next fire time should be in the future");
    TEST_ASSERT(next_fire <= current_time + 60, "Next fire should be within next minute");

    /* === Test schedule modifications === */
    time_t new_fire_time = current_time + 30; /* 30 seconds from now */
    TEST_ASSERT(sse_modify_schedule(&every_minute, new_fire_time),
               "Failed to modify schedule fire time");

    /* === For realistic testing, we'll simulate short-term schedules === */
    /* Create a schedule that fires in a few seconds for testing */
    r = parse_cron("* * * * ?", &test_expr, NULL); /* Every second, but we'll modify it */
    TEST_ASSERT(r, "Failed to create test cron expression");

    sse_watcher_t test_schedule;
    struct test_callback_data test_data;
    test_callback_init(&test_data);

    TEST_ASSERT(sse_start_schedule(&loop, &test_schedule, test_schedule_callback, &test_data,
                                  &test_expr, 1),
               "Failed to start test schedule");

    TEST_ASSERT(sse_modify_schedule(&test_schedule, time(NULL) + 3),
               "Failed to modify test schedule");

    /* === Wait for test schedule to fire === */
    int max_iterations = 50; /* 50 * 100ms = 5 seconds max */
    while (max_iterations-- > 0 && !test_data.callback_called) {
        if (!sse_run(&loop, 100)) {
            break;
        }
    }

    TEST_ASSERT(test_data.callback_called, "Test schedule should have fired");
    TEST_ASSERT(test_data.last_event & SSE_READ, "Schedule should generate read event");

    /* === Test watcher stop/restart === */
    TEST_ASSERT(sse_stop_schedule(&hourly), "Failed to stop hourly schedule");
    TEST_ASSERT(!sse_watcher_active(&hourly), "Stopped schedule should be inactive");

    TEST_ASSERT(sse_restart_schedule(&hourly), "Failed to restart hourly schedule");
    TEST_ASSERT(sse_watcher_active(&hourly), "Restarted schedule should be active");

    /* === Cleanup === */
    sse_free(&loop);

    return true;
}

/* Test schedule error handling */
static bool test_schedule_watchers_error_handling(void) {
    sse_epoll_t loop;
    sse_watcher_t watcher1, watcher2;
    struct test_callback_data cb_data1, cb_data2;
    cron_t valid_expr = CRON_EXPR_ZERO;

    test_callback_init(&cb_data1);
    test_callback_init(&cb_data2);

    TEST_ASSERT(sse_new(&loop, 10), "Failed to create epoll loop");
    loop.running = true ;

    /* Create valid cron expression */
    int r = parse_cron("0 * * * ?", &valid_expr, NULL);
    TEST_ASSERT(r, "Failed to create valid cron expression");

    /* Start valid schedule watcher */
    TEST_ASSERT(sse_start_schedule(&loop, &watcher1, test_schedule_callback, &cb_data1,
                                  &valid_expr, 1),
               "Failed to start valid schedule watcher");

    /* Test error conditions */
    TEST_ASSERT(!sse_start_schedule(NULL, &watcher2, test_schedule_callback, &cb_data2,
                                   &valid_expr, 1),
               "Should fail with NULL epoll");

    TEST_ASSERT(!sse_start_schedule(&loop, NULL, test_schedule_callback, &cb_data2,
                                   &valid_expr, 1),
               "Should fail with NULL watcher");

    TEST_ASSERT(!sse_start_schedule(&loop, &watcher2, test_schedule_callback, &cb_data2,
                                   NULL, 1),
               "Should fail with NULL cron expression");

    /* Test invalid modification */
    TEST_ASSERT(!sse_modify_schedule(&watcher1, -1), "Should fail with negative time");

    /* Test that valid watcher still works */
    time_t future_time = time(NULL) + 2;
    TEST_ASSERT(sse_modify_schedule(&watcher1, future_time), "Valid modification should work");

    int max_iterations = 30;
    while (max_iterations-- > 0 && !cb_data1.callback_called) {
        if (!sse_run(&loop, 100)) {
            break;
        }
    }

    TEST_ASSERT(cb_data1.callback_called, "Valid schedule should work after error conditions");

    sse_free(&loop);

    return true;
}

/* Test different cron expression patterns */
static bool test_schedule_cron_patterns(void) {
    sse_epoll_t loop;
    sse_watcher_t specific_watcher, wildcard_watcher, range_watcher;
    struct test_callback_data specific_data, wildcard_data, range_data;

    cron_t specific_expr = CRON_EXPR_ZERO, wildcard_expr = CRON_EXPR_ZERO, range_expr = CRON_EXPR_ZERO;

    test_callback_init(&specific_data);
    test_callback_init(&wildcard_data);
    test_callback_init(&range_data);

    TEST_ASSERT(sse_new(&loop, 15), "Failed to create epoll loop");
    loop.running = true ;

    /* === Test different cron patterns === */

    /* Specific time pattern: "30 14 ? * 1" (2:30 PM on Mondays) */
    int r = parse_cron("30 14 ? * 1", &specific_expr, NULL);
    TEST_ASSERT(r, "Failed to parse specific time cron");

    /* Wildcard pattern: "* * * * *" (every minute) */
    r = parse_cron("* * * * ?", &wildcard_expr, NULL);
    TEST_ASSERT(r, "Failed to parse wildcard cron");

    /* Range pattern: "0 9-17 ? * 1-5" (every hour from 9 AM to 5 PM, weekdays) */
    r = parse_cron("0 9-17 ? * 1-5", &range_expr, NULL);
    TEST_ASSERT(r, "Failed to parse range cron");

    /* Start schedule watchers */
    TEST_ASSERT(sse_start_schedule(&loop, &specific_watcher, test_schedule_callback, &specific_data,
                                  &specific_expr, 1),
               "Failed to start specific schedule");

    TEST_ASSERT(sse_start_schedule(&loop, &wildcard_watcher, test_schedule_callback, &wildcard_data,
                                  &wildcard_expr, 2),
               "Failed to start wildcard schedule");

    TEST_ASSERT(sse_start_schedule(&loop, &range_watcher, test_schedule_callback, &range_data,
                                  &range_expr, 3),
               "Failed to start range schedule");

    /* === Test next fire time calculations === */
    time_t current = time(NULL);

    time_t specific_next = sse_getfire_schedule(&specific_watcher);
    time_t wildcard_next = sse_getfire_schedule(&wildcard_watcher);
    time_t range_next = sse_getfire_schedule(&range_watcher);

    TEST_ASSERT(specific_next > current, "Specific schedule should have future fire time");
    TEST_ASSERT(wildcard_next > current, "Wildcard schedule should have future fire time");
    TEST_ASSERT(range_next > current, "Range schedule should have future fire time");

    /* Wildcard should fire sooner than specific complex patterns */
    TEST_ASSERT(wildcard_next <= specific_next, "Wildcard should fire sooner than specific");

    /* === Test schedule modifications with different patterns === */
    time_t modified_time = current + 5;

    TEST_ASSERT(sse_modify_schedule(&wildcard_watcher, modified_time),
               "Failed to modify wildcard schedule");

    /* Wait for modified schedule to fire */
    int max_iterations = 60; /* 6 seconds */
    while (max_iterations-- > 0 && !wildcard_data.callback_called) {
        if (!sse_run(&loop, 100)) {
            break;
        }
    }

    TEST_ASSERT(wildcard_data.callback_called, "Modified wildcard schedule should fire");

    sse_free(&loop);

    return true;
}

/* Test schedule clock change handling */
static bool test_schedule_clock_change_handling(void) {
    sse_epoll_t loop;
    sse_watcher_t schedule_watcher;
    struct test_callback_data schedule_data;
    cron_t test_expr = CRON_EXPR_ZERO;

    test_callback_init(&schedule_data);

    TEST_ASSERT(sse_new(&loop, 15), "Failed to create epoll loop");
    loop.running = true ;

    /* Create a schedule that would be affected by clock changes */
    int r = parse_cron("* * * * ?", &test_expr, NULL); /* Every minute */
    TEST_ASSERT(r, "Failed to create test cron expression");

    TEST_ASSERT(sse_start_schedule(&loop, &schedule_watcher, test_schedule_callback, &schedule_data,
                                  &test_expr, 1),
               "Failed to start schedule for clock change test");

    /* Set to fire in near future */
    time_t fire_time = time(NULL) + 2;
    TEST_ASSERT(sse_modify_schedule(&schedule_watcher, fire_time),
               "Failed to modify schedule for clock change test");

    /* Wait for schedule to fire */
    int max_iterations = 50;
    while (max_iterations-- > 0 && !schedule_data.callback_called) {
        if (!sse_run(&loop, 100)) {
            break;
        }
    }

    TEST_ASSERT(schedule_data.callback_called, "Schedule should fire normally");

    /* Test that schedule data structure contains correct information */
    sse_schedule_t *sched_data = (sse_schedule_t *)schedule_watcher.sdata;
    TEST_ASSERT_NOT_NULL(sched_data, "Schedule should have data structure");
    TEST_ASSERT_NOT_NULL(sched_data->expr, "Schedule should have cron expression");

    /* Test getting next fire time */
    time_t next_fire = sse_getfire_schedule(&schedule_watcher);
    TEST_ASSERT(next_fire > time(NULL), "Next fire time should be in future");

    sse_free(&loop);

    return true;
}

TEST_SUITE_BEGIN("SSE Schedule Watcher Tests - Single Loop Multiple Schedules")
    RUN_TEST(test_schedule_watchers_comprehensive);
    RUN_TEST(test_schedule_watchers_error_handling);
    RUN_TEST(test_schedule_cron_patterns);
    RUN_TEST(test_schedule_clock_change_handling);
TEST_SUITE_END()