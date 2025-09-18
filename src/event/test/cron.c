#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <oblibs/sse.h>

void print_time(const char *label, time_t t) {
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
    fprintf(stderr, "%s: %s\n", label, buf);
}

time_t parse_time(const char *str) {
    struct tm tm = {0};
    tm.tm_isdst = -1;
    strptime(str, "%Y-%m-%d %H:%M:%S", &tm);
    return mktime(&tm);
}

void test_cron_expr(const char *expr_str, const char *tz, const char *start_str, const char *expected_str) {
    printf("==== Testing ====\n");
    printf("Expression : %s\n", expr_str);
    printf("Start time : %s\n", start_str);
    printf("Expected   : %s\n", expected_str);
    printf("Timezone   : %s\n", tz ? tz : "NULL");

    cron_t expr = CRON_EXPR_ZERO;

    int ret = parse_cron(expr_str, &expr, tz);
    assert(ret == 1);

    time_t t_start = parse_time(start_str);
    time_t t_expected = parse_time(expected_str);
    time_t t_next = cron_next(&expr, &t_start);

    assert(t_next == t_expected);
}

void test_cron_invalid(const char *expr_str) {
    printf("==== Testing invalid expression ====\n");
    printf("Expression : %s\n", expr_str);

    cron_t expr = CRON_EXPR_ZERO;
    int ret = parse_cron(expr_str, &expr, "UTC");
    assert(ret == 0);

}

int main() {
    printf("Starting tests...\n");
    test_cron_expr("0 * * * * ?", "UTC", "2025-08-21 10:02:00", "2025-08-21 10:03:00");
    test_cron_expr("0 0 * * * ?", "UTC", "2025-08-21 10:59:59", "2025-08-21 11:00:00");
    test_cron_expr("0 0 0 * * ?", "UTC", "2025-08-21 23:59:59", "2025-08-22 00:00:00");
    test_cron_expr("0 0 0 1 * ?", "UTC", "2025-08-21 10:00:00", "2025-09-01 00:00:00");
    test_cron_expr("0 0 0 ? * 2", "UTC", "2025-08-21 10:00:00", "2025-08-26 00:00:00");
    test_cron_expr("0 0 0 29 2 ?", "UTC", "2025-01-01 00:00:00", "2028-02-29 00:00:00");
    test_cron_expr("0 0/5 * * * ?", "UTC", "2025-08-21 10:07:00", "2025-08-21 10:10:00");
    test_cron_expr("* * * * * ?", "UTC", "2025-08-21 10:00:00", "2025-08-21 10:00:01");
    test_cron_expr("0 30 14 ? * 6", "UTC", "2025-08-21 10:00:00", "2025-08-23 14:30:00");
    test_cron_expr("0 0 0 1 1 ? *", "UTC", "2025-06-01 00:00:00", "2026-01-01 00:00:00");
    test_cron_expr("0 0 2 * * ?", "UTC", "2025-10-25 01:59:59", "2025-10-25 02:00:00");
    test_cron_expr("0 0/10 9-17 ? * 2-6", "UTC", "2025-08-21 17:00:00", "2025-08-21 17:10:00");
    test_cron_expr("0 0/10 9-17 ? * 2-6", "UTC", "2025-08-21 18:00:00", "2025-08-22 09:00:00");
    test_cron_expr("0 0 0 L * ?", "UTC", "2025-08-01 00:00:00", "2025-08-31 00:00:00");
    test_cron_expr("0 0 0 LW * ?", "UTC", "2025-08-01 00:00:00", "2025-08-29 00:00:00");
    test_cron_expr("0 0 0 15W * ?", "UTC", "2025-08-01 00:00:00", "2025-08-15 00:00:00");
    test_cron_expr("0 0 0 ? * 6L", "UTC", "2025-08-01 00:00:00", "2025-08-30 00:00:00");
    test_cron_expr("0 0 0 ? * 1#2", "UTC", "2025-08-01 00:00:00", "2025-08-11 00:00:00");
    test_cron_expr("0 0 12 1 4 ? 2026", "UTC", "2025-12-31 23:00:00", "2026-04-01 12:00:00");
    test_cron_expr("0 0/15 9-17 ? * 2-6", "UTC", "2025-08-21 10:22:00", "2025-08-21 10:30:00");

    test_cron_expr("0 0/30 8-16 ? * 1-5", "UTC", "2025-08-21 15:45:00", "2025-08-21 16:00:00");
    test_cron_expr("0 15,45 10-14 ? * 2-6", "UTC", "2025-08-21 14:30:00", "2025-08-21 14:45:00");
    test_cron_expr("0 0 0 ? * 3#1", "UTC", "2025-08-01 00:00:00", "2025-08-06 00:00:00");
    test_cron_expr("@weekly", "UTC", "2025-08-15 12:00:00", "2025-08-17 00:00:00");
    test_cron_expr("0 0 0 10W * ?", "UTC", "2025-08-01 00:00:00", "2025-08-11 00:00:00");
    test_cron_expr("0 0 12 1,15 * ?", "UTC", "2025-08-14 23:59:59", "2025-08-15 12:00:00");
    test_cron_expr("0 0 0 ? * 6L 2025", "UTC", "2025-12-01 00:00:00", "2025-12-27 00:00:00");
    test_cron_expr("0 0/5 9-17 ? * 2-5", "UTC", "2025-08-22 18:00:00", "2025-08-26 09:00:00");
    test_cron_expr("0 0 0 L-4 * ?", "UTC", "2025-08-01 00:00:00", "2025-08-04 00:00:00");
    test_cron_expr("0 0 0 ? * 1#4", "UTC", "2025-08-01 00:00:00", "2025-08-25 00:00:00");

    test_cron_invalid("0 0 0 ? * ?");
    test_cron_invalid("0 0 0 32 * ?");
    test_cron_invalid("0 61 * * * ?");
    test_cron_invalid("0 0 24 * * ?");
    test_cron_invalid("0 0 0 * 13 ?");
    test_cron_invalid("0 0 0 ? * 8");
    test_cron_invalid("INVALID");
    printf("All tests passed!\n");
    return 0;
}
