/* 
 * memory.h
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_MEMORY_H
#define OB_MEMORY_H

#include <stddef.h>

/** alloc memory with size of @size 
 * and initialize string with 0
 * @Return a pointer to the string 
 * @Return NULL in case of failure*/
extern void *obstr_alloc(size_t size) ; 

/** freed the address of @ptr
 * and set it to NULL*/
extern void free_mem(void **ptr) ;

/** freed the address of @str
 * Set the pointer to NULL*/
extern void obstr_del(void **str) ;

#endif
