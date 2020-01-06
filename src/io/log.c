/* 
 * log.c
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

#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>

#include <oblibs/log.h>

#include <skalibs/buffer.h>
#include <skalibs/env.h>
#include <skalibs/types.h>

// strerror is disabled by default
// the define of the log function
// will deal with it automatically
size_t *LOG_SYS = LOG_SYS_NO ;

// only error + log_info by default
unsigned int VERBOSITY = 1 ;

char const *LOG_ERROR_MSG[] ;
char const *LOG_ERROR_UNABLE_MSG[] ;
char const *LOG_INFO_MSG[] ;
char const *LOG_WARN_MSG[] ;
char const *LOG_WARN_UNABLE_MSG[] ;
char const *LOG_TRACE_MSG[] ;
char const *LOG_USAGE_MSG[] ;
char const *LOG_NOMEM_MSG[] ;

// Color is disable by default
set_color_t *log_color = &log_color_disable ;

set_color_t log_color_enable = {
	BWHITE, //info
	BGREEN, // valid
	BYELLOW,  //warning
	BL_BBLUE, // blink
	BRED, //error
	NOCOLOR //reset
} ;

set_color_t log_color_disable = {
	"",
	"",
	"",
	"",
	"",
	""
} ;

void log_color_init(void)
{
	LOG_ERROR_MSG[0] = LOG_ERROR_UNABLE_MSG[0] = ": " ;
	LOG_ERROR_MSG[1] = LOG_ERROR_UNABLE_MSG[1] = log_color->error ;
	LOG_ERROR_MSG[2] = LOG_ERROR_UNABLE_MSG[2] = "fatal" ;
	LOG_ERROR_MSG[3] = LOG_ERROR_UNABLE_MSG[3] = log_color->off ;
	LOG_ERROR_MSG[4] = LOG_ERROR_UNABLE_MSG[4] = ": " ;
	LOG_ERROR_UNABLE_MSG[5] = "unable to " ;
	LOG_ERROR_MSG[5] = LOG_ERROR_UNABLE_MSG[6] = 0 ;
	
	LOG_INFO_MSG[0] = ": " ;
	LOG_INFO_MSG[1] = log_color->valid ;
	LOG_INFO_MSG[2] = "info" ;
	LOG_INFO_MSG[3] = log_color->off ;
	LOG_INFO_MSG[4] = ": " ;
	LOG_INFO_MSG[5] = 0 ;
	
	LOG_WARN_MSG[0] = LOG_WARN_UNABLE_MSG[0] = ": " ;
	LOG_WARN_MSG[1] = LOG_WARN_UNABLE_MSG[1] = log_color->warning ;
	LOG_WARN_MSG[2] = LOG_WARN_UNABLE_MSG[2] = "warning" ;
	LOG_WARN_MSG[3] = LOG_WARN_UNABLE_MSG[3] = log_color->off ;
	LOG_WARN_MSG[4] = LOG_WARN_UNABLE_MSG[4] = ": " ;
	LOG_WARN_UNABLE_MSG[5] = "unable to " ;
	LOG_WARN_MSG[5] = LOG_WARN_UNABLE_MSG[6] = 0 ;
	
	LOG_TRACE_MSG[0] = ": " ;
	LOG_TRACE_MSG[1] = log_color->info ;
	LOG_TRACE_MSG[2] = "tracing" ;
	LOG_TRACE_MSG[3] = log_color->off ;
	LOG_TRACE_MSG[4] = ": " ;
	LOG_TRACE_MSG[5] = 0 ;
	
	LOG_USAGE_MSG[0] = ": " ;
	LOG_USAGE_MSG[1] = log_color->error ;
	LOG_USAGE_MSG[2] = "usage" ;
	LOG_USAGE_MSG[3] = log_color->off ;
	LOG_USAGE_MSG[4] = ": " ;
	LOG_USAGE_MSG[5] = 0 ;
	
	LOG_NOMEM_MSG[0] = ": " ;
	LOG_NOMEM_MSG[1] = log_color->error ;
	LOG_NOMEM_MSG[2] = "out of memory" ;
	LOG_NOMEM_MSG[3] = log_color->off ;
	LOG_NOMEM_MSG[4] = ": " ;
	LOG_NOMEM_MSG[5] = 0 ;
}

void log_out(char const *str[],...)
{ 
	int e = errno ;
	va_list alist ;
	va_start(alist,str) ;
		
	while (*str) 
		buffer_puts(buffer_2, *str++) ; 
	
	buffer_putflush(buffer_2, "\n", 1) ; 
	va_end(alist) ; 
	errno = e ;
}

void log_out_builder(log_dbg_info_t *db_info,int level, char const *msg[],char const *str[],...)
{
	if (level <= VERBOSITY)
	{
		log_color_init() ;
		
		size_t len = 3 + env_len(str) + env_len(msg) + (size_t)LOG_SYS ;
		
		if (VERBOSITY >= LOG_LEVEL_DEBUG)
			len += 7 ;
		
		char line[UINT32_FMT] ;
		line[uint32_fmt(line,db_info->line)] = 0 ;
		
		char const *ap[len] ;
		uint8_t i = 0 ;
		ap[i++] = PROG ;
		if (VERBOSITY >= LOG_LEVEL_DEBUG)
		{
			ap[i++] = "(" ;
			ap[i++] = db_info->file ;
			ap[i++] = ": " ;
			ap[i++] = db_info->func ;
			ap[i++] = ": " ;
			ap[i++] = line ;
			ap[i++] = ")" ;
		}
		
		while (*msg)
			ap[i++] = *msg++ ;
		
		while (*str)
			ap[i++] = *str++ ;

		if (LOG_SYS)
		{ 
			ap[i++] = ": " ;
			ap[i++] = strerror(errno) ;
		}
		ap[i] = 0 ;

		log_out(ap) ;
	}
}

void log_die_builder (int e,log_dbg_info_t *db_info,int level, char const *msg[],char const *str[],...)
{
 	log_out_builder(db_info,level,msg,str) ;
	_exit(e) ;
}

void log_die_nclean_builder (int e,ss_log_cleanup_func_t *cleaner,log_dbg_info_t *db_info,int level, char const *msg[],char const *str[],...)
{
 	if (cleaner) (*cleaner)() ;
 	log_out_builder(db_info,level,msg,str) ;
	_exit(e) ;
}

void log_warn_nclean_builder(ss_log_cleanup_func_t *cleaner,log_dbg_info_t *db_info,int level,char const *msg[], char const *str[],...)
{
	if (cleaner) (*cleaner)() ;
 	log_out_builder(db_info,level,msg,str) ;
}
