/*
 * string.h
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_STRING_H
#define OB_STRING_H

#include <sys/types.h>
#include <stdarg.h>
#include <stdint.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/gccattributes.h>

#if !defined(nb_el)
    #define nb_el(x) (sizeof((x)) / sizeof((x)[0]))
#endif

/** keep compatibility */
#if !defined(N_ELEMENTS)
    #define N_ELEMENTS(x) nb_el(x)
#endif

#define auto_string_from(dest,baselen,...) auto_string_builder(dest,baselen,(char const *[]){__VA_ARGS__, NULL})

#define auto_strings(dest,...) auto_string_builder(dest,0,(char const *[]){__VA_ARGS__, NULL})

#define auto_stra(sa,...) auto_stra_builder(sa,(char const *[]){__VA_ARGS__, NULL})

#define auto_buf(b,...) auto_buf_builder(b,(char const *[]){__VA_ARGS__, NULL})

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

/** @Return the number of line into @str */
extern unsigned int get_nbline(char const *str, size_t len) ;

/** Check if the line is : empty,only contain \n,begin by # even
 * if the line contain space before it,line only containing space
 * @Return 0 on all this case else @Return 1 */
extern int get_wasted_line(char const *line) ;

/** Scan for space at the begin of @line
 * @Return n bytes space*/
extern unsigned int scan_isspace(char const *line) ;

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

/** Append @sa with a list of string @str
 * Only accept char const * as string
 * @sa is appended with 0 at the end of the string.
 * @Return 1 on success
 * @Return 0 on fail */
extern int auto_stra_builder(stralloc *sa,char const *str[],...) ;

/** Append @b with a list of string @str.
 * The buffer is NOT flushed.
 * @Return 1 on success
 * @Return 0 on fail */
extern int auto_buf_builder(buffer *b, char const *str[], ...) ;

/** Scan @str for digit between dot where @ndot is the number
 * of dot expected e.g ndot = 2 -> 0.1.0, ndot = 3 > 1.0.1.0,...
 * append @sa with the result.
 * @Return 1 on success
 * @Return 0 on fail
 * @Return -1 system failure */
extern int version_scan(stralloc *sa, char const *str,uint8_t ndot) ;

/** This function assume that @one and @two only contain numeric
 * value and it's on the form digit.digit.digit....
 * @ndot is the number of dot on the format e.g 1.0.1
 * Follow strcmp format return here:
 * @Return 1 if one > two
 * @Return 0 if one == two
 * @Return -1 if one < two
 * @Return -2 on system error */
extern int version_cmp(char const  *a, char const *b, uint8_t ndot) ;

/** Search string @search in string @str
 * @Return the count of the lenght corresponding
 * to the end of the @search string in @str on success
 * @Return -1 on fail */
extern int str_contain(char const *str, char const *search) ;

/** Look if @str start with the regex @regex
 * @Return 1 on success
 * @Return 0 on fail */
extern int str_start_with(char const *str, char const *search) ;

#endif
