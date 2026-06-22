/*
 * lexer_config.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <oblibs/lexer.h>

// escape ' \t\r\n'
lexer_config LEXER_CFG_TRIM_CLEAN = { \
    .str = 0, .slen = 0,\
    .open = 0, .olen = 0,\
    .close = " \n", .clen = 2,\
    .skip = " \t\r\n", .skiplen = 4,\
    .kopen = 1, .kclose = 0,\
    .forceopen = 1, .forceclose = 1,\
    .firstoccurence = 0,\
    .pos = 0, .opos = 0,\
    .cpos = 0, .invalue = 0,\
    .found = 0, .count = 0,\
    .style = 0, .exitcode = 0,\
} ;

// do not escape anything
lexer_config LEXER_CFG_TRIM_UNCLEAN = { \
    .str = 0, .slen = 0,\
    .open = 0, .olen = 0,\
    .close = " ", .clen = 1,\
    .skip = 0, .skiplen = 0,\
    .kopen = 1, .kclose = 0,\
    .forceopen = 1, .forceclose = 1,\
    .firstoccurence = 0,\
    .pos = 0, .opos = 0,\
    .cpos = 0, .invalue = 0,\
    .found = 0, .count = 0,\
    .style = 0, .exitcode = 0,\
} ;

lexer_config LEXER_CFG_GET_LINE = { \
    .str = 0, .slen = 0,\
    .open = 0, .olen = 0,\
    .close = "\n", .clen = 1,\
    .skip = "\n", .skiplen = 1,\
    .kopen = 1, .kclose = 0,\
    .forceopen = 1, .forceclose = 1,\
    .firstoccurence = 0,\
    .pos = 0, .opos = 0,\
    .cpos = 0, .invalue = 0,\
    .found = 0, .count = 0,\
    .style = 0, .exitcode = 0,\
} ;



