/* 
 * types.h
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
 
#ifndef TYPES_H
#define TYPES_H

#include <sys/types.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>

/** Scan if @str has the corresponding @mode
 * @Return 1 on success
 * @Return 0 if @str doesn't exist or stat fail
 * @Return -1 if @str haven't the corresponding @mode
 * Also warm the user if stat fail*/
extern ssize_t scan_mode(char const *str,mode_t mode) ;

/** Check if @s is an uint32_t type
 * @s mustn't contain space
 * @Return 1 on success else 0*/
extern int scan_uint32(char *s) ;

/** get the uid of @name
 * and store it at @uid
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int get_uidbyname(const char *name,uid_t *uid) ;

/** get the name of @uid
 * and store it at @name
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern int get_namebyuid(uid_t uid,stralloc *name) ;

/** Check if @str is an uint32_t type
 * store the result in @array at index @row
 * @Return 1 on success
 * @Return -1 if @str is not an uint32 type
 * @Return 0 if array[row] is already set
 * This function assume that @str is different than 0 */ 
extern int scan_timeout(char *str, uint32_t *array, int row) ;

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

#endif
