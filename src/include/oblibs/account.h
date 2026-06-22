/*
 * account.h
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 *
 * @brief User and group account lookup helpers.
 *
 * This module wraps the standard `<pwd.h>` and `<grp.h>` name-service lookups
 * (`getpwnam`, `getpwuid`, `getgrnam`, `getgrgid`) with a uniform interface that
 * resolves between account names and numeric ids. Every lookup function saves
 * the caller's `errno` on entry and restores it on success, so a successful call
 * never clobbers `errno`; on failure `errno` carries the cause. When the
 * underlying NSS lookup returns no entry but leaves `errno` untouched, these
 * helpers normalise the failure to `ESRCH`.
 *
 * Beware the two return conventions in this header: the *user uid / group gid*
 * lookups (`get_uidbyname`, `get_gidbyname`, `get_gidbygroup`) report failure
 * with -1, while the *name-into-strbuf* lookups (`get_namebyuid`,
 * `get_groupbygid`) and `scan_uidlist` report failure with 0. Read each
 * function's exit contract rather than assuming a single convention.
 */

#ifndef OB_ACCOUNT_H
#define OB_ACCOUNT_H

#include <sys/types.h>

#include <oblibs/strbuf.h>

/**
 * @brief Resolve a user name to its uid.
 *
 * Looks up @name in the user database with `getpwnam()` and, on success, stores
 * the entry's `pw_uid` into @uid. The caller's `errno` is saved on entry and
 * restored before returning 1, so a successful call leaves `errno` unchanged.
 *
 * @param[in]  name User name to resolve. Must not be NULL.
 * @param[out] uid  Receives the resolved uid on success. Untouched on failure.
 *
 * @return 1 on success (@uid set, `errno` preserved).
 * @return -1 on failure (@uid untouched). `errno` is set to `EINVAL` when @name
 *         is NULL, to the value left by `getpwnam()` when the lookup failed with
 *         an error, or to `ESRCH` when `getpwnam()` returned NULL without setting
 *         `errno` (no such user).
 */
extern int get_uidbyname(char const *name,uid_t *uid) ;

/**
 * @brief Resolve a uid to its user name.
 *
 * Looks up @uid with `getpwuid()` and, on success, appends the entry's `pw_name`
 * to @name via `strbuf_cats()`. The caller's `errno` is saved on entry and
 * restored before returning 1.
 *
 * @param[in]     uid  User id to resolve.
 * @param[in,out] name Strbuf the resolved name is appended to (existing content
 *                     is kept). Must be a valid strbuf; not modified on failure.
 *
 * @return 1 on success (name appended to @name).
 * @return 0 on failure. `errno` is set to the value left by `getpwuid()` when the
 *         lookup failed with an error, to `ESRCH` when `getpwuid()` returned NULL
 *         without setting `errno` (no such uid), or to `ERANGE` when appending the
 *         name to @name failed (allocation failure / size overflow). On the
 *         lookup-failure paths @name is untouched; on the append-failure path
 *         @name may hold a partial result.
 */
extern int get_namebyuid(uid_t uid,strbuf *name) ;

/**
 * @brief Resolve a user name to its primary gid.
 *
 * Looks up @name with `getpwnam()` and, on success, stores the entry's `pw_gid`
 * (the user's primary group) into @gid. The caller's `errno` is saved on entry
 * and restored before returning 1.
 *
 * @param[in]  name User name to resolve. Must not be NULL.
 * @param[out] gid  Receives the user's primary gid on success. Untouched on
 *                  failure.
 *
 * @return 1 on success (@gid set, `errno` preserved).
 * @return -1 on failure (@gid untouched). `errno` is set to `EINVAL` when @name
 *         is NULL, to the value left by `getpwnam()` when the lookup failed with
 *         an error, or to `ESRCH` when `getpwnam()` returned NULL without setting
 *         `errno` (no such user).
 */
extern int get_gidbyname(char const *name,gid_t *gid) ;

/**
 * @brief Resolve a gid to its group name.
 *
 * Looks up @gid with `getgrgid()` and, on success, appends the entry's `gr_name`
 * to @name via `strbuf_cats()`. The caller's `errno` is saved on entry and
 * restored before returning 1.
 *
 * @param[in]     gid  Group id to resolve.
 * @param[in,out] name Strbuf the resolved name is appended to (existing content
 *                     is kept). Must be a valid strbuf; not modified on failure.
 *
 * @return 1 on success (name appended to @name).
 * @return 0 on failure. `errno` is set to the value left by `getgrgid()` when the
 *         lookup failed with an error, to `ESRCH` when `getgrgid()` returned NULL
 *         without setting `errno` (no such gid), or to `ERANGE` when appending the
 *         name to @name failed (allocation failure / size overflow). On the
 *         lookup-failure paths @name is untouched; on the append-failure path
 *         @name may hold a partial result.
 */
extern int get_groupbygid(gid_t gid,strbuf *name) ;

/**
 * @brief Resolve a group name to its gid.
 *
 * Looks up @name with `getgrnam()` and, on success, stores the entry's `gr_gid`
 * into @gid. The caller's `errno` is saved on entry and restored before
 * returning 1.
 *
 * @param[in]  name Group name to resolve. Must not be NULL.
 * @param[out] gid  Receives the resolved gid on success. Untouched on failure.
 *
 * @return 1 on success (@gid set, `errno` preserved).
 * @return -1 on failure (@gid untouched). `errno` is set to `EINVAL` when @name
 *         is NULL, to the value left by `getgrnam()` when the lookup failed with
 *         an error, or to `ESRCH` when `getgrnam()` returned NULL without setting
 *         `errno` (no such group).
 */
extern int get_gidbygroup(char const *name,gid_t *gid) ;

/**
 * @brief Resolve a whitespace-separated list of user names to their uids.
 *
 * Tokenises @str into user names (using `sbl_clean_string()`, which escapes
 * `' \t\r\n'` and splits on spaces/newlines), resolves each name with
 * get_uidbyname(), and writes the uids into @array. Element 0 of @array is a
 * running count: the function reads its initial value as the starting index and
 * increments it once per resolved user, appending each uid at the new index.
 * For example, with `array[0] == 0` and @str = "root obarun", on success
 * `array[0] == 2`, `array[1]` and `array[2]` hold the two uids.
 *
 * @param[in]     str   Whitespace-separated list of user names. Must not be
 *                      empty and must not alias the internal buffer.
 * @param[in,out] array uid output buffer. `array[0]` must be initialised by the
 *                      caller to the desired starting count (typically 0); it is
 *                      read on entry and updated as users are appended. The
 *                      caller must size @array to hold `array[0] + (number of
 *                      names in @str) + 1` `uid_t` elements; this function
 *                      performs no bounds checking.
 *
 * @return 1 on success: every name in @str resolved and was appended to @array.
 * @return 0 on failure. `errno` is set by the failing inner call:
 *         - `sbl_clean_string()` failed (e.g. `errno` `EINVAL` when @str is
 *           empty), in which case @array is untouched; or
 *         - a name did not resolve, in which case `errno` is the value left by
 *           get_uidbyname() (the `getpwnam()` errno, or `ESRCH` for no such
 *           user). @array may have been partially updated for the names
 *           resolved before the failing one.
 *
 * @note Despite resolving account names, this function prints no message and
 *       performs no warning output; a missing user is reported only through the
 *       0 return and `errno`.
 * @see get_uidbyname
 */
extern int scan_uidlist(char const *str, uid_t *array) ;

#endif
