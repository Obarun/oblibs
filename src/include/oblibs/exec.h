/*
 * exec.h
 *
 * Copyright (c) 2018-2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * Process-image replacement (execve) with PATH resolution. Replaces the
 * skalibs/exec.h family (exec_ae / mexec_* / xmexec_*) with an explicit,
 * non-magic API: @file, @argv and @envp are always provided by the caller,
 * there is no implicit "file defaults to argv[0]" nor "envp defaults to
 * environ". Every function in this header REPLACES the current process image;
 * it never forks. On success it does not return.
 */

#ifndef OB_EXEC_H
#define OB_EXEC_H

#include <stddef.h>

#include <oblibs/attributes.h>

/* Fallback search path when PATH is unset in the current environment. */
#define EXEC_DEFAULTPATH "/usr/bin:/bin"

/** Replace the current process image with @file, executed with @argv and
 * @envp. If @file contains a '/', it is execve'd verbatim; otherwise it is
 * searched in the PATH of the CURRENT environment (getenv("PATH"), falling
 * back to EXEC_DEFAULTPATH) -- note this is the caller's PATH, independent of
 * the @envp handed to the executed program.
 * Returns only on failure, with errno set. */
extern void exec_path (char const *file, char const *const *argv, char const *const *envp) ;

/** Like exec_path but, before executing, merges the modif string @modif (of
 * length @modiflen, a sequence of null-terminated "KEY=value" to set / "KEY"
 * to unset) into @envp via environ_merge.
 * Returns only on failure, with errno set. */
extern void exec_path_merge (char const *file, char const *const *argv, char const *const *envp, char const *modif, size_t modiflen) ;

/** Like exec_path but dies on failure: exits 127 if errno is ENOENT
 * (command not found), 126 otherwise (found but not executable), matching the
 * POSIX shell convention. */
extern void exec_path_die (char const *file, char const *const *argv, char const *const *envp) attribute_noreturn ;

/** Like exec_path_merge but dies 127 (ENOENT) / 126 (other) on failure. */
extern void exec_path_merge_die (char const *file, char const *const *argv, char const *const *envp, char const *modif, size_t modiflen) attribute_noreturn ;

#endif
