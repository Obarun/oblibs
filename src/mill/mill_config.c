/*
 * mill_config.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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

parse_mill_t MILL_GET_COMMENT = \
{ \
    .forceopen = 1, .keepopen = 1, \
    .jump = "#;", .jumplen = 2, \
    .end = "=", .endlen = 1, \
    .inner.debug = "get_comment" } ;

parse_mill_t MILL_GET_DOUBLE_QUOTE = \
{ \
    .open = '"', .close = '"', \
    .skip = " \t\r", .skiplen = 3,
    .forceclose = 1, \
    .inner.debug = "get_double_quote" } ;

parse_mill_t MILL_GET_KEY_NCLEAN = \
{ \
    .close = '=', .forceclose = 1, \
    .forceopen = 1, .keepopen = 1, \
    .skip = " \t\r", .skiplen = 3, .forceskip = 1, \
    .inner.debug = "get_key_nclean" } ;

parse_mill_t MILL_GET_VAL = \
{ \
    .open = '=', .end = "\n", \
    .endlen = 1, .inner.debug = "get_val" } ;

parse_mill_t MILL_CLEAN_SPLITTED_VALUE = \
{ \
    .skip = " \n\t\r", .skiplen = 4, \
    .close = 0, .forceclose = 1, \
    .forceopen = 1, .forceskip = 1, \
    .inner.debug = "clean_splitted_value" } ;

parse_mill_t MILL_CLEAN_LINE = \
{ \
    .forceopen = 1, .keepopen = 1, \
    .skip = " \t\r", .skiplen = 3, .forceskip = 1, \
    .end = "\n", .endlen = 1, \
    .inner.debug = "clean_line" } ;

parse_mill_t MILL_SPLIT_ELEMENT = \
{ \
    .close = ' ', .forceopen = 1, \
    .forceclose = 1, .keepopen = 1, \
    .end= "\n", .endlen = 1, \
    .inner.debug = "split_element" } ;

parse_mill_t MILL_SPLIT_LINE = \
{ \
    .close = '\n', .forceopen = 1, .forceclose = 1, \
    .keepopen = 1, \
    .end= "\n", .endlen = 1, \
    .inner.debug = "split_line" } ;
