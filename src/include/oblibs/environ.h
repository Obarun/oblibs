/* 
 * environ.h
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

#include <execline/execline.h>

#define MAXVAR  50 
#define MAXFILE 100 
#define MAXENV 4095 
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
extern int environ_remove_unexport(stralloc *modifs,stralloc *sa) ;

/** This function is a simple combinaison
 * of environ_get_clean_env, environ_clean_nline and
 * environ_remove_unexport function.
 * @Return 1 on success
 * @Return 0 on fail 
 * @Return -1 if @modifs is empty meaning no environment variables*/
extern int environ_clean_envfile(stralloc *modifs,stralloc *sa) ;

/** Get of line from @sa
 * @Return the number of line on success
 * @Return -1 on fail */
extern unsigned int environ_get_num_of_line(stralloc *sa) ;

/** General function:
 * -it search if @src is a file or a directory
 * -it opens and reads the file
 * -it call environ_clean_envfile function
 * -It do the same for each file from @src in case of directory
 * @Return 1 on success
 * See environ_get_envfile_error function for failure */ 
extern int environ_get_envfile(stralloc *modifs,char const *src) ;

/**General function:
 * -it call environ_get_envfile function
 * -it merge @envp with @newenv and use @tmpenv as temporary array
 * @Return 1 on success
 * See environ_get_envfile_error function for failure */
extern int environ_get_envfile_n_merge(char const *src,char const *const *envp,char const **newenv, char *tmpenv) ;

/** handle environ_get_envfile and environ_get_envfile_n_merge error.
 * This function do not die, it use log_warnu? function
 * @Return -8 empty environment
 * @Return -7 invalid environment
 * @Return -6 environment too long
 * @Return -5 unable to open envdir
 * @Return -4 unable to parse and clean
 * @Return -3 unable get number of line
 * @Return -2 too many variable
 * @Return -1 too many files
 * @Return 0 merge environment */
extern void environ_get_envfile_error(int error, char const *envdir) ;

#endif
