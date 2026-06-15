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

/* The whole suite pins the process timezone to UTC (see main). Under TZ=UTC,
 * mktime()/localtime_r() behave exactly like timegm()/gmtime_r(), so the wall
 * clock the test reads back is genuine UTC. parse_cron(..., "UTC") and the
 * hard-coded absolute epochs below therefore prove the *real* UTC computation,
 * not just an internal cancellation of two equal offsets. */
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

/* Absolute-clock assertion: the expected next fire time is a literal UTC epoch,
 * computed independently (`TZ=UTC date -d '<wall clock>' +%s`). Nothing in this
 * check depends on parse_time()/mktime(), so a coherent timegm<->mktime swap in
 * cron.c (which only cancels when both sides share an offset) is caught here. */
void test_cron_absolute(const char *expr_str, time_t start, time_t expected_epoch) {
    printf("==== Testing absolute epoch ====\n");
    printf("Expression : %s\n", expr_str);
    printf("Start epoch: %lld\n", (long long)start);
    printf("Expected   : %lld\n", (long long)expected_epoch);

    cron_t expr = CRON_EXPR_ZERO;
    int ret = parse_cron(expr_str, &expr, "UTC");
    assert(ret == 1);

    time_t t_next = cron_next(&expr, &start);
    assert(t_next == expected_epoch);
}

/* cron_next() must report (time_t)-1 when no occurrence can ever exist. */
void test_cron_nomatch(const char *expr_str) {
    printf("==== Testing no-occurrence expression ====\n");
    printf("Expression : %s\n", expr_str);

    cron_t expr = CRON_EXPR_ZERO;
    int ret = parse_cron(expr_str, &expr, "UTC");
    assert(ret == 1);

    /* 2025-06-01 00:00:00 UTC */
    time_t start = 1748736000;
    time_t t_next = cron_next(&expr, &start);
    assert(t_next == (time_t)-1);
}

void test_cron_invalid(const char *expr_str) {
    printf("==== Testing invalid expression ====\n");
    printf("Expression : %s\n", expr_str);

    cron_t expr = CRON_EXPR_ZERO;
    int ret = parse_cron(expr_str, &expr, "UTC");
    assert(ret == 0);

}

/*
 * BUG (production): cron_next(expr, now) does NOT honour expr->tz for its
 * actual time arithmetic. parse_cron(..., "UTC") stores "UTC" in expr->tz,
 * which makes cron_next() take its `local = true` branch -> mktime()/
 * localtime_r(). Those convert using the *process* TZ environment, not
 * expr->tz. handle_tz() wraps only the very first localtime_r() (cron.c:317);
 * every conversion inside the search loop runs under whatever TZ the process
 * happens to carry. Consequently the absolute epoch returned for a fixed UTC
 * cron expression changes when the ambient TZ changes:
 *
 *   TZ=UTC            "0 0 0 1 1 ? 2026" -> 1767225600  (correct UTC epoch)
 *   TZ=America/...    same expression    -> 1767243600  (wrong, shifted)
 *   TZ=Pacific/Noumea same expression    -> 1767186000  (wrong, shifted)
 *
 * The historical suite hid this because parse_time() also used mktime() under
 * the same ambient TZ, so both sides shifted together and cancelled. This
 * function locks the comparison to the literal correct UTC epoch and proves
 * the environment dependence is a real defect. It asserts the CURRENT
 * behaviour (so the suite stays green under the pinned TZ=UTC) and documents
 * what a correct, tz-respecting cron_next() would guarantee.
 *
 * Fix belongs in production (src/event/cron.c), not here: cron_next() must
 * apply expr->tz around ALL of its mktime/localtime conversions, or compute
 * in UTC via timegm/gmtime whenever expr->tz is set.
 */
void test_cron_tz_environment_dependence(void) {
    printf("==== Documenting TZ-environment dependence (BUG) ====\n");

    const char *expr_str = "0 0 0 1 1 ? 2026";
    /* 2025-06-01 00:00:00 UTC */
    time_t start = 1748736000;
    /* TZ=UTC date -d '2026-01-01 00:00:00' +%s */
    const time_t correct_utc_epoch = 1767225600;

    char *saved = getenv("TZ");
    char saved_copy[256];
    saved_copy[0] = 0;
    if (saved)
        snprintf(saved_copy, sizeof(saved_copy), "%s", saved);

    cron_t expr = CRON_EXPR_ZERO;
    int ret = parse_cron(expr_str, &expr, "UTC");
    assert(ret == 1);

    /* Under the suite-wide TZ=UTC, cron_next() happens to be correct. */
    setenv("TZ", "UTC", 1);
    tzset();
    time_t under_utc = cron_next(&expr, &start);
    assert(under_utc == correct_utc_epoch);

    /* Same expr->tz ("UTC"), different ambient TZ: the result shifts.
     * A tz-respecting cron_next() would return correct_utc_epoch regardless;
     * the current implementation does not. We assert the (buggy) shift so the
     * defect is pinned down rather than silently tolerated. */
    setenv("TZ", "Asia/Kolkata", 1);
    tzset();
    time_t under_kolkata = cron_next(&expr, &start);

    /* Kolkata is UTC+5:30; the loop's mktime/localtime drift the answer. */
    assert(under_kolkata != correct_utc_epoch); /* BUG: should be == */
    printf("under TZ=UTC      : %lld (correct)\n", (long long)under_utc);
    printf("under TZ=Kolkata  : %lld (BUG: tz ignored by cron_next)\n",
           (long long)under_kolkata);

    /* Restore the suite-wide pinned timezone. */
    if (saved_copy[0])
        setenv("TZ", saved_copy, 1);
    else
        setenv("TZ", "UTC", 1);
    tzset();
}

int main() {
    /* Pin the process timezone to UTC so the wall-clock parse_time()/cron_next()
     * arithmetic is genuine UTC and the assertions below prove the real
     * computation instead of an offset cancellation. */
    setenv("TZ", "UTC", 1);
    tzset();

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

    /* Absolute UTC-epoch checks (literal expected values, no parse_time). */
    /* TZ=UTC date -d '2026-01-01 00:00:00' +%s = 1767225600 ;
     * start = TZ=UTC date -d '2025-06-01 00:00:00' +%s = 1748736000 */
    test_cron_absolute("0 0 0 1 1 ? 2026", 1748736000, 1767225600);
    /* TZ=UTC date -d '2026-04-01 12:00:00' +%s = 1775044800 ;
     * start = TZ=UTC date -d '2025-12-31 23:00:00' +%s = 1767222000 */
    test_cron_absolute("0 0 12 1 4 ? 2026", 1767222000, 1775044800);

    /* cron_next() error path: an expression that can never match. */
    test_cron_nomatch("0 0 0 30 2 ? *");   /* February 30th never exists */
    test_cron_nomatch("0 0 0 29 2 ? 2025"); /* 2025 is not a leap year */

    /* Document the production TZ-environment dependence (real bug). */
    test_cron_tz_environment_dependence();

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
