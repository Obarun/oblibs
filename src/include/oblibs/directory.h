/* 
 * directory.h
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


#ifndef OB_DIRECTORY_H
#define OB_DIRECTORY_H

#include <sys/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/gccattributes.h>

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
extern int dir_create (char const *dst, mode_t mode) ;

/** Create a tmp directory at @dst with @tname as template
 * @Return NULL and set errno
 * @Return return the value of tmp directory*/
extern char *dir_create_tmp(stralloc *sa,char const *dst, char const *tname) ;

/** Create @dst directory with mode @mode
 * If parent of @dst doesn't exist it will created with mode @mode and do it
 * recursively like mkdir -p behaviour.
 * @Return 0 on fail, errno is set
 * @Return 1 on success */
extern int dir_create_parent(char const *dst, mode_t mode) ;

/** retrieve the absolute path of @dir and write the result in @dst
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int dir_beabsolute(char *dst,char const *dir) ;
#endif
