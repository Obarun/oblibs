/* 
 * oblist.h
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
 
#ifndef OBLIST_H
#define OBLIST_H

#include <string.h>

#include <skalibs/stralloc.h>

typedef struct oblist_s oblist ;
struct oblist_s
{
	void *s ; // string
	size_t len ; // len of string
	oblist *n ; //next list
		
} ;

typedef struct oblist2_s oblist2 ;
struct oblist2_s
{
	void *s ;
	size_t len ;
	oblist2 *n ;//next list
	oblist2 *p ;// previous list
} ;
#define OBLIST_ZERO { NULL }
extern oblist const oblist_zero ;


/** ***********************************
 *        @WARNING 
 * before using oblist is need to be
 * initialize to NULL by OBLIST_ZERO e.g
 * oblist *list = OBLIST_ZERO
 * ************************************/
 
/** allocate a new list
 * @return 1 on success else 0
 * if @s is NULL, list is allocated and
 * @list->s point to NULL 
 * @list->n point to NULL in any case of success*/
extern int oblist_new(oblist **list, void const *s, size_t len) ;

/** add a link at the tail of the @list
 * and populate it with @s
 * @Return 1 on success else 0*/
extern int oblist_addtail(oblist **list, char const *s, size_t n) ;
#define oblist_addts(list, s) oblist_addtail(list,(s),strlen(s))  

/** add a link at the head of the @list
 * and populate it with @s
 * @Return 1 on success else 0*/
extern int oblist_addhead (oblist **list, char const *s, size_t n) ;
#define oblist_addhs(list, s) oblist_addhead(list,(s),strlen(s))

/** reverse the order of link in @list */
extern void oblist_reverse(oblist **list) ;

/** @Return the number of link in @list */
extern size_t oblist_len(oblist *list) ;

/** compare @list->s with @s
 * @Return 1 on success else 0 */
extern int oblist_compare(oblist *list, char const *s) ;

/** compare each link of @list with @s
 * @Return 1 on success else 0*/
extern int oblist_compareall(oblist *list,char const *s) ;

/** compare each link of @list with @s
 * @Return a pointer to the link
 * @Return NULL if not found */
extern oblist *oblist_find(oblist **list, char const *s) ;

extern void oblist_iter(oblist *list, void (*func)(oblist *data)) ;

extern oblist *oblist_map(oblist *list, oblist *(*func)(oblist *data)) ;

/** freed a link of @list*/
extern void oblist_free(oblist *list) ;
/** freed all link in @list and @list itself */
extern void oblist_freeall(oblist *list) ;
/** freed the link in @list and set pointer to NULL */
extern void oblist_delone (oblist *list, void (*func)(void *, size_t)) ;

#endif
