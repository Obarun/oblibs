/*
 * types.h
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

#ifndef OB_TYPES_H
#define OB_TYPES_H

#include <sys/types.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>

// can be used like FLAGS_SET(has, want|want1|...)
#define FLAGS_SET(has, want) ((has) |= (want))
#define FLAGS_CLEAR(has, want) ((has) &= ~(want))
// return the interger of the flag if its set, 0 otherwise
#define FLAGS_ISSET(has, want) (((has) & (want)) == (want))

/** Scan if @str has the corresponding @mode
 * @Return 1 on success
 * @Return 0 if @str doesn't exist or stat fail
 * @Return -1 if @str haven't the corresponding @mode
 * Also warm the user if stat fail*/
extern ssize_t scan_mode(char const *str,mode_t mode) ;

/** Check if @s is an uint32_t type
 * @s mustn't contain space
 * @Return 1 on success else 0*/
extern int scan_uint32(char const *s) ;

/** get the uid of user's @name
 * and store the result in @uid
 * @Return 1 on success
 * @Return -1 on fail and set errno */
extern int get_uidbyname(char const *name,uid_t *uid) ;

/** get the name of user's @uid
 * and store the result in @name
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int get_namebyuid(uid_t uid,stralloc *name) ;

/** get the gid of user's @name
 * and store the result in @gid
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int get_gidbyname(char const *name,gid_t *gid) ;

/** get the name of group's @gid
 * and store the result in @name
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int get_groupbygid(gid_t gid,stralloc *name) ;

/** get the gid of group's @name
 * and store the result in @gid
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int get_gidbygroup(char const *name,gid_t *gid) ;

/** Check if @str is an uint32_t type
 * store the result in @array at index @row
 * @Return 1 on success
 * @Return -1 if @str is not an uint32 type
 * @Return 0 if array[row] is already set
 * This function assume that @str is different than 0 */
extern int scan_timeout(char const *str, uint32_t *array, int row) ;

/** Check if @str is valid user
 * and store it in @uid
 * @Display warning if the user is not on the system
 * then @Return 0
 * @Return 1 on success*/
extern int scan_uid(char const *str,uid_t *uid) ;

/** Check if @str is a list of valid user
 * and store the uid of the user in @array
 * the first elements of the array is reserved
 * to set the number of elements scanned
 * e.g if @str contain root obarun the array[0] will
 * contain 2.
 * @Display warning if the user is not on the system
 * then @Return 0
 * @Return 1 on success*/
extern int scan_uidlist(char const *str, uid_t *array) ;

/** same as scan_uid but parse first @str
 * and replace all occurence of @delim in @str by ' '*/
extern int scan_uidlist_wdelim(char const *str, uid_t *array,int delim) ;

/** compare @mode with @stmode
 * @Return @stmode if @mode == @stmode
 * otherwise @Return 0 */
extern mode_t get_flags(mode_t mode, mode_t stmode) ;

#endif
