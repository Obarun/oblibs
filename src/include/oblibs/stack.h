/*
 * stack.h
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_STACK_H
#define OB_STACK_H

#include <string.h>
#include <stdint.h>

#define _init_stack_(stk,__stk__maxlen__) char __stk__store__[__stk__maxlen__ + 3] ; stack stk = { .s = __stk__store__, .len = 0, .maxlen = __stk__maxlen__ + 2 } ;

#define FOREACH_STK(stk,pos) \
    stack_ref __stk__ref__ = stk ; \
    for (; (size_t)pos < __stk__ref__->len ; pos += (size_t)strlen(__stk__ref__->s + (size_t)pos) + 1)

typedef struct stack_s stack, *stack_ref ;
struct stack_s
{
    char *s ;
    size_t len ;
    size_t maxlen ;
} ;

extern uint8_t stack_convert_string(stack *stk, char const *string, size_t len) ;
extern uint8_t stack_add_g(stack *stk, char const *str) ;
extern uint8_t stack_add(stack *stk, char const *string, size_t len) ;
extern uint8_t stack_close(stack *stk) ;
#endif
