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
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_ACCOUNT_H
#define OB_ACCOUNT_H

#include <sys/types.h>

#include <oblibs/strbuf.h>

/** get the uid of user's @name
 * and store the result in @uid
 * @Return 1 on success
 * @Return -1 on fail and set errno */
extern int get_uidbyname(char const *name,uid_t *uid) ;

/** get the name of user's @uid
 * and store the result in @name
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int get_namebyuid(uid_t uid,strbuf *name) ;

/** get the gid of user's @name
 * and store the result in @gid
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int get_gidbyname(char const *name,gid_t *gid) ;

/** get the name of group's @gid
 * and store the result in @name
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int get_groupbygid(gid_t gid,strbuf *name) ;

/** get the gid of group's @name
 * and store the result in @gid
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int get_gidbygroup(char const *name,gid_t *gid) ;

/** Check if @str is a list of valid user
 * and store the uid of the user in @array.
 * The first elements of the array is reserved
 * to set the number of elements scanned
 * e.g if @str contain root obarun the array[0] will
 * contain 2.
 * @Display warning if the user is not on the system
 * then @Return 0
 * @Return 1 on success*/
extern int scan_uidlist(char const *str, uid_t *array) ;

#endif
