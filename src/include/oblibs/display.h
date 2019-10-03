/* 
 * display.h
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


#ifndef OB_DISPLAY_H
#define OB_DISPLAY_H

#define NOCOLOR     "\033[0m"
#define BOLD        "\033[0;1m"
//color
#define BLACK       "\033[0;30m"
#define RED         "\033[0;31m"
#define GREEN       "\033[0;32m"
#define YELLOW      "\033[0;33m"
#define BLUE        "\033[0;34m"
#define MAGENTA     "\033[0;35m"
#define CYAN        "\033[0;36m"
#define WHITE       "\033[0;37m"
//bold
#define BBLACK      "\033[1;30m"
#define BRED        "\033[1;31m"
#define BGREEN      "\033[1;32m"
#define BYELLOW     "\033[1;33m"
#define BBLUE       "\033[1;34m"
#define BMAGENTA    "\033[1;35m"
#define BCYAN       "\033[1;36m"
#define BWHITE      "\033[1;37m"
//underline
#define UBLACK      "\033[4;30m"
#define URED        "\033[4;31m"
#define UGREEN      "\033[4;32m"
#define UYELLOW     "\033[4;33m"
#define UBLUE       "\033[4;34m"
#define UMAGENTA    "\033[4;35m"
#define UCYAN       "\033[4;36m"
#define UWHITE      "\033[4;37m"
//background
#define B_BLACK     "\033[40m"
#define B_RED       "\033[41m"
#define B_GREEN     "\033[42m"
#define B_YELLOW    "\033[43m"
#define B_BLUE      "\033[44m"
#define B_MAGENTA   "\033[45m"
#define B_CYAN      "\033[46m"
#define B_WHITE     "\033[47m"
//high intensity
#define IBLACK      "\033[0;90m"
#define IRED        "\033[0;91m"
#define IGREEN      "\033[0;92m"
#define IYELLOW     "\033[0;93m"
#define IBLUE       "\033[0;94m"
#define IMAGENTA    "\033[0;95m"
#define ICYAN       "\033[0;96m"
#define IWHITE      "\033[0;97m"
//bold high intensity
#define IBBLACK     "\033[1;90m"
#define IBRED       "\033[1;91m"
#define IBGREEN     "\033[1;92m"
#define IBYELLOW    "\033[1;93m"
#define IBBLUE      "\033[1;94m"
#define IBMAGENTA   "\033[1;95m"
#define IBCYAN      "\033[1;96m"
#define IBWHITE     "\033[1;97m"
//background high intensity
#define B_IBLACK    "\033[0;100m"
#define B_IRED      "\033[0;101m"
#define B_IGREEN    "\033[0;102m"
#define B_IYELLOW   "\033[0;103m"
#define B_IBLUE     "\033[0;104m"
#define B_IMAGENTA  "\033[0;105m"
#define B_ICYAN     "\033[0;106m"
#define B_IWHITE    "\033[0;107m"
//blink bold
#define BL_BBLACK   "\033[5;1;30m"
#define BL_BRED     "\033[5;1;31m"
#define BL_BGREEN   "\033[5;1;32m"
#define BL_BYELLOW  "\033[5;1;33m"
#define BL_BBLUE    "\033[5;1;34m"
#define BL_BMAGENTA "\033[5;1;35m"
#define BL_BCYAN    "\033[5;1;36m"
#define BL_BWHITE   "\033[5;1;37m"

typedef struct set_color_s set_color_t ;
struct set_color_s
{
	char const *info ; //bblack
	char const *valid ; // bgreen
	char const *warning ; //byellow
	char const *blink ; //blink_blue
	char const *error ; //bred
	char const *off ; //nocolor
} ;
extern set_color_t const obcolor ;
#endif
