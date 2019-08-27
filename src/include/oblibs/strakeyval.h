/* 
 * strakeyval.h
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
 
#ifndef OB_STRAKEYVAL_H
#define OB_STRAKEYVAL_H

#include <stddef.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/types.h>

typedef struct strakeyval_s strakeyval, *strakeyval_ref ;
struct strakeyval_s
{
	stralloc key ;
	stralloc val ;
	uint32_t n ;
} ;
#define STRAKEYVAL_ZERO { .key = STRALLOC_ZERO, .val = STRALLOC_ZERO, .n = 0 }
extern strakeyval const strakeyval_zero ;

/** strakeyval type*/
#define gakvkey(ga) genalloc_s(strakeyval,ga)->key.s
#define gakvkeylen(ga) strlen(genalloc_s(strakeyval,ga)->key.s)
#define gaikvkey(ga,i) genalloc_s(strakeyval,ga)[i].key.s
#define gaikvkeylen(ga,i) strlen(genalloc_s(strakeyval,ga)[i].key.s)
#define gakvval(ga) genalloc_s(strakeyval,ga)->val.s
#define gakvvallen(ga) strlen(genalloc_s(strakeyval,ga)->val.s)
#define gaikvval(ga,i) genalloc_s(strakeyval,ga)[i].val.s
#define gaikvvallen(ga,i) strlen(genalloc_s(strakeyval,ga)[i].val.s)

/** Freed @ga*/
extern void strakv_free(strakeyval *ga) ;

/** add @key @val pair into an genalloc array @ga
 * @Return 1 on success 
 * @Return 0 if the allocation fail */
extern int strakv_new(genalloc *ga,char const *key,size_t klen,char const *val,size_t vlen) ;
#define strakv_add(ga,k,v) strakv_new(ga,k,strlen(k),v,strlen(v))

/** Parse @s with @sep as delim and store the result in @ga
 * @Return 1 on sucess
 * @Return 0 on fail*/
extern int strakv_addwithsep(genalloc *ga,char *s,char const *sep) ;
#define strakv_addequal(ga,s) strakv_addwithsep(ga,s,"=")


#endif
