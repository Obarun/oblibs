/* 
 * mill_config.c
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
 * */

#include <oblibs/mill.h>


// Usefull and general parse_mill_t
parse_mill_t MILL_GET_LINE = \
{ \
	.close = '\n', .end = "\n", \
	.endlen = 1, .forceclose = 1, .forceopen = 1, \
	.inner.debug = "get_line" } ;

parse_mill_t MILL_GET_SPACE_NCLEAN = \
{ \
	.skip = " \n\t\r", .skiplen = 4, \
	.forceclose = 1, .forceopen = 1, .forceskip = 1, \
	.inner.debug = "get_clean_space" } ;
								
parse_mill_t MILL_GET_COMMENT = \
{ \
	.jump = "#", .jumplen = 1, .flush = 0, \
	.inner.debug = "get_comment" } ;
