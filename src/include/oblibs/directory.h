/* 
 * directory.h
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */


#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <sys/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

/** Check if the string @s begin by an '/' character
 * and check for space at the begin of the string @s
 * @Return the position of '/' on @s 
 * @Return -1 on failure*/
extern int dir_scan_absopath(char const *s) ;

/** Remove a slash if exist at the end of @str
 * and replace it by '\0' */
extern void dir_unslash (char *str) ;

/** Search for @search in directory @srcdir
 * also check if @search have the corresponding @mode
 * @Return 1 on success
 * @Return 0 on fail 
 * @Return -1 if @search haven't the corresponding @mode */
extern ssize_t dir_search (char const *srcdir, char const *search,mode_t mode) ;

/** Create @dst as directory with mode @mode
 * @Return 0 on fail, errno is set
 * @Return 1 on success */
extern int dir_create (char const *dst, int mode) ;

/** Create @name as sub-directory on @dst directory with mode @mode
 * If @dst doesn't exist it will created with mode @mode and do it
 * recursively like mkdir -p behaviour.
 * @Return 0 on fail, errno is set
 * @Return 1 on success 
 * @Return -1 if @name already exist as with different mode than directory*/
extern ssize_t dir_create_under(char const *dst, char const *name, int mode) ;

/** Create a tmp directory at @dst with @tname as template
 * @Return NULL and set errno
 * @Return return the value of tmp directory*/
extern char *dir_create_tmp(stralloc *sa,char const *dst, char const *tname) ;

/** get all compoments in @srcdir as mode @mode execpt string @exclude
 * put the result in genalloc as type stralist
 * @Return 0 on fail
 * @Return 1 on success */
extern int dir_get(genalloc *ga, char const *srcdir,char const *exclude,mode_t mode) ;

/** compare @src directory name with @dst, if @src name doesn't exist in @dst,save
 * @src name in @ga. Exclude from the searc @exclude.
 * @Return 0 on fail
 * @Return 1 on success */
extern int dir_cmp(char const *src, char const *dst,char const *exclude, genalloc *ga) ;
#endif
