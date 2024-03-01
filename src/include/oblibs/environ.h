/*
 * environ.h
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

#ifndef OB_ENVIRON_H
#define OB_ENVIRON_H

#include <sys/types.h>

#include <oblibs/stack.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/gccattributes.h>

#include <execline/execline.h>

#define MAXVAR  50
#define MAXFILE 20
#define MAXENV 8191
#define VAR_UNEXPORT '!'

typedef struct exlsn_s exlsn_t, *exlsn_t_ref ;
struct exlsn_s
{
  stralloc vars ;
  stralloc values ;
  genalloc data ; // array of elsubst
  stralloc modifs ;
} ;

#define EXLSN_ZERO { \
    .vars = STRALLOC_ZERO, .values = STRALLOC_ZERO, \
    .data = GENALLOC_ZERO, .modifs = STRALLOC_ZERO }

/** Please POSIX */
extern char **environ ;

/** Substitute value of @key with @val into @envp
 * @Return 1 on success
 * @Return 0 on fail*/
int environ_substitute(char const *key, char const *val,exlsn_t *info, char const *const *envp,int unexport) ;

/** Add @key with value @val at struct @info
 * @Return 1 on success
 * @Return 0 on fail*/
int environ_add_key_val (const char *key, const char *val, exlsn_t *info) ;

/** Remove empty and commented line
 * Replace the content of @sa with the
 * result of the process
 * @Return 1 on success
 * @Return 0 on fail */
extern int environ_get_clean_env(stralloc *sa) ;

/** Get the key (meaning everything before '=') and
 * remove any " \t\r" character from it.
 * Replace the content of @sa with the
 * result of the process. Add up @pos with the position
 * of the '=' found + 1.
 * @Return 1 on success
 * @Return 0 on fail */
extern int environ_get_key_nclean(stralloc *sa,size_t *pos) ;

/** Get the value (meaning everything between '=' and '\n').
 * Replace the content of @sa with the
 * result of the process. Add up @pos with the position
 * of the '\n' found + 1.
 * @Return 1 on success
 * @Return 0 on fail*/
extern int environ_get_val(stralloc *sa,size_t *pos) ;

/** Get the value of @key from @sa.
 * Replace the content of @sa with the
 * result of the process.
 * @Return 1 on success
 * @Return 0 on fail*/
extern int environ_get_val_of_key(stralloc *sa,char const *key) ;

/** Rebuild a previously splitted line in one line.
 * Replace the content of @sa with the
 * result of the process.
 * @Return 1 on success
 * @Return 0 on fail */
extern int environ_rebuild_line(stralloc *sa) ;

/** Split a line in multiple elements and remove
 * any extra ' \n\t\r' for each elements.
 * Replace the content of @sa with the
 * result of the process.
 * @Return 1 on success
 * @Return 0 on fail*/
extern int environ_clean_line(stralloc *sa) ;

/** Same as environ_clean_line but do the same process
 * for a string containing multiple line.
 * Replace the content of @sa with the
 * result of the process.
 * @Return 1 on success
 * @Return 0 on fail*/
extern int environ_clean_nline(stralloc *sa) ;

/** Replace '=!' by '=' from @sa and write the
 * result of the proccess into @modifs
 * @Return 1 on success
 * @Return 0 on fail */
extern int environ_remove_unexport(stralloc *modifs, char const *string, size_t len) ;

/** @PATH MUST POINT TO A DIRECTORY NOT A FILE.
 * For each file found at @path:
 * - it sort the list of the file found at @path.
 * - it clean each key=value pair like environ_get_clean_env do.
 * - it remove any empty and commented line.
 * - it check if MAXFILE and MAXENV is respected.
 * - it drop every same key=value pair from the list found.
 * For a same key=value found on multiple file,
 * the last occurence is kept.
 * @Return 1 on success
 * @Return 0 on fail
 * WARNING: this function do not return 0 if @path is empty
 * or files are empty. Simply check the len of @modifs after the call
 * of this function.*/
extern int environ_clean_envfile(stralloc *modifs,char const *path) ;

/** Call environ_clean_envfile and call the environ_remove_unexport function.
 * @Return 1 on success
 * @Return 0 on fail */
extern int environ_clean_envfile_unexport(stralloc *modifs,char const *path) ;

/** Get of line from @sa
 * @Return the number of line on success
 * @Return -1 on fail */
extern unsigned int environ_get_num_of_line(stralloc *sa) ;

/** check @keyvalue contain a '=' character
 * @Return 1 on success
 * @Return 0 on fail */
extern int environ_keyvalue_isvalid(char const *key_value) ;

/** Remove commented line from @sa where comment can be '#' or ';'
 * @Return 1 on success
 * @Return 0 on fail */
extern int environ_drop_commented_line(stralloc *sa) ;

/** Get commented line from @src and store it to @tokeep
 * where comment can be '#' or ';'.
 * @Return 1 on success
 * @Return 0 on fail */
extern int environ_get_commented_line(stralloc *tokeep,stralloc *src) ;

/** Compare @newlist to @list and remove from @list
 * a same key=value found at @newlist
 * @Return 1 on success
 * @Return 0 on fail */
extern int environ_drop_previous_same_key(stralloc *list,stralloc *newlist) ;

extern int environ_get_key(stack *stkkey, char const *str, char const *key) ;
extern int environ_clean_key(stack *stk) ;
extern int environ_parse_files(stralloc *sa, char const *file) ;

#endif
