/*
 * lexer.c
 *
 * Copyright (c) 2024-2025 Eric Vidal <eric@obarun.org>
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

static inline uint8_t lexer_loop(lexer_config *cfg, char const *regex, size_t len)
{
    if (!len)
        return 0 ;

    size_t pos = 0 ;

    if (cfg->style) {

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

        char c = cfg->str[cfg->pos] ;

        for (; pos < len ; pos++) {
            if (c == regex[pos])
                return 1 ;
        }
    }
    return 0 ;
}

static inline uint8_t lexer_do(stack *stk, uint8_t state, lexer_config *cfg) {

    char c = cfg->str[cfg->pos] ;

    switch (state) {
        case LEXER_STATE_CHAR:
            if (cfg->invalue) {

                if (!stack_add(stk, &c, 1))
                    return (errno = EOVERFLOW, 0) ;

                cfg->count++ ;
            }
            break;
        case LEXER_STATE_BEGIN_VALUE:
            cfg->invalue = 1 ;
            cfg->opos = cfg->pos ;

            if (cfg->style)
                cfg->pos += cfg->olen - 1 ;

            if (cfg->kopen && !lexer_loop(cfg, cfg->skip, cfg->skiplen)) {

                if (cfg->style) {
                    if (!stack_add(stk, cfg->open, cfg->olen))
                        return (errno = EOVERFLOW, 0) ;
                } else {
                    if (!stack_add(stk, &c, 1))
                        return (errno = EOVERFLOW, 0) ;
                }

                cfg->count++ ;
            }
            break;
        case LEXER_STATE_IN_VALUE:
            if (cfg->invalue) {
                if (!stack_add(stk, &c, 1))
                    return (errno = EOVERFLOW, 0) ;

                cfg->count++ ;
            }
            break;
        case LEXER_STATE_SKIP:
            break ;
        case LEXER_STATE_END:
            if (cfg->invalue) {


                if (cfg->kclose) {
                    if (cfg->style) {
                        if (!stack_add(stk, cfg->close, cfg->clen))
                            return (errno = EOVERFLOW, 0) ;
                    } else {
                        if (!stack_add(stk, &c, 1))
                            return (errno = EOVERFLOW, 0) ;
                    }
                    cfg->count++ ;
                }

                /** Lexer_trim will clean the string twice which create
                 * empty entry at the stack array.
                 *
                 * So, the lexer deliver the stack as it. It's the
                 * responsability to others functions to close or not
                 * the string after the call of the lexer() function.
                */
                // if (cfg->count) {
                //     if (!stack_add(stk, "", 1))
                //         return (errno = EOVERFLOW, 0) ;
                // }

                if (cfg->style)
                    cfg->pos += cfg->clen - 1 ;

                cfg->cpos = cfg->pos ;
                cfg->pos++ ;
                cfg->found = 1 ;
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

uint8_t lexer(stack *stk, lexer_config *cfg) {

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

    uint8_t state = LEXER_STATE_CHAR, err = 0, exit = 0 ;

    if (!*cfg->str || !cfg->slen)
        return cfg->exitcode ;

    err = errno ;

    while (cfg->pos < cfg->slen) {

        char c = cfg->str[cfg->pos] ;

        if (!cfg->invalue && cfg->forceopen)
            state = LEXER_STATE_BEGIN_VALUE ;
        else
            state = state_table[state][lexer_cclass(c, cfg)];

        errno = 0 ;
        exit = lexer_do(stk, state, cfg) ;

        if (exit) {
            goto finish ;
        } else {
            if (errno == EOVERFLOW)
                return cfg->exitcode ;
        }
        cfg->pos++ ;
    }

    errno = err ;

    /** EOF and not closed but was opened.
     * typically cfg->open = "whatever", cfg->close = "".
     * With the forceclose, we consider as a good one.
    */
    if (cfg->forceclose && cfg->invalue && !cfg->found)
        cfg->found = 1 ;

    if (!cfg->forceclose && cfg->invalue && !cfg->found) {
        log_warn_return(cfg->exitcode, "unmatched close ", cfg->style ? "string" : "character", " request from the opened ", cfg->style ? "string" : "character ", " : ", cfg->open) ;
    }

    finish:
        cfg->invalue = 0 ;
        cfg->exitcode = 1 ;

    return cfg->exitcode ;
}

uint8_t lexer_trim(stack *stk, lexer_config *cfg)
{
    if (!cfg->slen)
        return 0 ;
    lexer_config c = LEXER_CONFIG_ZERO ;
    lexer_cp_cfg(&c, cfg) ;
    c.str = cfg->str ;
    c.slen = strlen(cfg->str) ;

    while (c.pos < c.slen) {

        lexer_reset(&c) ;
        c.opos = c.cpos = 0 ;

        if (!lexer(stk, &c))
            return 0 ;

        if (c.count) {
            if (!stack_add(stk, "", 1))
                return 0 ;
        }

        if (c.found) {

            lexer_cp_cfg(cfg, &c) ;
            if (c.firstoccurence)
                break ;
        }
    }

    if (!stack_close(stk))
        log_warn_return(LOG_EXIT_ZERO, "stack overflow") ;

    stk->count = stack_count_element(stk) ;

    return 1 ;
}

uint8_t lexer_trim_with(stack *stk, const char *s, size_t len, lexer_config *cfg)
{
    if (!len)
        return 0 ;
    lexer_config c ;
    uint8_t r = 0 ;
    lexer_cp_cfg(&c, cfg) ;
    c.str = s ; c.slen = strlen(s) ;
    r = lexer_trim(stk, &c) ;
    if (c.found)
        lexer_cp_cfg(cfg, &c) ;

    return r ;
}

void lexer_cp_cfg(lexer_config *dst, lexer_config *src)
{
    dst->str = src->str ;
    dst->slen = src->slen ;
    dst->open = src->open ;
    dst->olen = src->olen ;
    dst->close = src->close ;
    dst->clen = src->clen ;
    dst->skip = src->skip ;
    dst->skiplen = src->skiplen ;
    dst->kopen = src->kopen ;
    dst->kclose = src->kclose ;
    dst->forceopen = src->forceopen ;
    dst->forceclose = src->forceclose ;
    dst->firstoccurence = src->firstoccurence ;
    dst->pos = src->pos ;
    dst->opos = src->opos ;
    dst->cpos = src->cpos ;
    dst->invalue = src->invalue ;
    dst->found = src->found ;
    dst->count = src->count ;
    dst->style = src->style ;
    dst->exitcode = src->exitcode ;
}

void lexer_reset(lexer_config *cfg)
{
    cfg->found = cfg->count = cfg->exitcode = 0 ;
}

void lexer_reset_hard(lexer_config *cfg)
{
    lexer_reset(cfg) ;
    cfg->pos = cfg->opos = cfg->cpos = 0 ;
}

lexer_config const lexer_config_zero = LEXER_CONFIG_ZERO ;

uint8_t lexer_trim_clean(stack *stk, const char *s)
{
    lexer_config cfg = LEXER_CFG_TRIM_CLEAN ;
    return lexer_trim_with_g(stk, s, &cfg) ;
}

uint8_t lexer_trim_unclean(stack *stk, const char *s)
{
    lexer_config cfg = LEXER_CFG_TRIM_UNCLEAN ;
    return lexer_trim_with_g(stk, s, &cfg) ;
}

uint8_t lexer_trim_line(stack *stk, const char *s)
{
    lexer_config cfg = LEXER_CFG_GET_LINE ;
    return lexer_trim_with_g(stk, s, &cfg) ;
}

uint8_t lexer_trim_with_delim(stack *stk, const char *str, unsigned int delim)
{
    lexer_config cfg = LEXER_CONFIG_ZERO ;
    char c[2] = { delim, 0 } ;
    cfg.close = c ; cfg.clen = 1 ;
    cfg.skip = " \t\r\n" ; cfg.skiplen = 4 ;
    cfg.forceclose = 1 ; cfg.forceopen = 1 ;
    cfg.kopen = 1 ; cfg.kclose = 0 ;

    return lexer_trim_with_g(stk,str,&cfg) ;
}