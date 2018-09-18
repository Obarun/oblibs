/* 
 * bytes.h
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
 
#ifndef BYTES_H
#define BYTES_H

#include <stddef.h>

/** replace all char on @s by 0*/
extern void byte_tozero(void *s, size_t n) ;

/** find all @b occurence on @s of lenght @len
 * @Return the count*/
extern size_t byte_tosearch (char const *s, size_t len, char const b) ;

#endif
