/* 
 * mill.h
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
 
#ifndef OB_MILL_H
#define OB_MILL_H

#include <string.h>
#include <stdarg.h>
#include <sys/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/lolstdio.h>
#include <skalibs/buffer.h>

// set to 1 to debug the parser
#define MILL_DBG 0 
#define DBG(...) do \
{ \
	bprintf(buffer_1, __VA_ARGS__) ; \
	buffer_putflush(buffer_1,"\n",1) ; \
} while (0)

typedef struct parse_mill_inner_s parse_mill_inner_t, *parse_mill_inner_t_ref ;
struct parse_mill_inner_s
{
	char curr ;
	uint32_t nline ;
	uint8_t nopen ; //number of open found
	uint8_t nclose ; //number of close found
	uint8_t jumped ;//jump was made or not 1->no,0->yes
	uint8_t flushed ;//flush was made or not 1->no,0->yes
	uint8_t search ;//elements searched was found or not 0->no,1->yes
	char const *debug ;//name of the parse_mill_t used
	size_t pos ;// current position
	size_t lastclose ; //position of the last close
} ;

#define PARSE_MILL_INNER_ZERO { .curr = 0, \
							.nline = 1, \
							.nopen = 0, \
							.nclose = 0, \
							.jumped = 0, \
							.flushed = 0, \
							.search = 0 , \
							.debug = 0, \
							.pos = 0, \
							.lastclose = 0 }
							
typedef struct parse_mill_s parse_mill_t,*parse_mill_t_ref ;
struct parse_mill_s
{
	char const *file ;
	char open ;
	char close ;
	char *skip ;
	size_t skiplen ;
	char *end ;
	size_t endlen ;
	char *jump ;
	size_t jumplen ;
	char *search ;
	size_t searchlen ;
	uint8_t check ;//check if nopen == openclose, 0 -> no,1->yes
	uint8_t flush ;//set nopen,nclose,sa.len to 0 at every new line
	uint8_t keepopen ; // force to keep the open character
	uint8_t keepclose ; // force to keep the close character
	uint8_t forceskip ;//force to skip even if nopen is positive
	uint8_t forceopen ; //force to open
	uint8_t forceclose ; //force to close
	parse_mill_inner_t inner ;
} ;

#define PARSE_MILL_ZERO { .file = 0, .open = 0, .close = 0, \
						.skip = 0 , .skiplen = 0, \
						.end = 0 , .endlen = 0, \
						.jump = 0 , .jumplen = 0, \
						.search = 0 , .searchlen = 0, \
						.check = 0 , .flush = 0 , \
						.keepopen = 0, .keepclose = 0, \
						.forceskip = 0, .forceopen = 0, .forceclose = 0, \
						.inner = PARSE_MILL_INNER_ZERO }

typedef enum parse_enum_e parse_enum_t,*parse_enum_t_ref ;
enum parse_enum_e
{
	IGN = 0 ,
	KEEP ,
	JUMP ,
	END ,
	EFILE
} ;



// general function
extern char mill_next(char const *s,size_t *pos) ;
extern uint8_t mill_cclass (parse_mill_t *p) ;
extern int mill_config(parse_mill_t *p, char const *string, stralloc *kp,size_t *pos) ;
extern int mill_prepare(stralloc *dst, char const *string,parse_mill_t *sep,size_t *pos) ;
extern int mill_element(stralloc *dst, char const *string, parse_mill_t *sep,size_t *pos) ;
extern int mill_string(stralloc *dst, stralloc *sa, parse_mill_t *sep) ;
extern int mill_nstring(stralloc *dst, stralloc *sa, parse_mill_t *sep) ;
//helper
extern void wild_zero(parse_mill_t *m) ;
extern void wild_zero_nline(parse_mill_t *m) ;
extern void wild_zero_pos(parse_mill_t *m) ;
extern void wild_zero_all(parse_mill_t *m) ;
extern int mill_inner_loop(char curr,char const *s,size_t len) ;
extern void mill_copy(parse_mill_t *cp, parse_mill_t *m) ;
//usefull config
extern parse_mill_t MILL_GET_LINE ;
extern parse_mill_t MILL_GET_COMMENT ;
extern parse_mill_t MILL_GET_CLEAN_LINE ;
extern parse_mill_t MILL_GET_DOUBLE_QUOTE ;
extern parse_mill_t MILL_GET_KEY_NCLEAN ;
extern parse_mill_t MILL_CLEAN_SPLITTED_VALUE ;
extern parse_mill_t MILL_CLEAN_LINE ;
extern parse_mill_t MILL_SPLIT_ELEMENT ;
extern parse_mill_t MILL_SPLIT_LINE ;
extern parse_mill_t MILL_GET_VAL ;
extern parse_mill_t MILL_GET_VAL_FROM_NLINE ;
#endif
