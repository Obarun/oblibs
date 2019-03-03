/* 
 * stralist.h
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
 
#ifndef STRALIST_H
#define STRALIST_H

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

typedef struct stralist_s stralist ;
struct stralist_s
{
	stralloc sa ;
} ;
#define STRALIST_ZERO { .sa = STRALLOC_ZERO }
extern stralist const stralist_zero ;
/** general type */
#define gaitype(type,ga,i) genalloc_s(type,ga)+i
#define gailen(type,ga,i) genalloc_len(type,ga)+i
/** stralist type*/
#define gastr(ga) genalloc_s(stralist,ga)->sa.s
#define gastrlen(ga) strlen(genalloc_s(stralist,ga)->sa.s)
#define gaistr(ga,i) genalloc_s(stralist,ga)[i].sa.s
#define gaistrlen(ga,i) strlen(genalloc_s(stralist,ga)[i].sa.s)

/** Freed @stra*/
extern void stra_free(stralist *stra) ;

/** add @s string into an genalloc array @ga
 * @Return 1 on success else 0 if the allocation fail */
extern int stra_new(genalloc *ga,char const *s,size_t len) ;
#define stra_add(ga,s) stra_new(ga,s,strlen(s))

/** compare the @s string with @ga at index @idx
 * @Return 1 on success else 0*/
extern int stra_cmpone(genalloc *ga,char const *s,unsigned int idx) ;

/** compare each elements of @ga with @s 
 * @Return 1 on success else 0*/
extern int stra_cmp(genalloc *ga,char const *s) ;

/** return the index number of the @s string into @ga 
 * else return -1 */
extern int stra_findidx(genalloc *ga,char const *s) ;

/** remove an elements at index @idx from @ga genalloc array 
 * @Return 1 on success else 0*/
extern int stra_removeidx(genalloc *ga, unsigned int idx) ;

/** remove @s from @ga
 * @Return 1 on success else 0*/
extern int stra_remove(genalloc *ga, char const *s) ;

/** Copy @ga which have stralist as type
 * to @buf*/
extern void stra_cpytobuf(char *buf,genalloc *ga) ;

/** Copy @ga which have stralist as type
 * to @buf from @start*/
extern void stra_icpytobuf(char *buf,genalloc *ga,int start) ;

/** Copy @ga which have stralist as type
 * to @buf from @start to @end */
extern void stra_iecpytobuf(char *buf,genalloc *ga,int start,int end) ;

/** Replace @sa.s by @s
 * @Return 1 on success
 * @Return 0 on fail*/
extern int stralloc_obreplace(stralloc *sa, char const *s) ;
#endif
