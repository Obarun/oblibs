/*
 * directory.h
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


#ifndef OB_DIRECTORY_H
#define OB_DIRECTORY_H

#include <sys/types.h>
#include <dirent.h>

#include <oblibs/strbuf.h>

/** Close a directory stream without disturbing errno (for cleanup paths).
 * No EINTR retry: closedir may already have freed the DIR, so retrying would
 * risk a use-after-free. */
extern void dir_close (DIR *dir) ;

/** Create @dst as directory with mode @mode
 * @Return 0 on fail, errno is set
 * @Return 1 on success */
extern int dir_create (char const *dst, mode_t mode) ;

/** Create a tmp directory at @dst with @tname as template
 * @Return NULL and set errno
 * @Return return the value of tmp directory*/
extern char *dir_create_tmp(strbuf *sb, char const *dst, char const *tname) ;

/** Create @dst directory with mode @mode
 * If parent of @dst doesn't exist it will created with mode @mode and do it
 * recursively like mkdir -p behaviour.
 * @Return 0 on fail, errno is set
 * @Return 1 on success */
extern int dir_create_parent(char const *dst, mode_t mode) ;

/** Retrieve the absolute path of @dir and write the result in @dst
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int dir_beabsolute(char *dst, char const *dir) ;

/** Look if @child is a subtree of @parent
 * @Return 1 on success
 * @Return 0 on fail */
extern int dir_is_child(char const *parent, char const *child) ;

/** Remove the last '/' from @path if exist */
extern void dir_unslash(char *path) ;

/** Recursively remove @dir and everything under it (rm -rf semantics).
 * Symlinks are unlinked, never followed. Removing a nonexistent path is a
 * success. The directory listing of each level is read fully and the directory
 * closed before recursing, so no directory handle is held open across the
 * recursion.
 * @Return 1 on success, 0 on failure (errno set). */
extern int dir_destroy (char const *dir) ;

#endif
