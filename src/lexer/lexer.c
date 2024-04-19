/*
 * lexer.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>

#include <oblibs/stack.h>
#include <oblibs/lexer.h>
#include <oblibs/log.h>

void lexer_reset(lexer_config *cfg)
{
    cfg->pos = cfg->found = cfg->invalue = cfg->count = 0, cfg->exitcode = 0 ;
}

static inline uint8_t lexer_loop(lexer_config *cfg, char const *regex, size_t len)
{
    if (cfg->style) {

        size_t pos = 0 ;
        int found = 0 ;

        char cmp[len + 1] ;
        memcpy(cmp, cfg->str + cfg->pos, len) ;
        cmp[len] = 0 ;

        /* the first and last character must match */
        if (cmp[0] != regex[0] || cmp[len-1] != regex[len-1])
            return 0 ;

        for (; pos < len ; pos++){

            if (cmp[pos] != regex[pos])
                break ;

            found++ ;
        }

        if (found && (pos == len))
            return 1 ;

    } else {

        size_t pos = 0 ;
        char c = cfg->str[cfg->pos] ;
        for (; pos < len ; pos++) {
            if (c == regex[pos])
                return 1 ;
        }
    }

    return 0 ;
}

static inline uint8_t lexer_switch_char(stack *stk, uint8_t state, lexer_config *cfg) {

    char c = cfg->str[cfg->pos] ;

    switch (state) {
        case LEXER_STATE_CHAR:
            if (cfg->invalue) {
                stack_add(stk, &c, 1) ;
                cfg->count++ ;
            }
            break;
        case LEXER_STATE_BEGIN_VALUE:
            cfg->invalue = 1 ;
            cfg->opos = cfg->pos ;
            if (cfg->kopen && !lexer_loop(cfg, cfg->skip, cfg->skiplen)) {
                stack_add(stk, &c, 1) ;
                cfg->count++ ;
            }
            break;
        case LEXER_STATE_IN_VALUE:
            if (cfg->invalue) {
                stack_add(stk, &c, 1) ;
                cfg->count++ ;
            }
            break;
        case LEXER_STATE_SKIP:
            break ;
        case LEXER_STATE_END:
            if (cfg->invalue) {
                if (cfg->kclose) {
                    stack_add(stk, &c, 1) ;
                    cfg->count++ ;
                }
                if (cfg->count)
                    stack_add(stk, "", 1) ;
                cfg->cpos = cfg->pos ;
                cfg->pos++ ;
                cfg->found = 1 ;
                cfg->count = 0 ;
                return 1 ;
            }
            break ;
        default:
            // Do nothing for LEXER_ACTION_OTHER states
            break;
    }
    return 0 ;
}

static inline uint8_t lexer_switch_string(stack *stk, uint8_t state, lexer_config *cfg)
{

    char c = cfg->str[cfg->pos] ;

    switch (state) {
        case LEXER_STATE_CHAR:
            if (cfg->invalue) {
                stack_add(stk, &c, 1) ;
                cfg->count++ ;
            }
            break;
        case LEXER_STATE_BEGIN_VALUE:
            cfg->invalue = 1 ;
            cfg->pos += cfg->olen - 1;
            cfg->opos = cfg->pos ;
            if (cfg->kopen) {
                stack_add(stk, cfg->open, cfg->olen) ;
                cfg->count++ ;
            }
            break;
        case LEXER_STATE_IN_VALUE:
            if (cfg->invalue) {
                stack_add(stk, &c, 1) ;
                cfg->count++ ;
            }
            break;
        case LEXER_STATE_SKIP:
            break ;
        case LEXER_STATE_END:
            if (cfg->invalue) {
                if (cfg->kclose) {
                    stack_add(stk, cfg->close, cfg->clen) ;
                    cfg->count++ ;
                }
                if (cfg->count)
                    stack_add(stk, "", 1) ;
                cfg->pos += cfg->clen - 1 ;
                cfg->cpos = cfg->pos ;
                cfg->pos++ ;
                cfg->found = 1 ;
                cfg->count = 0 ;
                return 1 ;
            }
            break ;
        default:
            // Do nothing for LEXER_ACTION_OTHER states
            break;
    }
    return 0 ;
}

static inline uint8_t lexer_cclass (char c, lexer_config *cfg)
{
    if (!c) return LEXER_ACTION_END ;

    if (!cfg->invalue) {
        if (lexer_loop(cfg, cfg->open, cfg->olen))
            return LEXER_ACTION_OPEN ;

    } else {

        if (lexer_loop(cfg, cfg->close, cfg->clen))
            return LEXER_ACTION_CLOSE ;

        uint8_t style = cfg->style ;
        cfg->style = 0 ;
        uint8_t r = lexer_loop(cfg, cfg->skip, cfg->skiplen) ;
        cfg->style = style ;
        if (r) return LEXER_ACTION_SKIP ;
    }

    return LEXER_ACTION_OTHER ;
}

int lexer(stack *stk, lexer_config *cfg) {

    static uint16_t const state_table[5][5] = {
        [LEXER_STATE_CHAR] = {
            [LEXER_ACTION_OPEN] = LEXER_STATE_BEGIN_VALUE,
            [LEXER_ACTION_CLOSE] = LEXER_STATE_END,
            [LEXER_ACTION_OTHER] = LEXER_STATE_CHAR,
            [LEXER_ACTION_SKIP] = LEXER_STATE_SKIP,
            [LEXER_ACTION_END] = LEXER_STATE_END
        },
        [LEXER_STATE_BEGIN_VALUE] = {
            [LEXER_ACTION_OPEN] = LEXER_STATE_IN_VALUE,
            [LEXER_ACTION_CLOSE] = LEXER_STATE_END,
            [LEXER_ACTION_OTHER] = LEXER_STATE_IN_VALUE,
            [LEXER_ACTION_SKIP] = LEXER_STATE_SKIP,
            [LEXER_ACTION_END] = LEXER_STATE_END
        },
        [LEXER_STATE_IN_VALUE] = {
            [LEXER_ACTION_OPEN] = LEXER_STATE_IN_VALUE,
            [LEXER_ACTION_CLOSE] = LEXER_STATE_END,
            [LEXER_ACTION_OTHER] = LEXER_STATE_IN_VALUE,
            [LEXER_ACTION_SKIP] = LEXER_STATE_SKIP,
            [LEXER_ACTION_END] = LEXER_STATE_END
        },
        [LEXER_STATE_SKIP] = {
            [LEXER_ACTION_OPEN] = LEXER_STATE_CHAR,
            [LEXER_ACTION_CLOSE] = LEXER_STATE_END,
            [LEXER_ACTION_OTHER] = LEXER_STATE_CHAR,
            [LEXER_ACTION_SKIP] = LEXER_STATE_SKIP,
            [LEXER_ACTION_END] = LEXER_STATE_END
        },
        [LEXER_STATE_END] = {
            [LEXER_ACTION_OPEN] = LEXER_STATE_END,
            [LEXER_ACTION_CLOSE] = LEXER_STATE_END,
            [LEXER_ACTION_OTHER] = LEXER_STATE_END,
            [LEXER_ACTION_SKIP] = LEXER_STATE_END,
            [LEXER_ACTION_END] = LEXER_STATE_END
        }
    };

    uint8_t state = LEXER_STATE_CHAR;

    if (!cfg->str || !cfg->slen)
        return cfg->exitcode ;

    while (cfg->pos < cfg->slen) {

        char c = cfg->str[cfg->pos] ;

        if (!cfg->opos && cfg->forceopen) {
            state = LEXER_STATE_BEGIN_VALUE ;
        } else {
            state = state_table[state][lexer_cclass(c, cfg)];
        }

        if (!cfg->style) {
            if (lexer_switch_char(stk, state, cfg))
                goto finish ;
        } else {
            if (lexer_switch_string(stk, state, cfg))
                goto finish ;
        }

        cfg->pos++;
    }

    if (state != LEXER_STATE_CHAR && state != LEXER_STATE_END && !cfg->forceclose)
        log_warn_return(cfg->exitcode, "parse process ended in middle of value") ;

    finish:
        cfg->invalue = 0 ;

    cfg->exitcode = 1 ;

    return cfg->exitcode ;
}