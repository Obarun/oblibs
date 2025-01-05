/*
 * log.h
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_LOG_H
#define OB_LOG_H

#include <errno.h>
#include <stddef.h>
#include <stdarg.h>

#include <skalibs/gccattributes.h>

// might we have some colors...
#define NOCOLOR     "\x1B[0m"
#define BOLD        "\x1B[0;1m"
#define BLINK       "\x1B[5;37m"

//color
#define BLACK       "\x1B[0;30m"
#define RED         "\x1B[0;31m"
#define GREEN       "\x1B[0;32m"
#define YELLOW      "\x1B[0;33m"
#define BLUE        "\x1B[0;34m"
#define MAGENTA     "\x1B[0;35m"
#define CYAN        "\x1B[0;36m"
#define WHITE       "\x1B[0;37m"
//bold
#define BBLACK      "\x1B[1;30m"
#define BRED        "\x1B[1;31m"
#define BGREEN      "\x1B[1;32m"
#define BYELLOW     "\x1B[1;33m"
#define BBLUE       "\x1B[1;34m"
#define BMAGENTA    "\x1B[1;35m"
#define BCYAN       "\x1B[1;36m"
#define BWHITE      "\x1B[1;37m"
//underline
#define UBLACK      "\x1B[4;30m"
#define URED        "\x1B[4;31m"
#define UGREEN      "\x1B[4;32m"
#define UYELLOW     "\x1B[4;33m"
#define UBLUE       "\x1B[4;34m"
#define UMAGENTA    "\x1B[4;35m"
#define UCYAN       "\x1B[4;36m"
#define UWHITE      "\x1B[4;37m"
//background
#define B_BLACK     "\x1B[40m"
#define B_RED       "\x1B[41m"
#define B_GREEN     "\x1B[42m"
#define B_YELLOW    "\x1B[43m"
#define B_BLUE      "\x1B[44m"
#define B_MAGENTA   "\x1B[45m"
#define B_CYAN      "\x1B[46m"
#define B_WHITE     "\x1B[47m"
//high intensity
#define IBLACK      "\x1B[0;90m"
#define IRED        "\x1B[0;91m"
#define IGREEN      "\x1B[0;92m"
#define IYELLOW     "\x1B[0;93m"
#define IBLUE       "\x1B[0;94m"
#define IMAGENTA    "\x1B[0;95m"
#define ICYAN       "\x1B[0;96m"
#define IWHITE      "\x1B[0;97m"
//bold high intensity
#define IBBLACK     "\x1B[1;90m"
#define IBRED       "\x1B[1;91m"
#define IBGREEN     "\x1B[1;92m"
#define IBYELLOW    "\x1B[1;93m"
#define IBBLUE      "\x1B[1;94m"
#define IBMAGENTA   "\x1B[1;95m"
#define IBCYAN      "\x1B[1;96m"
#define IBWHITE     "\x1B[1;97m"
//background high intensity
#define B_IBLACK    "\x1B[0;100m"
#define B_IRED      "\x1B[0;101m"
#define B_IGREEN    "\x1B[0;102m"
#define B_IYELLOW   "\x1B[0;103m"
#define B_IBLUE     "\x1B[0;104m"
#define B_IMAGENTA  "\x1B[0;105m"
#define B_ICYAN     "\x1B[0;106m"
#define B_IWHITE    "\x1B[0;107m"
//blink bold
#define BL_BBLACK   "\x1B[5;1;30m"
#define BL_BRED     "\x1B[5;1;31m"
#define BL_BGREEN   "\x1B[5;1;32m"
#define BL_BYELLOW  "\x1B[5;1;33m"
#define BL_BBLUE    "\x1B[5;1;34m"
#define BL_BMAGENTA "\x1B[5;1;35m"
#define BL_BCYAN    "\x1B[5;1;36m"
#define BL_BWHITE   "\x1B[5;1;37m"

// basic structure for the color scheme
typedef struct set_color_s set_color_t ;
struct set_color_s
{
    char const *info ; //bblack
    char const *valid ; // bgreen
    char const *warning ; //byellow
    char const *blink ; //blink_blue
    char const *error ; //bred
    char const *ablink ; //blink
    char const *blue ; //blink
    char const *magenta ; //bmagenta
    char const *off ; //nocolor
} ;

extern set_color_t log_color_enable ;
extern set_color_t log_color_disable ;
// Color is disable by default
extern set_color_t *log_color ;

/* Convention log verbosity
 * 0 -> error -> log_die and log_die_nclean familly
 * 1 -> error + info -> log_info familly
 * 2 -> error + info + warning -> log_warn familly
 * 3 -> error + info + warning + trace -> log_trace familly
 * 4 -> error + info + warning + trace + (__FILE__,__func__,__LINE__)
 * 5 -> error + info + warning + flow + (__FILE__,__func__,__LINE__) */

enum log_level {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_FLOW
} ;

// Exit code convention
enum log_exit_status {

    LOG_EXIT_SYS = 111 ,
    LOG_EXIT_USER = 100 ,
    LOG_EXIT_ZERO = 0 ,
    LOG_EXIT_ONE = 1 ,
    LOG_EXIT_LESSONE = -1
} ;


// Allow to see the file,function and the line
// at DEBUG/FLOW level
typedef struct log_dbg_info_s log_dbg_info_t ;
struct log_dbg_info_s {
    const char *file ;
    const char *func ;
    int line ;
} ;

#define log_dbg_init { .file = __FILE__, .func = __func__, .line = __LINE__ }

// basic function to pass to the cleaner at die state
typedef void ss_log_cleanup_func_t(void) ;
typedef ss_log_cleanup_func_t *ss_log_cleanup_func_t_ref ;
extern ss_log_cleanup_func_t log_cleanup ;

// Append or not the output with the errno field
#define LOG_SYS_NO 0
#define LOG_SYS_YES 2 // 2 instead of 1 to append the ':' charactere before strerror(errno)
extern size_t *LOG_SYS ;

// basic string
extern char const *LOG_ERROR_MSG[6] ;
extern char const *LOG_ERROR_UNABLE_MSG[7] ;
extern char const *LOG_INFO_MSG[6] ;
extern char const *LOG_WARN_MSG[6] ;
extern char const *LOG_WARN_UNABLE_MSG[7] ;
extern char const *LOG_TRACE_MSG[6] ;
extern char const *LOG_FLOW_MSG[6] ;
extern char const *LOG_USAGE_MSG[6] ;
extern char const *LOG_NOMEM_MSG[6] ;

extern unsigned int VERBOSITY ;
extern unsigned int DOUBLE_OUTPUT ;
extern unsigned int TRAILING_NEWLINE ;
extern unsigned int SWITCH_STREAM ;
extern unsigned int DEFAULT_MSG ;
extern unsigned int CLOCK_ENABLED ;
extern unsigned int CLOCK_TIMESTAMP ;
extern char const *PROG ;
extern char const *REDIRFD_1 ;
extern char const *REDIRFD_2 ;
// initialize color
extern void log_color_init(void) ;

// output to stderr and stdout
extern void set_double_output(int enable) ;
// do not append trailing newline
extern void set_trailing_newline(int enable) ;
// reverse stream: stderr -> stdout, stdout->stderr
extern void set_switch_stream(int enable) ;
// display default message e.g info:, warning:,...
extern void set_default_msg(int enable) ;
// display the clock time
extern void set_clock_enable(int enable) ;
// format of the time to display, 0 -> TAI, 1 -> ISO
extern void set_clock_timestamp(int enable) ;
// set the file to use for stdout redirection
extern void set_redirfd_1(char const *file) ;
// set the file to use for stderr redirection
extern void set_redirfd_2(char const *file) ;

// general function to redirect stdout/stderr
extern void redir_fd(int fd,char const *file) ;

// main function used everywhere
extern void log_out(char const *str[],...) ;

// build the string before calling log_out
extern void log_out_builder(log_dbg_info_t *db_info,int level, char const *msg[],char const *str[],...) ;

// die builder function
extern void log_die_builder (int e,log_dbg_info_t *db_info,int level, char const *msg[],char const *str[],...) gccattr_noreturn ;
extern void log_die_nclean_builder (int e,ss_log_cleanup_func_t *cleaner,log_dbg_info_t *db_info,int level, char const *msg[],char const *str[],...) gccattr_noreturn ;
extern void log_warn_nclean_builder(ss_log_cleanup_func_t *cleaner,log_dbg_info_t *db_info,int level,char const *msg[], char const *str[],...) ;

// let's go
// these function only accept string as arguments
// compiler will warn if it's not the case

/**
 *
 * INFO
 *
 * **/

// prog: info: msg
#define log_info(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_INFO_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// prog: info: msg: strerror msg
#define log_infosys(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_INFO_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

#define log_info_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_INFO_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

#define log_infosys_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_INFO_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

#define log_info_nclean_return(ret,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_warn_nclean_builder(cleaner,&dbg_info,LOG_LEVEL_INFO,LOG_INFO_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

/**
 *
 * TRACING
 *
 * **/

// prog: tracing: msg
#define log_trace(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_TRACE,LOG_TRACE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// prog: tracing: msg: strerror msg
#define log_tracesys(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_TRACE,LOG_TRACE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

#define log_trace_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_TRACE,LOG_TRACE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

#define log_tracesys_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_TRACE,LOG_TRACE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

/** tracing with LOG_LEVEL_INFO */

#define log_1_trace(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_TRACE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// prog: tracing: msg: strerror msg
#define log_1_tracesys(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_TRACE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

#define log_1_trace_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_TRACE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

#define log_1_tracesys_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_TRACE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)


/**
 *
 * WARNING
 *
 * **/

// prog: warning: msg
#define log_warn(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_WARN,LOG_WARN_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// prog: warning: msg: strerror msg
#define log_warnsys(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_out_builder(&dbg_info,LOG_LEVEL_WARN,LOG_WARN_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// prog: warning: unable to msg
#define log_warnu(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_WARN,LOG_WARN_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// prog: warning: unable to msg: strerror msg
#define log_warnusys(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_out_builder(&dbg_info,LOG_LEVEL_WARN,LOG_WARN_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

#define log_warn_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_WARN,LOG_WARN_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

// prog: warning: msg: strerror msg
#define log_warnsys_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_out_builder(&dbg_info,LOG_LEVEL_WARN,LOG_WARN_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

// prog: warning: unable to msg
#define log_warnu_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_WARN,LOG_WARN_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

// prog: warning: unable to msg: strerror msg
#define log_warnusys_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_out_builder(&dbg_info,LOG_LEVEL_WARN,LOG_WARN_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)


#define log_warn_nclean_return(ret,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_warn_nclean_builder(cleaner,&dbg_info,LOG_LEVEL_WARN,LOG_WARN_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

// prog: warning: msg: strerror msg
#define log_warnsys_nclean_return(ret,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_warn_nclean_builder(cleaner,&dbg_info,LOG_LEVEL_WARN,LOG_WARN_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

// prog: warning: unable to msg
#define log_warnu_nclean_return(ret,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_warn_nclean_builder(cleaner,&dbg_info,LOG_LEVEL_WARN,LOG_WARN_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

// prog: warning: unable to msg: strerror msg
#define log_warnusys_nclean_return(ret,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_warn_nclean_builder(cleaner,&dbg_info,LOG_LEVEL_WARN,LOG_WARN_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

/**
 *
 * warning with LOG_LEVEL_INFO
 *
 * */

// prog: warning: msg
#define log_1_warn(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_WARN_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// prog: warning: msg: strerror msg
#define log_1_warnsys(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_WARN_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// prog: warning: unable to msg
#define log_1_warnu(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_WARN_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// prog: warning: unable to msg: strerror msg
#define log_1_warnusys(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_WARN_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

#define log_1_warn_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_WARN_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

// prog: warning: msg: strerror msg
#define log_1_warnsys_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_WARN_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

// prog: warning: unable to msg
#define log_1_warnu_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_WARN_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

// prog: warning: unable to msg: strerror msg
#define log_1_warnusys_return(ret,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_out_builder(&dbg_info,LOG_LEVEL_INFO,LOG_WARN_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)


#define log_1_warn_nclean_return(ret,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_warn_nclean_builder(cleaner,&dbg_info,LOG_LEVEL_INFO,LOG_WARN_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

// prog: warning: msg: strerror msg
#define log_1_warnsys_nclean_return(ret,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_warn_nclean_builder(cleaner,&dbg_info,LOG_LEVEL_INFO,LOG_WARN_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

// prog: warning: unable to msg
#define log_1_warnu_nclean_return(ret,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_warn_nclean_builder(cleaner,&dbg_info,LOG_LEVEL_INFO,LOG_WARN_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

// prog: warning: unable to msg: strerror msg
#define log_1_warnusys_nclean_return(ret,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_warn_nclean_builder(cleaner,&dbg_info,LOG_LEVEL_INFO,LOG_WARN_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
    return ret ; \
} while (0)

/**
 *
 * FATAL(die)
 *
 * **/

// prog: fatal: msg
#define log_die(err,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_die_builder(err,&dbg_info,LOG_LEVEL_ERROR,LOG_ERROR_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// prog: fatal: msg: strerror msg
#define log_diesys(err,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_die_builder(err,&dbg_info,LOG_LEVEL_ERROR,LOG_ERROR_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// prog: fatal: unable to msg
#define log_dieu(err,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_die_builder(err,&dbg_info,LOG_LEVEL_ERROR,LOG_ERROR_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// prog: fatal: unable to msg: strerror msg
#define log_dieusys(err,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_die_builder(err,&dbg_info,LOG_LEVEL_ERROR,LOG_ERROR_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// these following do the same as log_die? familly but
// call a cleanup function before dying
#define log_die_nclean(err,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_die_nclean_builder(err,cleaner,&dbg_info,LOG_LEVEL_ERROR,LOG_ERROR_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

#define log_diesys_nclean(err,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_die_nclean_builder(err,cleaner,&dbg_info,LOG_LEVEL_ERROR,LOG_ERROR_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

#define log_dieu_nclean(err,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_die_nclean_builder(err,cleaner,&dbg_info,LOG_LEVEL_ERROR,LOG_ERROR_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

#define log_dieusys_nclean(err,cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    log_die_nclean_builder(err,cleaner,&dbg_info,LOG_LEVEL_ERROR,LOG_ERROR_UNABLE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

// output fatal msg
#define log_fatal(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_ERROR,LOG_ERROR_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

/**
 *
 * USAGE
 *
 * **/

// prog: usage: msg
#define log_usage(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_die_builder(LOG_EXIT_USER,&dbg_info,LOG_LEVEL_ERROR,LOG_USAGE_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

/**
 *
 * OUT OF MEMORY
 *
 * **/

// prog: out of memory: msg
// force verbosity to have the full debug mode
#define log_die_nomem(...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    VERBOSITY = LOG_LEVEL_DEBUG ; \
    log_die_builder(LOG_EXIT_SYS,&dbg_info,LOG_LEVEL_DEBUG,LOG_NOMEM_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

#define log_die_nomem_nclean(cleaner,...) do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_YES ; \
    VERBOSITY = LOG_LEVEL_DEBUG ; \
    log_die_nclean_builder(LOG_EXIT_SYS,cleaner,&dbg_info,LOG_LEVEL_DEBUG,LOG_NOMEM_MSG,(char const *[]){__VA_ARGS__, NULL}) ; \
} while (0)

/**
 *
 * FLOW
 *
 * **/

#define log_flow() do { \
    log_dbg_info_t dbg_info = log_dbg_init ; \
    LOG_SYS = (size_t *)LOG_SYS_NO ; \
    log_out_builder(&dbg_info,LOG_LEVEL_FLOW,LOG_FLOW_MSG,(char const *[]){"entering: ", dbg_info.file," -> ",dbg_info.func, "()", NULL}) ; \
} while (0)


#endif
