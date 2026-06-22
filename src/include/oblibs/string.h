/*
 * string.h
 *
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#ifndef OB_STRING_H
#define OB_STRING_H

#include <stddef.h>
#include <sys/types.h>

#include <oblibs/stream.h>
#include <oblibs/strbuf.h>

#if !defined(nb_el)
    #define nb_el(x) (sizeof((x)) / sizeof((x)[0]))
#endif

/** keep compatibility */
#if !defined(N_ELEMENTS)
    #define N_ELEMENTS(x) nb_el(x)
#endif

#define auto_string_from(dest,baselen,...) auto_string_builder(dest,baselen,(char const *[]){__VA_ARGS__, NULL})

#define auto_strings(dest,...) auto_string_builder(dest,0,(char const *[]){__VA_ARGS__, NULL})

#define auto_buf(b,...) auto_buf_builder(b,(char const *[]){__VA_ARGS__, NULL})

#define auto_strbuf(b,...) auto_strbuf_builder(b,(char const *[]){__VA_ARGS__, NULL})

/** count the length of the string until the @end
 * character was found.
 * @Return -1 if @end was not found else @Return length*/
extern ssize_t get_len_until(const char *str, char const end) ;

/** same as get_len_until but begin from the end of the
 * string which have @from as lenght*/
extern ssize_t get_rlen_until(const char *str, char const end,size_t from) ;

/** same as get_rlen_until but search for a string @search instead of a char
 * into @str.
 * each character must correspond e.g str-> ntpd-log search->-log */
extern ssize_t get_rstrlen_until(char const *str,char const *search) ;

/** count the length of the string until the @sepstart
 * character was found.
 * @Return -1 if @septart was not found before @sepend
 * @Return 0 if @sepend is not found
 * @Return length on success*/
extern ssize_t get_sep_before (char const *line, char const sepstart, char const sepend) ;

/** Appends the basename of filename @path to @dst
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int ob_basename(char *dst,char const *path) ;

/** Appends the dirname of filename @path to @dst
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int ob_dirname(char *dst,char const *path) ;

/** Append @dest with a list of string @str
 * starting at @baselen size. Only accept char const * as string
 * @dest is appended with 0 at end of the string*/
extern void auto_string_builder(char *dest,size_t baselen, char const *str[],...) ;

/** Append to stream @b a list of strings @str.
 * Each element of @str must be a NUL-terminated C string (the length is taken
 * with strlen); the array itself ends on a NULL pointer.
 * The stream is NOT flushed.
 * @Return 1 on success
 * @Return 0 on fail */
extern int auto_buf_builder(ostream *b, char const *str[], ...) ;

/** Append to strbuf @b a list of strings @str.
 * Each element of @str must be a NUL-terminated C string (the length is taken
 * with strlen); the array itself ends on a NULL pointer.
 * Leaves @b NUL-terminated (a usable C string) with @b->len excluding that NUL.
 * @Return 1 on success
 * @Return 0 on fail */
extern int auto_strbuf_builder(strbuf *b, char const *str[], ...) ;

/** Search string @search in string @str
 * @Return the count of the lenght corresponding
 * to the end of the @search string in @str on success
 * @Return -1 on fail */
extern int str_contain(char const *str, char const *search) ;

/** Offset of the first occurrence of @c in @s.
 * @Return the index of @c, or strlen(@s) if @c is not present */
extern size_t str_search(char const *s, int c) ;

/** Look if @str start with the regex @regex
 * @Return 1 on success
 * @Return 0 on fail */
extern int str_start_with(char const *str, char const *search) ;

#endif
