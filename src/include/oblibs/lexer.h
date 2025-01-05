/*
 * lexer.h
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

#ifndef OB_LEXER_H
#define OB_LEXER_H

#include <stddef.h>
#include <stdint.h>

#include <oblibs/stack.h>

// lexer state
enum {
    LEXER_STATE_CHAR,
    LEXER_STATE_BEGIN_VALUE,
    LEXER_STATE_IN_VALUE,
    LEXER_STATE_SKIP,
    LEXER_STATE_END,
};

enum {
    LEXER_ACTION_OPEN,
    LEXER_ACTION_CLOSE,
    LEXER_ACTION_OTHER,
    LEXER_ACTION_SKIP,
    LEXER_ACTION_END,
};

/* If the style is greater than 0, then "open" and "close" are considered as strings,
 * meaning that all characters must match their counterparts in the string to compare.
 * If the style is 0, then "open" and "close" are considered as separate characters,
 * one of them matching its counterpart is valid.
 * */

typedef struct lexer_config_s lexer_config, *lexer_config_ref ;
struct lexer_config_s
{
    // API
    const char *str ; // string to parse
    size_t slen ;
    char *open ; // char or string to begin the store process
    size_t olen ;
    char *close ; // char or string to end the store process
    size_t clen ;
    char *skip ; // characters to skip
    size_t skiplen ;
    uint8_t kopen ; // store the open character
    uint8_t kclose ; // store the close character
    uint8_t forceopen ; // boolean: begin to copy from the begin of str
    uint8_t forceclose ; // boolean: consider string as a closed at the end of string
    uint8_t firstoccurence ; // boolean: stop the parsing process at first occurence find
    // inner usage
    uint32_t pos ; // current position
    uint32_t opos ; // open character position. if style > 0 its represent the last character of the string to open
    uint32_t cpos ; // close character position. if style > 0 its represent the last character of the string to close
    uint8_t invalue ; // boolean: state of the parser -> actually keeping character between open and close
    uint32_t found ; // boolean: get a tmp
    uint32_t count ; // count of character/string added
    uint8_t style ; // boolean: style of the open and close format to apply: 0 -> char or 1 -> string
    uint8_t exitcode ; // exit code of the parser
} ;

#define LEXER_CONFIG_ZERO { \
    .str = 0,\
    .slen = 0,\
    .open = 0,\
    .olen = 0,\
    .close = 0,\
    .clen = 0,\
    .skip = 0,\
    .skiplen = 0,\
    .kopen = 1,\
    .kclose = 1,\
    .forceopen = 0,\
    .forceclose = 0,\
    .firstoccurence = 0,\
    .pos = 0,\
    .opos = 0,\
    .cpos = 0,\
    .invalue = 0,\
    .found = 0,\
    .count = 0,\
    .style = 0,\
    .exitcode = 0,\
}

extern uint8_t lexer(stack *stk, lexer_config *cfg)  ;
extern uint8_t lexer_trim(stack *stk, lexer_config *cfg) ;
extern uint8_t lexer_trim_with(stack *stk, const char *s, size_t len, lexer_config *cfg) ;
#define lexer_trim_with_g(stk,s,cfg) lexer_trim_with(stk, (s), strlen(s), (cfg))
extern void lexer_cp_cfg(lexer_config *dst, lexer_config *src) ;
/** set cfg->found = cfg->count = cfg->exitcode = 0 */
extern void lexer_reset(lexer_config *cfg) ;
extern void lexer_reset_hard(lexer_config *cfg) ;
extern lexer_config const lexer_config_zero ;
/** common configuration */
// escape ' \t\r\n'
extern lexer_config LEXER_CFG_TRIM_CLEAN ;
// do not escape anything
extern lexer_config LEXER_CFG_TRIM_UNCLEAN ;
// trim by line
extern lexer_config LEXER_CFG_GET_LINE ;

// use LEXER_CFG_TRIM_CLEAN configuration
extern uint8_t lexer_trim_clean(stack *stk, const char *s) ;
// use LEXER_CFG_TRIM_UNCLEAN configuration
extern uint8_t lexer_trim_unclean(stack *stk, const char *s) ;
// use LEXER_CFG_GET_LINE configuration
extern uint8_t lexer_trim_line(stack *stk, const char *s) ;
/** trim @str with @delim as delimiter
 * it also escapr ' \t\r\n'*/
extern uint8_t lexer_trim_with_delim(stack *stk, const char *str, unsigned int delim) ;

#define stack_string_clean(stk,s) lexer_trim_clean(stk,(s))
#define stack_string_unclean(stk,s) lexer_trim_unclean(stk,(s))
#define stack_string_trimline(stk,s) lexer_trim_line(stk,(s))

#endif
