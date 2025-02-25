/*
 * sastr.h
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

#ifndef OB_SASTR_H
#define OB_SASTR_H

#include <sys/stat.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>

#define FOREACH_SASTR(sa,pos) \
    stralloc_ref _saref_ = sa ; \
    for (; (size_t)pos < _saref_->len ; pos += (size_t)strlen(_saref_->s + (size_t)pos) + 1)

#define _cleanup_stralloc_ __attribute__((cleanup(stralloc_free)))

#define _alloc_sa_(sa) _cleanup_stralloc_ stralloc sa = STRALLOC_ZERO

/** get all compoments on @srcdir with mode @mode execpt string @exclude
 * and append @sa with the result in a chained stralloc
 * Note: srcdir must contain the last '/' e.g /etc/
 * @Return 0 on fail
 * @Return 1 on success */
extern int sastr_dir_get(stralloc *sa, char const *srcdir,char const **exclude,mode_t mode) ;

/** Same as sastr_dir_get but do it recursively
 * it use lstat() and returns information about the link itself in case of symlink*/
extern int sastr_dir_get_recursive(stralloc *sa, char const *srcdir,char const **exclude,mode_t mode, uint8_t include_path) ;

/** Seach @element on @scrcdir with mode @mode and append @sa
 * with the result in a chained stralloc if it found, with @scrcdir as
 * dirname if @include_path is positive. */
extern int sastr_dir_get_element(stralloc *sa, char const *srcdir,char const *element,mode_t mode,uint8_t include_path) ;

/** Compare @s in a chainled stralloc @sa
 * @Return -1 if not found
 * @Return the position of @s into the chained stralloc @sa */
extern ssize_t sastr_cmp(stralloc *sa,char const *s) ;

/** Rebuild a chained stralloc into a classic string
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_rebuild_in_oneline(stralloc *sa) ;

/** Rebuild a chained stralloc into x line
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_rebuild_in_nline(stralloc *sa) ;

/** Replace all occurence found of @regex by @by into @sa
 * @regex must be matched exactly.
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_replace(stralloc *sa,char const *regex,char const *by) ;

/** split the contents of @sa by line then
 * call sastr_replace for each line then
 * rebuild @sa appending each line with `\n` character.
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_replace_g(stralloc *sa,char const *regex,char const *by) ;

/** Append @sa with @string and call sastr_clean_element
 * escape ' \t\r\n' characters.
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_clean_string(stralloc *sa,char const *string) ;

/** Split each elements of @sa in a chained string
 * on multiple lines where element means everything
 * between '\n' character
 * e.g:
 * 'var=value'
 * 'var1=value1'
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_split_string_in_nline(stralloc *sa) ;

/** Find @str in @sa
 * @Return position of @str in @sa
 * else @Return -1 */
extern ssize_t sastr_find (stralloc *sa,char const *str) ;

/** Return the len of a chained stralloc */
extern size_t sastr_len(stralloc *sa) ;

/** Return the number of element of a chained stralloc */
extern size_t sastr_nelement(stralloc *sa) ;

/** Append @sa with @str
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_add_string(stralloc *sa, char const *str) ;

/** Reverse the contain of @sa
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_reverse(stralloc *sa) ;

/** find a specific element @el into chained stralloc @sa
 * @Return the number of the start of the string of the element if found
 * @Return -1 in other case */
extern ssize_t sastr_find_element_byid(stralloc *sa,unsigned int el) ;

/** Sort the chained @sa string by string comparaison
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_sort(stralloc *sa) ;

/** Remove a finding same elament on chained @sa.
 * WARNING: this function assume that @sa is already ordered (see sastr_sort)
 * Prefers to use the sastr_sortndrop_element() which is a convenient
 * function of sastr_sort() and sastr_drop_same_element().
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_drop_same_element(stralloc *sa) ;

/** Convenient function of sastr_sort() and sastr_drop_same_element()
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_sortndrop_element(stralloc *sa) ;

/** Remove any element from a chained @sa where element is a subtree.
 * e.g @sa contain /foo /foo/bar -> /foo/bar will be removed.
 * WARNING: This function assume that @sa is already ordered. Use sastr_sort()
 * function before.
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_dir_drop_child(stralloc *sa) ;

/** Copy a chained @sa to @str as chained too */
extern void sastr_to_char(char *str,stralloc *sa) ;

/** Remove @element from a chained @sa
 * @Return 1 on success
 * @Return 0 on fail */
extern int sastr_remove_element(stralloc *sa,char const *element) ;
#endif
