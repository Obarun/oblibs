/*
 * get_wasted_line.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <ctype.h>

#include <oblibs/string.h>


int get_wasted_line(char const *line)
{
    int space ;
    space = 0 ;

    if (!line || line[0] == '#' ||  line[0] == '\n') return 0 ;
    while(isspace(line[space])){
        space++;
    }
    if(!line[space] || line[space] == '#') return 0 ;

    return 1 ;
}
