/* 
 * display.h
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


#ifndef DISPLAY_H
#define DISPLAY_H

#define NOCOLOR       "\033[0m"
#define BOLD          "\033[0;1m"
//color
#define BLACK         "\033[0;30m"
#define RED           "\033[0;31m"
#define GREEN         "\033[0;32m"
#define YELLOW        "\033[0;33m"
#define BLUE          "\033[0;34m"
#define MAGENTA       "\033[0;35m"
#define CYAN          "\033[0;36m"
#define WHITE         "\033[0;37m"
//bold
#define BBLACK     "\033[1;30m"
#define BRED       "\033[1;31m"
#define BGREEN     "\033[1;32m"
#define BYELLOW    "\033[1;33m"
#define BBLUE      "\033[1;34m"
#define BMAGENTA   "\033[1;35m"
#define BCYAN      "\033[1;36m"
#define BWHITE     "\033[1;37m"

typedef struct str_color_s obstr_color ;
struct str_color_s
{
	const char *normal ;
	const char *valid ;
	const char *info ;
	const char *warning ;
	const char *error ;
};
extern obstr_color const obcolor ;
#endif
