/* 
 * files.h
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
 
#ifndef FILES_H
#define FILES_H

#include <sys/types.h>

#include <skalibs/genalloc.h>

/** @Return the size of @filename
 * @Return -1 on failure and set errno*/
extern ssize_t file_get_size(const char* filename) ;

/** open @filename with read only permissions and write it on @buffile
 * of lenght @buffsize
 * @Return an fd in case of success
 * @Return -1 on fail and set errno*/
extern int file_openclose_readonly(char *buffile, char const *filename, size_t buffsize) ;

/** Create an empty @name file at @dst directory with mode @mode
 * @Return 1 on success
 * @Return 0 on fail and set errno*/
extern int file_create_empty (char const *dst, char const *name,int mode) ;

/** Write @name at @dst with @content as content
 * The previous contents of @name are destroyed even if the function fails
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int file_write_unsafe(char const *dst,char const *name,char const *content,size_t contlen) ;

/** Append @ga as type stralist with all files found in @srcdir
 * @Return 1 on success
 * @Return 0 on fail */
extern int file_get_fromdir(genalloc *ga,char const *srcdir) ;

#endif
