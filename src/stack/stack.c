/*
 * stack.c
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

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <oblibs/stack.h>
#include <oblibs/mill.h>
#include <oblibs/string.h>
#include <oblibs/lexer.h>
#include <oblibs/io.h>

#ifndef MAXENV
#define MAXENV 8192
#endif

void stack_reset(stack *stk)
{
    stk->len = stk->count = 0 ;
}

uint8_t stack_add(stack *stk, char const *str, size_t len)
{
    if (stk->maxlen >= MAXENV)
        return (errno = ENOBUFS, 0) ;

    if (stk->len + len > stk->maxlen)
        return (errno = EOVERFLOW, 0) ;
    memcpy(stk->s + stk->len, str, len) ;
    stk->len += len ;
    /*
     * a general hash table will be
     * better
     */
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
    stack_add(stk, "", 1) ;
    stk->len-- ;
    stk->count-- ; // stack_add modify the count integer, reverse it.
    return 1 ;
}

uint8_t stack_copy(stack *stk, char const *string, size_t len)
{
    if (stk->maxlen >= MAXENV)
        return (errno = ENOBUFS, 0) ;
    if (len > stk->maxlen)
        return (errno = EOVERFLOW, 0) ;
    memmove(stk->s, string, len) ;
    stk->len = len ;
    if (!stack_close(stk))
        return (errno = EOVERFLOW, 0) ;
    return 1 ;
}

size_t stack_count_element(stack *stk)
{
    size_t n = 0, pos = 0 ;
    FOREACH_STK(stk, pos)
        n++ ;
    return n ;
}

ssize_t stack_retrieve_element(stack *stk, char const *string)
{
    size_t pos = 0 ;
    /**TODO: maybe an hash table to avoid this ugly loop*/
    FOREACH_STK(stk, pos)
        if (!strcmp(stk->s + pos, string))
            return pos ;

    return -1 ;
}

uint8_t stack_remove_element(stack *stk, size_t index)
{
    size_t ilen = strlen(stk->s + index), next = index + ilen + 1, nextlen = strlen(stk->s + next) ;
    if (nextlen)
        memmove(stk->s + index, stk->s + next, stk->len - next) ;
    stk->len -= ilen + 1 ;
    if (!stack_close(stk)) return 0 ;
    /**TODO: removing it from hash table should be set here*/
    stk->count-- ;
    return 1 ;
}

uint8_t stack_remove_element_g(stack *stk, char const *element)
{
    ssize_t idx = stack_retrieve_element(stk, element) ;
    return idx < 0 ? 1 : stack_remove_element(stk, (size_t)idx) ;
}

uint8_t stack_string_rebuild(stack *stk)
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

    if (tmplen >= stk->maxlen)
         return (errno = EOVERFLOW, 0) ;

    memmove(stk->s, tmp, tmplen) ;

    stk->len = tmplen ;

    if (!stack_close(stk))
        return (errno = EOVERFLOW, 0) ;

    return 1 ;
}

uint8_t stack_insert(stack *stk, size_t offset, char const *s)
{
    if (offset > stk->len)
        return (errno = EINVAL, 0) ;
    size_t len = strlen(s) ;
    memmove(stk->s + offset + len, stk->s + offset, stk->len - offset) ;
    stk->len += len ;
    memmove(stk->s + offset, s, len) ;
    return 1 ;
}

int stack_read_file(stack *stk, const char *path)
{
    ssize_t fd = -1, e = errno ;
    ssize_t r ;
    struct stat st ;

    fd = io_open(path, O_RDONLY | O_CLOEXEC) ;
    if (fd < 0)
        return 0 ;

    if (fstat(fd, &st) < 0)
        goto err ;

    if (st.st_size >= (long int)stk->maxlen) {
        e = ENOBUFS ;
        goto err ;
    }

    r = io_read(stk->s, fd, (size_t)st.st_size) ;

    if (r <= 0 || r != st.st_size) {
        e = errno ;
        goto err ;
    }

    stk->len = (size_t)st.st_size ;
    if (!stack_close(stk))
        goto err ;
    close(fd) ;
    errno = e ;
    return 1 ;
    err:
        close(fd) ;
        errno = e ;
        return 0 ;
}