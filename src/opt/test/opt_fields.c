/*
 * opt_fields.c -- tests for opt_fields / opt_list (layer 3)
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

#include <oblibs/opt.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message) ; \
            printf("      Line %d: %s\n", __LINE__, #condition) ; \
            exit(1) ; \
        } \
    } while(0)

#define TEST_START(name) printf("Running %s...", name)
#define TEST_END() printf(" PASS\n")

enum { F_DEPENDS = 0, F_GROUPS, F_NOSEED, F_ALLOW } ;

static opt_field_t const fields[] = {
    { .id = F_DEPENDS, .key = "depends", .hasval = 1 },
    { .id = F_GROUPS,  .key = "groups",  .hasval = 1 },
    { .id = F_NOSEED,  .key = "noseed",  .hasval = 0 },
    { .id = F_ALLOW,   .key = "allow",   .hasval = 1 },
} ;

/* recording context */
typedef struct rec_s rec_t ;
struct rec_s {
    int   ids[16] ;
    char  vals[16][64] ;  /* "" when val == 0 */
    int   n ;
} ;

static int record(int id, char const *key, char const *val, void *data)
{
    (void)key ;
    rec_t *r = data ;
    r->ids[r->n] = id ;
    if (val)
        strcpy(r->vals[r->n], val) ;
    else
        r->vals[r->n][0] = 0 ;
    r->n++ ;
    return 1 ;
}

static int always_fail(int id, char const *key, char const *val, void *data)
{
    (void)id ; (void)key ; (void)val ; (void)data ;
    return 0 ;
}

static void test_basic(void)
{
    TEST_START("test_basic") ;
    rec_t r = { .n = 0 } ;
    uint8_t ok = opt_fields("depends=a,b:groups=admin:noseed", ':', '=',
                            fields, OPT_COUNT(fields), &record, &r) ;
    TEST_ASSERT(ok, "parse succeeds") ;
    TEST_ASSERT(r.n == 3, "three fields") ;
    TEST_ASSERT(r.ids[0] == F_DEPENDS && !strcmp(r.vals[0], "a,b"), "depends=a,b") ;
    TEST_ASSERT(r.ids[1] == F_GROUPS && !strcmp(r.vals[1], "admin"), "groups=admin") ;
    TEST_ASSERT(r.ids[2] == F_NOSEED && r.vals[2][0] == 0, "noseed bare") ;
    TEST_END() ;
}

static void test_missing_value(void)
{
    TEST_START("test_missing_value") ;
    rec_t r = { .n = 0 } ;
    /* "depends" needs a value */
    uint8_t ok = opt_fields("depends", ':', '=', fields, OPT_COUNT(fields), &record, &r) ;
    TEST_ASSERT(!ok, "bare key for value-field fails") ;
    TEST_END() ;
}

static void test_value_on_bare(void)
{
    TEST_START("test_value_on_bare") ;
    rec_t r = { .n = 0 } ;
    /* "noseed" takes no value */
    uint8_t ok = opt_fields("noseed=x", ':', '=', fields, OPT_COUNT(fields), &record, &r) ;
    TEST_ASSERT(!ok, "value on bare-key field fails") ;
    TEST_END() ;
}

static void test_unknown_key(void)
{
    TEST_START("test_unknown_key") ;
    rec_t r = { .n = 0 } ;
    uint8_t ok = opt_fields("bogus=1", ':', '=', fields, OPT_COUNT(fields), &record, &r) ;
    TEST_ASSERT(!ok, "unknown key fails") ;
    TEST_END() ;
}

static void test_empty(void)
{
    TEST_START("test_empty") ;
    rec_t r = { .n = 0 } ;
    uint8_t ok = opt_fields("", ':', '=', fields, OPT_COUNT(fields), &record, &r) ;
    TEST_ASSERT(ok, "empty string is a no-op success") ;
    TEST_ASSERT(r.n == 0, "no fields") ;
    TEST_END() ;
}

static void test_callback_abort(void)
{
    TEST_START("test_callback_abort") ;
    uint8_t ok = opt_fields("groups=admin", ':', '=', fields, OPT_COUNT(fields), &always_fail, 0) ;
    TEST_ASSERT(!ok, "callback returning 0 aborts") ;
    TEST_END() ;
}

static void test_opt_list(void)
{
    TEST_START("test_opt_list") ;
    _alloc_strbuf_(sb, 64) ;
    TEST_ASSERT(opt_list(&sb, "a,b,c", ','), "split ok") ;
    TEST_ASSERT(sbl_count(&sb) == 3, "three elements") ;

    size_t pos = 0 ;
    int i = 0 ;
    char const *expect[] = { "a", "b", "c" } ;
    FOREACH_SBL(&sb, pos) {
        TEST_ASSERT(!strcmp(sb.s + pos, expect[i]), "element matches") ;
        i++ ;
    }
    TEST_END() ;
}

static void test_opt_list_none(void)
{
    TEST_START("test_opt_list_none") ;
    _alloc_strbuf_(sb, 64) ;
    TEST_ASSERT(opt_list(&sb, "none", ','), "split ok") ;
    TEST_ASSERT(sbl_count(&sb) == 1, "single element") ;
    TEST_ASSERT(!strcmp(sb.s, "none"), "is 'none'") ;
    TEST_END() ;
}

int main(void)
{
    test_basic() ;
    test_missing_value() ;
    test_value_on_bare() ;
    test_unknown_key() ;
    test_empty() ;
    test_callback_abort() ;
    test_opt_list() ;
    test_opt_list_none() ;
    printf("\nAll opt_fields tests passed.\n") ;
    return 0 ;
}
