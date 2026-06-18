/*
 * parse_cron.c
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

#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <oblibs/bits.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/log.h>

#include <oblibs/sse.h>

enum cron_class_e
{
    CRON_CLASS_END = 0,
    CRON_CLASS_WILDCARD, // 1
    CRON_CLASS_SLASH, // 2
    CRON_CLASS_COMMA, // 3
    CRON_CLASS_DASH, // 4
    CRON_CLASS_L, // 5
    CRON_CLASS_W, // 6
    CRON_CLASS_HASH, // 7
    CRON_CLASS_UPPER, // 8
    CRON_CLASS_LOWER, // 9
    CRON_CLASS_DIGIT, //10
    CRON_CLASS_EXCLAMATION, //11
    CRON_CLASS_OTHER, //12
    CRON_CLASS_ENDOFKEY //13
} ;
typedef enum cron_class_e cron_class_t ;

enum cron_state_e
{
    CRON_STATE_START,
    CRON_STATE_WILDCARD,
    CRON_STATE_SLASH,
    CRON_STATE_COMMA,
    CRON_STATE_DASH,
    CRON_STATE_L,
    CRON_STATE_W,
    CRON_STATE_HASH,
    CRON_STATE_UPPER,
    CRON_STATE_LOWER,
    CRON_STATE_DIGIT,
    CRON_STATE_EXCLAMATION,
    CRON_STATE_END,
    CRON_STATE_ERROR,
    CRON_STATE_ENDOFKEY
} ;
typedef enum cron_state_e cron_state_t ;

#define ACT_NONE        (1 << 1)
#define ACT_DIGIT       (1 << 2)
#define ACT_NAME_MIN    (1 << 3)
#define ACT_NAME_MAX    (1 << 4)
#define ACT_RANGE       (1 << 5)
#define ACT_STEP        (1 << 6)
#define ACT_WEEKDAY     (1 << 7)
#define ACT_LAST        (1 << 8)
#define ACT_LAST_MIN    (1 << 9)
#define ACT_LIST        (1 << 10)
#define ACT_SETMIN      (1 << 11)
#define ACT_SETMAX      (1 << 12)
#define ACT_WILDCARD    (1 << 13)
#define ACT_EXCLAMATION (1 << 14)
#define ACT_HASH        (1 << 15)
#define ACT_ERROR       (1 << 16)
#define ACT_RECORD      (1 << 17)

struct transition_s {
    unsigned char prev_state ;
    unsigned int action ;
} ;
typedef struct transition_s transition_t ;

static transition_t state_table[CRON_STATE_ENDOFKEY][CRON_CLASS_ENDOFKEY] ;

struct cron_context_e
{
    char *field ;
    cron_type_t type ;
    int min ;
    int max ;
    cron_t *e ;

    int parser_min ;
    int parser_max ;
    int parser_step ;
} ;
typedef struct cron_context_e cron_context_t ;

#define CRON_CONTEXT_ZERO { NULL, CRON_TYPE_OTHER, -1, -1, NULL, -1, -1, 0 }

static const unsigned char char_class[256] = {
    CRON_CLASS_END,   CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //8
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //16
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //24
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //32
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_HASH,  CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //40
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_WILDCARD, CRON_CLASS_OTHER, CRON_CLASS_COMMA, CRON_CLASS_DASH,  CRON_CLASS_OTHER, CRON_CLASS_SLASH,  //48
    CRON_CLASS_DIGIT, CRON_CLASS_DIGIT, CRON_CLASS_DIGIT,    CRON_CLASS_DIGIT, CRON_CLASS_DIGIT, CRON_CLASS_DIGIT, CRON_CLASS_DIGIT, CRON_CLASS_DIGIT, //56
    CRON_CLASS_DIGIT, CRON_CLASS_DIGIT, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_EXCLAMATION, //64
    CRON_CLASS_OTHER, CRON_CLASS_UPPER, CRON_CLASS_UPPER,    CRON_CLASS_UPPER, CRON_CLASS_UPPER, CRON_CLASS_UPPER, CRON_CLASS_UPPER, CRON_CLASS_UPPER, //72
    CRON_CLASS_UPPER, CRON_CLASS_UPPER, CRON_CLASS_UPPER,    CRON_CLASS_UPPER, CRON_CLASS_L,     CRON_CLASS_UPPER, CRON_CLASS_UPPER, CRON_CLASS_UPPER, //80
    CRON_CLASS_UPPER, CRON_CLASS_UPPER, CRON_CLASS_UPPER,    CRON_CLASS_UPPER, CRON_CLASS_UPPER, CRON_CLASS_UPPER, CRON_CLASS_UPPER, CRON_CLASS_W, //88
    CRON_CLASS_UPPER, CRON_CLASS_UPPER, CRON_CLASS_UPPER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //96
    CRON_CLASS_OTHER, CRON_CLASS_LOWER, CRON_CLASS_LOWER,    CRON_CLASS_LOWER, CRON_CLASS_LOWER, CRON_CLASS_LOWER, CRON_CLASS_LOWER, CRON_CLASS_LOWER, //104
    CRON_CLASS_LOWER, CRON_CLASS_LOWER, CRON_CLASS_LOWER,    CRON_CLASS_LOWER, CRON_CLASS_LOWER, CRON_CLASS_LOWER, CRON_CLASS_LOWER, CRON_CLASS_LOWER, //112
    CRON_CLASS_LOWER, CRON_CLASS_LOWER, CRON_CLASS_LOWER,    CRON_CLASS_LOWER, CRON_CLASS_LOWER, CRON_CLASS_LOWER, CRON_CLASS_LOWER, CRON_CLASS_LOWER, //120
    CRON_CLASS_LOWER, CRON_CLASS_LOWER, CRON_CLASS_LOWER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //128
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //136
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //144
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //152
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //160
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //168
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //176
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //184
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //192
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //200
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //208
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //216
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //224
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //232
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //240
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, //248
    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER,    CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, CRON_CLASS_OTHER, 0   //256
} ;

static inline char parse_next(char const *s, size_t *pos)
{
    char c = 0 ;
    size_t slen = strlen(s) ;
    if (*pos > slen) return -1 ;
    c = s[*pos] ;
    (*pos) += 1 ;
    return c ;
}

static int parse_name(const char *s, bool weekday)
{
    char tmp[4] ;
    size_t len = strlen(s) ;
    static const char *months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"} ;
    static const char *weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"} ;

    if (len > 3)
        return (errno = EOVERFLOW, -1) ;

    auto_strings(tmp, s) ;

    if (weekday) {
        for (int i = 0 ; i < 7 ; i++) {
            if (!strcasecmp(tmp, weekdays[i]))
                return i ;
        }
    }

    for (int i = 0 ; i < 13 ; i++) {
        if (!strcasecmp(tmp, months[i]))
            return i + 1 ;
    }

    return (errno = EINVAL, -1) ;
}

static int parse_number(const char *s, int min, int max, size_t *pos)
{
    const char *st = s ;
    int val = 0, ndig = 0 ;

    while(*st >= '0' && *st <= '9') {
        val = val * 10 + (*st - '0') ;
        ++st ;
        ++ndig ;
    }

    if (!ndig)
        return -1 ;

    if (val < min || val > max)
        return -1 ;

    (*pos) = ndig ;

    return val ;
}

void bitset_range(bitset8_t *field, uint32_t min, uint32_t max, int step)
{
    uint32_t pos = min ;
    step = step <= 0 ? 1 : step ;
    for (; pos < max ; pos += step)
        bitset8_set(field, pos) ;
}

static void set_range(cron_context_t *ctx)
{
    if (ctx->parser_min < 0)
        ctx->parser_min = ctx->min ;

    if (ctx->parser_max < 0) {
        if (ctx->parser_step < 0)
            ctx->parser_max = ctx->parser_min + 1 ;
        else ctx->parser_max = ctx->max ;
    }

    bitset8_t *b ;

    switch(ctx->type) {
        case CRON_TYPE_SECOND:
            b = &ctx->e->seconds ;
            break ;
        case CRON_TYPE_MINUTE:
            b = &ctx->e->minutes ;
            break ;
        case CRON_TYPE_HOUR:
            b = &ctx->e->hours ;
            break ;
        case CRON_TYPE_DOM:
            b = &ctx->e->dom ;
            break ;
        case CRON_TYPE_MONTH:
            b = &ctx->e->months ;
            break ;
        case CRON_TYPE_DOW:
            b = &ctx->e->dow ;
            break ;
        case CRON_TYPE_YEAR:
            ctx->parser_min -= 1900 ;
            ctx->parser_max -= 1900 ;
            if (ctx->parser_max > CRON_BITSET_YEARS)
                ctx->parser_max = CRON_BITSET_YEARS ;
            b = &ctx->e->years ;
            break ;
        case CRON_TYPE_ENDOFKEY:
        case CRON_TYPE_OTHER:
        default:
            return ;
    }

    bitset_range(b, ctx->parser_min, ctx->parser_max, ctx->parser_step) ;
}

static int solve_macro(const char *expression, strbuf *s)
{
    if (strcmp(expression, "@yearly") == 0 || strcmp(expression, "@annually") == 0) {
        auto_strings(s->s, "0 0 0 1 1 ?") ;
    } else if (strcmp(expression, "@monthly") == 0) {
        auto_strings(s->s, "0 0 0 1 * ?") ;
    } else if (strcmp(expression, "@weekly") == 0) {
        auto_strings(s->s, "0 0 0 ? * 0") ;
    } else if (strcmp(expression, "@daily") == 0 || strcmp(expression, "@midnight") == 0) {
        auto_strings(s->s, "0 0 0 * * ?") ;
    } else if (strcmp(expression, "@hourly") == 0) {
        auto_strings(s->s, "0 0 * * * ?") ;
    } else if (strcmp(expression, "@minutely") == 0) {
        auto_strings(s->s, "0 * * * * ?") ;
    } else if (strcmp(expression, "@secondly") == 0) {
        auto_strings(s->s, "* * * * * ?") ;
    } else {
        return 0 ;
    }

    return 1 ;
}

static void init_state_table(void)
{
    for (int s = 0; s < CRON_STATE_ENDOFKEY; ++s)
        for (int c = 0; c < CRON_CLASS_ENDOFKEY; ++c)
            state_table[s][c] = (transition_t){CRON_STATE_ERROR, ACT_ERROR} ;

    // CRON_STATE_START (actual state vs character found)
    state_table[CRON_STATE_START][CRON_CLASS_WILDCARD] = (transition_t){CRON_STATE_WILDCARD, ACT_WILDCARD} ;
    state_table[CRON_STATE_START][CRON_CLASS_DIGIT] = (transition_t){CRON_STATE_DIGIT, ACT_SETMIN} ;
    state_table[CRON_STATE_START][CRON_CLASS_L] = (transition_t){CRON_STATE_L, ACT_LAST} ;
    state_table[CRON_STATE_START][CRON_CLASS_W] = (transition_t){CRON_STATE_W, ACT_WEEKDAY} ;
    state_table[CRON_STATE_START][CRON_CLASS_UPPER] = (transition_t){CRON_STATE_UPPER, ACT_NAME_MIN} ;
    state_table[CRON_STATE_START][CRON_CLASS_LOWER] = (transition_t){CRON_STATE_LOWER, ACT_NAME_MIN} ;
    state_table[CRON_STATE_START][CRON_CLASS_EXCLAMATION] = (transition_t){CRON_STATE_EXCLAMATION, ACT_EXCLAMATION} ;

    // CRON_STATE_WILDCARD
    state_table[CRON_STATE_WILDCARD][CRON_CLASS_SLASH] = (transition_t){CRON_STATE_SLASH, ACT_STEP} ;

    // CRON_STATE_SLASH
    state_table[CRON_STATE_SLASH][CRON_CLASS_DIGIT] = (transition_t){CRON_STATE_DIGIT, ACT_STEP} ;
    state_table[CRON_STATE_SLASH][CRON_CLASS_COMMA] = (transition_t){CRON_STATE_COMMA, ACT_LIST} ;

    // CRON_STATE_COMMA
    state_table[CRON_STATE_COMMA][CRON_CLASS_DIGIT] = (transition_t){CRON_STATE_DIGIT, ACT_LIST} ;
    state_table[CRON_STATE_COMMA][CRON_CLASS_UPPER] = (transition_t){CRON_STATE_UPPER, ACT_NAME_MIN} ;
    state_table[CRON_STATE_COMMA][CRON_CLASS_LOWER] = (transition_t){CRON_STATE_LOWER, ACT_NAME_MIN} ;

    // CRON_STATE_DASH
    state_table[CRON_STATE_DASH][CRON_CLASS_WILDCARD] = (transition_t){CRON_STATE_WILDCARD, ACT_RANGE} ;
    state_table[CRON_STATE_DASH][CRON_CLASS_DIGIT] = (transition_t){CRON_STATE_DIGIT, ACT_SETMAX} ;
    state_table[CRON_STATE_DASH][CRON_CLASS_UPPER] = (transition_t){CRON_STATE_UPPER, ACT_NAME_MAX} ;
    state_table[CRON_STATE_DASH][CRON_CLASS_LOWER] = (transition_t){CRON_STATE_LOWER, ACT_NAME_MAX} ;

    // CRON_STATE_L
    state_table[CRON_STATE_L][CRON_CLASS_DASH] = (transition_t){CRON_STATE_DASH, ACT_LAST_MIN} ;
    state_table[CRON_STATE_L][CRON_CLASS_W] = (transition_t){CRON_STATE_W, ACT_WEEKDAY} ;

    // CRON_STATE_W
    state_table[CRON_STATE_W][CRON_CLASS_END] = (transition_t){CRON_STATE_END, ACT_WEEKDAY} ;

    // CRON_STATE_UPPER
    state_table[CRON_STATE_UPPER][CRON_CLASS_DASH] = (transition_t){CRON_STATE_DASH, ACT_NONE} ;
    state_table[CRON_STATE_UPPER][CRON_CLASS_SLASH] = (transition_t){CRON_STATE_SLASH, ACT_STEP} ;
    state_table[CRON_STATE_UPPER][CRON_CLASS_COMMA] = (transition_t){CRON_STATE_COMMA, ACT_LIST} ;

    // CRON_STATE_LOWER
    state_table[CRON_STATE_LOWER][CRON_CLASS_DASH] = (transition_t){CRON_STATE_DASH, ACT_NONE} ;
    state_table[CRON_STATE_LOWER][CRON_CLASS_SLASH] = (transition_t){CRON_STATE_SLASH, ACT_STEP} ;
    state_table[CRON_STATE_LOWER][CRON_CLASS_COMMA] = (transition_t){CRON_STATE_COMMA, ACT_LIST} ;

    // CRON_STATE_DIGIT
    state_table[CRON_STATE_DIGIT][CRON_CLASS_SLASH] = (transition_t){CRON_STATE_SLASH, ACT_STEP} ;
    state_table[CRON_STATE_DIGIT][CRON_CLASS_COMMA] = (transition_t){CRON_STATE_COMMA, ACT_LIST} ;
    state_table[CRON_STATE_DIGIT][CRON_CLASS_DASH]  = (transition_t){CRON_STATE_DASH, ACT_NONE} ;
    state_table[CRON_STATE_DIGIT][CRON_CLASS_L] = (transition_t){CRON_STATE_L, ACT_LAST} ;
    state_table[CRON_STATE_DIGIT][CRON_CLASS_W] = (transition_t){CRON_STATE_W, ACT_WEEKDAY} ;
    state_table[CRON_STATE_DIGIT][CRON_CLASS_HASH] = (transition_t){CRON_STATE_HASH, ACT_HASH} ;
}

static int get_number(cron_context_t *ctx, size_t *pos)
{
    size_t idx = 0 ;
    int n = parse_number(ctx->field + (*pos), ctx->min, ctx->max, &idx) ;
    (*pos) += idx ;
    return n ;
}

static int get_name(cron_context_t *ctx, size_t *pos)
{
    char s[4] ;
    memcpy(s, ctx->field + (*pos) - 1, 3) ;
    s[3] = 0 ;
    int n = parse_name(s, ctx->type == CRON_TYPE_MONTH ? false : true) ;
    (*pos) += 2 ;
    return n ;
}

static int parse_field(cron_context_t *ctx)
{
    int state = CRON_STATE_START ;
    size_t pos = 0, len = strlen(ctx->field) ;

    while(state != CRON_STATE_END) {

        unsigned char c = parse_next(ctx->field, &pos) ;

        unsigned char cls = char_class[c] ;
        transition_t t = state_table[state][cls] ;

        if (t.action & ACT_ERROR)
            flog_warn_return(LOG_EXIT_ZERO, "Parse error character: %c at pos: %zu", ctx->field[pos-1], pos) ;

        if (t.action & ACT_WILDCARD) {
            ctx->parser_min = ctx->min ;
            ctx->parser_max = ctx->max ;
        }

        if (t.action & ACT_SETMIN) {
            pos-- ;
            ctx->parser_min = get_number(ctx, &pos) ;
            if (ctx->parser_min < ctx->min || ctx->parser_min >= ctx->max || ctx->parser_min < 0)
                return (errno = EINVAL, 0) ;
        }

        if (t.action & ACT_SETMAX) {
            pos-- ;
            ctx->parser_max = get_number(ctx, &pos) ;
            if (ctx->parser_max > ctx->max || ctx->parser_max <= ctx->min || ctx->parser_max < 0)
                return (errno = EINVAL, -1) ;
            ctx->parser_max++ ; // 10-20 to get 20 we need 21, see set_range()
        }

        if (t.action & ACT_STEP) {
            ctx->parser_step = get_number(ctx, &pos) ;
            if (ctx->parser_step > ctx->max || ctx->parser_step < ctx->min || ctx->parser_step < 0)
                return (errno = EINVAL, 0) ;
        }

        if (t.action & ACT_RANGE) {
            ctx->parser_max = ctx->max ;
        }

        if (t.action & ACT_NAME_MIN) {
            ctx->parser_min = get_name(ctx, &pos) ;
            if (ctx->parser_min < 0)
                return (errno = EINVAL, 0) ;
        }

        if (t.action & ACT_NAME_MAX) {
             ctx->parser_max = get_name(ctx, &pos) ;
            if (ctx->parser_max < 0)
                return (errno = EINVAL, 0) ;
            ctx->parser_max++ ;
        }

        if (t.action & ACT_LIST) {
            set_range(ctx) ;
            ctx->parser_min = -1 ;
            ctx->parser_max = -1 ;
            ctx->parser_step = -1 ;
            t.prev_state = CRON_STATE_START ;
        }

        if (t.action & ACT_LAST) {
            ctx->e->last = true ;
            if (ctx->type == CRON_TYPE_DOW && ctx->parser_min == -1) {
                ctx->parser_min = 7 ;
            }
        }

        if (t.action & ACT_LAST_MIN) {
            if (ctx->type != CRON_TYPE_DOM)
                return (errno = EINVAL, 0) ;
            ctx->parser_min = get_number(ctx, &pos) ;
            if (ctx->parser_min < ctx->min || ctx->parser_min >= ctx->max || ctx->parser_min < 0)
                return (errno = EINVAL, 0) ;
        }

        if (t.action & ACT_WEEKDAY) {
            if (ctx->type != CRON_TYPE_DOM)
                return (errno = EINVAL, 0) ;
            ctx->e->wday = true ;
        }

        if (t.action & ACT_EXCLAMATION) {
            if (ctx->type != CRON_TYPE_DOM && ctx->type != CRON_TYPE_DOW)
                return (errno = EINVAL, 0) ;

            if (ctx->type == CRON_TYPE_DOM)
                ctx->e->dom_dow = true ;

            break ;
        }

        if (t.action & ACT_HASH) {

            if (ctx->parser_min < 0)
                return (errno = EINVAL, 0) ;
            ctx->e->hash = get_number(ctx, &pos) ;
            if (ctx->e->hash < 1 || ctx->e->hash > 5)
                return (errno = EINVAL, 0) ;
        }

        if (cls == CRON_CLASS_END || pos >= len) {
            if (ctx->e->last) {
                if (ctx->type == CRON_TYPE_DOM)
                    if (ctx->parser_min == -1 || ctx->e->wday) // L alone or LW
                        break ;
            }

            set_range(ctx) ;
            break ;
        }

        state = t.prev_state ;
    }

    return 1 ;
}

static void cron_init(cron_t *expr)
{
    expr->seconds = bitset8_init(CRON_BITSET_SECONDS) ;
    expr->minutes = bitset8_init(CRON_BITSET_MINUTES) ;
    expr->hours = bitset8_init(CRON_BITSET_HOURS) ;
    expr->dom = bitset8_init(CRON_BITSET_DOM) ;
    expr->months = bitset8_init(CRON_BITSET_MONTHS) ;
    expr->dow = bitset8_init(CRON_BITSET_DOW) ;
    expr->years = bitset8_init(CRON_BITSET_YEARS) ;
}

int parse_cron(const char *expression, cron_t *expr, const char *tz)
{
    if (!expression || !expr)
        return 0 ;

    size_t pos = 0 ;
    _alloc_strbuf_(raw, 256) ;   /* raw expression as a C string */
    _alloc_sbl_(e, 256) ;        /* cleaned fields, NUL-separated */
    cron_context_t ctx = CRON_CONTEXT_ZERO ;

    if (*expression == '@') {
        if (!solve_macro(expression, &raw)) {
            return (errno = EINVAL, 0) ;
        }
    } else {
        if (!strbuf_copys(&raw, expression) || !strbuf_uncounted(&raw))
            return (errno = ENOMEM, 0) ;
    }

    /* clean from @raw into @e (separate buffers: no aliasing) */
    if (!sbl_clean_string(&e, raw.s))
        return (errno = ENOMEM, 0) ;

    size_t ecount = sbl_count(&e) ;
    if (ecount < 5 || ecount > 7)
        return (errno = EINVAL, 0) ;

    init_state_table() ;

    cron_init(expr) ;

    memset(expr->tz, 0, sizeof(char) * 256) ;

    if (tz) {
        if (strlen(tz) > 255) {
            errno = ENAMETOOLONG ;
        } else {
            auto_strings(expr->tz, tz) ;
        }
    }

    ctx.e = expr ;

    uint8_t count = 0 ;

    FOREACH_SBL(&e, pos) {

        if (count > ecount)
            break ;

        ctx.field = e.s + pos ;
        ctx.parser_min = -1 ;
        ctx.parser_max = -1 ;
        ctx.parser_step = -1 ;

        // seconds
        if (count == 0) {
            if (ecount == 5) {
                bitset8_set(&ctx.e->seconds, 0) ;
                count++ ;
            } else {
                ctx.min = CRON_ZERO ;
                ctx.max = CRON_MAX_SECONDS ;
                ctx.type = CRON_TYPE_SECOND ;
                if (!parse_field(&ctx))
                    return 0 ;
                goto next  ;
            }
        }

        // minutes
        if (count == 1) {
            ctx.min = CRON_ZERO ;
            ctx.max = CRON_MAX_MINUTES ;
            ctx.type = CRON_TYPE_MINUTE ;
            if (!parse_field(&ctx))
                return 0 ;
            goto next  ;
        }

        // hour
        if (count == 2) {
            ctx.min = CRON_ZERO ;
            ctx.max = CRON_MAX_HOURS ;
            ctx.type = CRON_TYPE_HOUR ;
            if (!parse_field(&ctx))
                return 0 ;
            goto next  ;
        }

        // day of month
        if (count == 3) {
            ctx.min = 1 ;
            ctx.max = CRON_MAX_DOM ;
            ctx.type = CRON_TYPE_DOM ;
            if (!parse_field(&ctx))
                return 0 ;
            goto next  ;
        }

        // month
        if (count == 4) {
            ctx.min = 1 ;
            ctx.max = CRON_MAX_MONTHS + 1 ;
            ctx.type = CRON_TYPE_MONTH ;
            if (!parse_field(&ctx))
                return 0 ;
            goto next  ;
        }

        // day of week
        if (count == 5) {
            ctx.min =  CRON_ZERO ;
            ctx.max = CRON_MAX_DOW + 1 ;
            ctx.type = CRON_TYPE_DOW ;
            if (!parse_field(&ctx))
                return 0 ;
            if (ctx.e->dom.count && ctx.e->dow.count)
                log_warn_return(LOG_EXIT_ZERO, "you cannot use day of month and day of week field together -- set '?' for one of them") ;

            goto next  ;
        }

        // year
        if (count == 6) {
            ctx.min = CRON_MIN_YEARS ;
            ctx.max = CRON_MAX_YEARS ;
            ctx.type = CRON_TYPE_YEAR ;
            if (!parse_field(&ctx))
                return 0 ;
        }
        next:
        count++ ;
    }

    if (ecount < 7) {

        ctx.min = ctx.parser_min = CRON_MIN_YEARS ;
        ctx.max = ctx.parser_max = CRON_MAX_YEARS ;
        ctx.parser_step = -1 ;
        ctx.type = CRON_TYPE_YEAR ;
        set_range(&ctx) ;
    }

    // '?' mark is not set to day of month AND day of week.
    if ((!ctx.e->dom.count && !ctx.e->last) && !ctx.e->dow.count)
       return (errno = EINVAL, 0) ;

    if (ctx.e->dom.count && ctx.e->dow.count)
        return (errno = EINVAL, 0) ;

    return 1 ;
}



