/*
 * stack.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <oblibs/stack.h>
#include <oblibs/mill.h>
#include <oblibs/string.h>

uint8_t stack_add(stack *stk, char const *str, size_t len)
{
    if (stk->len + len > stk->maxlen)
        { errno = EOVERFLOW ; return 0 ; }
    memcpy(stk->s + stk->len, str, len) ;
    stk->len += len ;
    stk->count++ ;
    return 1 ;
}

uint8_t stack_add_g(stack *stk, char const *str)
{
    size_t len = strlen(str) ;
    return stack_add(stk, str, len + 1) ;
}

uint8_t stack_close(stack *stk)
{
    if (stk->len + 1 > stk->maxlen)
        { errno = EOVERFLOW ; return 0 ; }
    stack_add(stk, "", 1) ;
    stk->len-- ;
    stk->count-- ; // stack_add modify the count integer, reverse it.
    return 1 ;
}

uint8_t stack_copy(stack *stk, char const *string, size_t len)
{
    if (len > stk->maxlen)
        { errno = EOVERFLOW ; return 0 ; }
    memmove(stk->s, string, len) ;
    stk->len = len ;
    return 1 ;
}

size_t stack_count(stack *stk)
{
    size_t n = 0, pos = 0 ;
    FOREACH_STK(stk, pos)
        n++ ;
    return n ;
}

/**
 * \t\r\n are not escaped
 * */
uint8_t stack_convert_tostring(stack *stk)
{
    if (!stk->len)
        return 0 ;
    size_t pos = 0, tmplen = 0 ;
    char tmp[stk->maxlen + 1] ;
    memset(tmp, 0, (stk->maxlen + 1) * sizeof(char)) ;
    FOREACH_STK(stk, pos)
        auto_strings(tmp + strlen(tmp), stk->s + pos, " ") ;
    tmplen = strlen(tmp) ;
    tmp[tmplen - 1] = 0 ;
    memmove(stk->s, tmp, tmplen) ;
    stk->len = tmplen ;
    if (!stack_close(stk)) return 0 ;
    return 1 ;
}

/**
 * \t\r\n are not escaped
 * */
uint8_t stack_convert_string(stack *stk, char const *string, size_t len)
{
    if (!len)
        return 0 ;
    int r ;
    _init_stack_(tmp, len + 1) ;
    wild_zero_all(&MILL_SPLIT_ELEMENT) ;
    r = mill_string(&tmp, string, len, &MILL_SPLIT_ELEMENT) ;
    if (r == -1 || !r) return 0 ;
    if (!stack_close(&tmp)) return 0 ;
    memmove(stk->s, tmp.s, tmp.len) ;
    stk->len = tmp.len ;
    if (!stack_close(stk)) return 0 ;
    stk->count = stack_count(stk) ;
    return 1 ;
}

uint8_t stack_convert_string_g(stack *stk, char const *string)
{
    size_t len = strlen(string) ;
    return stack_convert_string(stk, string, len + 1) ;
}

ssize_t stack_retrieve_element(stack *stk, char const *string)
{
    size_t pos = 0 ;
    FOREACH_STK(stk, pos)
        if (!strcmp(stk->s + pos, string))
            return pos ;

    return -1 ;
}

size_t stack_count_element(stack *stk)
{
    return stk->count ;
}

uint8_t stack_remove_element(stack *stk, size_t index)
{
    size_t ilen = strlen(stk->s + index), next = index + ilen + 1, nextlen = strlen(stk->s + next) ;
    if (nextlen)
        memmove(stk->s + index, stk->s + next, stk->len - next) ;
    stk->len -= ilen + 1 ;
    if (!stack_close(stk)) return 0 ;
    stk->count-- ;
    return 1 ;
}

uint8_t stack_remove_element_g(stack *stk, char const *element)
{
    ssize_t idx = stack_retrieve_element(stk, element) ;
    return idx < 0 ? 1 : stack_remove_element(stk, (size_t)idx) ;
}
