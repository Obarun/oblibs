/* 
 * string.h
 * 
 * Copyright (c) 2018-2019 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */
 
#ifndef STRING_H
#define STRING_H

#include <sys/types.h>

#include <skalibs/genalloc.h>

#if !defined(nb_el)
    #define nb_el(x) (sizeof((x)) / sizeof((x)[0]))
#endif

/** remove all occurence of @occ into @str 
 * @Return 1 on success else 0 */
extern int obstr_trim(char *str, int occ) ;

/** compare @str1 to @str2
 * @Return 1 on success else 0*/
extern int obstr_equal(const char *str1, const char *str2) ;

/** replace all occurence of @a by @b into @str*/
extern void obstr_replace(char *str,int a,int b) ;

/** duplicate a string, allocate a memory
 * return a * to @str which can be used by free()
 * return 0 is case of failure*/
extern char *obstr_dup(const char *str) ;

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

/** Extracts tokens from a string.
 * Replaces strset which is not portable (missing on Solaris).
 * Copyright (c) 2001 by François Gouget <fgouget_at_codeweavers.com>
 * Modified by Eric Vidal <eric@obarun.org>
 * Modifies str to point to the first character after the token if one is
 * found, or 0 if one is not.
 * return pointer to the first token in str if str is not 0, 0 if
 * str is 0
 */
extern char *obstr_sep(char **str, const char *delims) ;

/** @Return the number of line into @str */
extern unsigned int get_nbline(char const *str, size_t len) ;

/** return the number of line of @str
 * copy each line into @ga*/
extern unsigned int get_nbline_ga(char const *str, size_t len, genalloc *ga) ;

/** copy @len bytes character from @str beginning at @index
 * @Return a new pointer to the copy
 * else @Return NULL */
extern char *obstr_sub(char const *str, unsigned int index, size_t len) ;

/** Check if the line is : empty,only contain \n,begin by # even
 * if the line contain space before it,line only containing space
 * @Return 0 on all this case else @Return 1 */ 
extern int get_wasted_line(char const *line) ;

/** Scan for space at the begin of @line
 * @Return n bytes space*/
extern unsigned int scan_isspace(char const *line) ;

/** Search for space between value on @line
 * remove all spaces behind and after the value
 * and store it on @ga which is a stralist type
 * @Return 1 on success
 * @Return 0 on fail*/
extern int clean_val(genalloc *ga, char const *line) ;

/** Same as clean_val but parse @line first and
 * replace all occurence of @delim in @line by ' '*/
extern int clean_val_wdelim(genalloc *ga,char const *line,int delim) ;

/** Append @str with @c if the last character
 * of @str differs of @c, terminate the @str
 * with 0. This function assume that @str are
 * enough allocation of memory to add 0 at the end*/
extern void str_append_check (char *str,char c) ;

/** Appends the basename of filename @path to @dst
 * @Return 1 on success
 * @Return 0 on fail and set errno */ 
extern int basename(char *dst,char const *path) ;

/** Appends the dirname of filename @path to @dst
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int dirname(char *dst,char const *path) ;

#endif
