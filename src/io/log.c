/*
 * log.c
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

#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#include <oblibs/log.h>
#include <oblibs/environ.h>

#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/djbtime.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>

// strerror is disabled by default
// the define of the log function
// will deal with it automatically
size_t *LOG_SYS = LOG_SYS_NO ;

// only error + log_info by default
unsigned int VERBOSITY = 1 ;
//only on buffer_1 by default
unsigned int DOUBLE_OUTPUT = 0 ;
// append a trailing newline by default
unsigned int TRAILING_NEWLINE = 1 ;
// set the default buffer to use
// 0 -> buffer_2, 1 -> buffer_1
unsigned int SWITCH_STREAM = 0 ;
// display informative message eg. info;, warning:,...
// 1 -> yes, 0 -> no
unsigned int DEFAULT_MSG = 1 ;
// display clock
unsigned int CLOCK_ENABLED = 0 ;
// format of display, 0->TAI, 1->ISO
unsigned int CLOCK_TIMESTAMP = 1 ;
// name of the file for stdout redirection
char const *REDIRFD_1 = 0 ;
// name of the file for stderr redirection
char const *REDIRFD_2 = 0 ;
// enable or disable color
unsigned int COLOR_ENABLED = 0 ;

char const *LOG_ERROR_MSG[] ;
char const *LOG_ERROR_UNABLE_MSG[] ;
char const *LOG_INFO_MSG[] ;
char const *LOG_WARN_MSG[] ;
char const *LOG_WARN_UNABLE_MSG[] ;
char const *LOG_TRACE_MSG[] ;
char const *LOG_FLOW_MSG[] ;
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
    BLINK, //blink
    BBLUE, //blue
    BMAGENTA, //magenta
    NOCOLOR //reset
} ;

set_color_t log_color_disable = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    ""
} ;


void set_double_output(int enable)
{
    DOUBLE_OUTPUT = !!enable ;
}

void set_trailing_newline(int enable)
{
    TRAILING_NEWLINE = !!enable ;
}

void set_switch_stream(int enable)
{
    SWITCH_STREAM = !!enable ;
}

void set_default_msg(int enable)
{
    DEFAULT_MSG = !!enable ;
}

void set_clock_enable(int enable)
{
    CLOCK_ENABLED = !!enable ;
}

void set_clock_timestamp(int enable)
{
    CLOCK_TIMESTAMP  = !!enable ;
}

void set_redirfd_1(char const *file)
{
    if (file[0] == '0') REDIRFD_1 = 0 ;
    else REDIRFD_1 = file ;
}

void set_redirfd_2(char const *file)
{
    if (file[0] == '0') REDIRFD_2 = 0 ;
    else REDIRFD_2 = file ;
}

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

    LOG_FLOW_MSG[0] = ": " ;
    LOG_FLOW_MSG[1] = log_color->blue ;
    LOG_FLOW_MSG[2] = "flow" ;
    LOG_FLOW_MSG[3] = log_color->off ;
    LOG_FLOW_MSG[4] = ": " ;
    LOG_FLOW_MSG[5] = 0 ;

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

void redir_fd(int fd,char const *file)
{
    unsigned int flags = 0 ;
    flags |= O_CREAT|O_APPEND|O_WRONLY ;
    flags &= ~(O_TRUNC|O_EXCL) ;

    int fdmodif ;
    fdmodif = open3(file, flags, 0666) ;
    if ((fdmodif == -1) && (errno == ENXIO))
    {
        int fdr = open_read(file) ;
        if (fdr == -1) {
            REDIRFD_1 = REDIRFD_2 = 0 ;
            log_dieusys(LOG_EXIT_SYS, "open_read: ", file) ;
        }
        fdmodif = open3(file, flags, 0666) ;
        fd_close(fdr) ;
    }
    if (fdmodif == -1) {
            REDIRFD_1 = REDIRFD_2 = 0 ;
            log_dieusys(LOG_EXIT_SYS, "open: ", file) ;
    }
    if (fd_move(fd, fdmodif) == -1)
    {
        char fmt[UINT_FMT] ;
        fmt[uint_fmt(fmt, fdmodif)] = 0 ;
        char fdmt[UINT_FMT] ;
        fdmt[uint_fmt(fdmt, fd)] = 0 ;
        REDIRFD_1 = REDIRFD_2 = 0 ;
        log_dieusys(LOG_EXIT_SYS, "move fd ", fmt, " to fd: ",fdmt) ;
    }
}

void log_clock(buffer *stream)
{
    size_t secless = 22 ;
    char hstamp[32 + 2] ;
    char tstamp[TIMESTAMP + 2] ;
    tain now ;
    tain_wallclock_read(&now) ;
    char *pstr = 0 ;

    if (!CLOCK_TIMESTAMP)
    {
        timestamp_fmt(tstamp, &now) ;
        tstamp[TIMESTAMP] = ' ' ;
        tstamp[TIMESTAMP + 1] = 0 ;
        pstr = &tstamp[0] ;
    }
    else
    {
        localtmn l ;
        localtmn_from_tain(&l, &now, 1) ;
        localtmn_fmt(hstamp, &l) ;
        hstamp[secless] = ' ' ;
        hstamp[secless + 1] = 0 ;
        pstr = &hstamp[0] ;
    }
    buffer_puts(stream,pstr) ;
}

void log_out(char const *str[],...)
{
    int e = errno ;
    va_list alist ;
    va_start(alist,str) ;
    buffer_ref stream_1 = SWITCH_STREAM ? buffer_2 : buffer_1 ;
    buffer_ref stream_2 = SWITCH_STREAM ? buffer_1 : buffer_2 ;

    if (REDIRFD_1)
        redir_fd(1,REDIRFD_1) ;

    if (REDIRFD_2)
        redir_fd(2,REDIRFD_2) ;

    if (CLOCK_ENABLED)
    {
        log_clock(stream_2) ;
        if (DOUBLE_OUTPUT)
            log_clock(stream_1) ;
    }
    while (*str) {
        buffer_puts(stream_2, *str) ;
        if (DOUBLE_OUTPUT)
            buffer_puts(stream_1, *str) ;
        *str++ ;
    }
    if (TRAILING_NEWLINE)
    {
        buffer_putflush(stream_2, "\n", 1) ;
        if (DOUBLE_OUTPUT) buffer_putflush(stream_1, "\n", 1) ;
    }
    else
    {
        buffer_flush(stream_2) ;
        if (DOUBLE_OUTPUT) buffer_flush(stream_1) ;
    }
    va_end(alist) ;
    errno = e ;
}

void log_out_builder(log_dbg_info_t *db_info,int level, char const *msg[],char const *str[],...)
{
    if ((unsigned int)level <= VERBOSITY)
    {
        if (COLOR_ENABLED) log_color = &log_color_enable ;
        log_color_init() ;

        size_t len = 3 + environ_length(str) + environ_length(msg) + (size_t)LOG_SYS ;

        if (VERBOSITY >= LOG_LEVEL_DEBUG)
            len += 7 ;

        char line[UINT32_FMT] ;
        line[uint32_fmt(line,db_info->line)] = 0 ;

        char const *ap[len] ;
        uint8_t i = 0 ;
        if (DEFAULT_MSG) ap[i++] = PROG ;
        if (VERBOSITY >= LOG_LEVEL_DEBUG && level != LOG_LEVEL_FLOW)
        {
            ap[i++] = ": (" ;
            ap[i++] = db_info->func ;
            ap[i++] = "(): " ;
            ap[i++] = line ;
            ap[i++] = ")" ;
        }

        if (DEFAULT_MSG) {
            while (*msg)
                ap[i++] = *msg++ ;
        }

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
