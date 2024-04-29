/*
 * stack.h
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <unistd.h>

#define _init_stack_(stk,__stk__maxlen__) char stk##__stk__store__[__stk__maxlen__ + 3] ; stack stk = { .s = stk##__stk__store__, .len = 0, .maxlen = __stk__maxlen__ + 2, .count = 0 } ;

#define STACK_ZERO { .s = 0, .len = 0, .maxlen = 0, .count = 0 }

#define FOREACH_STK(stk,pos) \
    stack_ref __stk__ref__ = stk ; \
    for (; (size_t)pos < __stk__ref__->len ; pos += (size_t)strlen(__stk__ref__->s + (size_t)pos) + 1)

typedef struct stack_s stack, *stack_ref ;
struct stack_s
{
    char *s ;
    size_t len ;
    size_t maxlen ;
    size_t count ;
} ;

/** See also, lexer.h for stack_string_XXX function */
extern uint8_t stack_add_g(stack *stk, char const *str) ;
extern uint8_t stack_add(stack *stk, char const *string, size_t len) ;
extern uint8_t stack_close(stack *stk) ;
extern uint8_t stack_copy(stack *stk, char const *string, size_t len) ;
#define stack_copy_g(stack, s) stack_copy(stack, (s), strlen(s))
#define stack_copy_stack(dst, src) stack_copy_g(dst, (src)->s)
extern size_t stack_count_element(stack *stk) ;
extern ssize_t stack_retrieve_element(stack *stk, char const *string) ;
extern uint8_t stack_remove_element(stack *stk, size_t index) ;
extern uint8_t stack_remove_element_g(stack *stk, char const *element) ;
extern uint8_t stack_string_rebuild(stack *stk) ;
extern uint8_t stack_insert(stack *stk, size_t offset, char const *s) ;
extern int stack_read_file(stack *stk, const char *path) ;
extern void stack_reset(stack *stk) ;
#endif
