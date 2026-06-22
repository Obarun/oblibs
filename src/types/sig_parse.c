/*
 * sig_parse.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 *
 * Native replacement for skalibs sig0_scan (sig.h). Parses a signal given as a
 * name ("TERM"), a strict decimal number ("15"), or either with a "SIG" prefix
 * ("SIGTERM", "SIG15"); the name match is case-insensitive. The name->number
 * table is the one from skalibs sig_number.c. Credit: skalibs (ISC, L. Bercot).
 */

#include <string.h>
#include <strings.h>
#include <signal.h>

#include <oblibs/types.h>

static struct { char const *name ; int number ; } const table[] =
{
    { "ABRT", SIGABRT },
    { "ALRM", SIGALRM },
    { "BUS", SIGBUS },
    { "CHLD", SIGCHLD },
#ifdef SIGCLD
    { "CLD", SIGCLD },
#endif
    { "CONT", SIGCONT },
#ifdef SIGEMT
    { "EMT", SIGEMT },
#endif
    { "FPE", SIGFPE },
    { "HUP", SIGHUP },
    { "ILL", SIGILL },
#ifdef SIGINFO
    { "INFO", SIGINFO },
#endif
    { "INT", SIGINT },
#ifdef SIGIO
    { "IO", SIGIO },
#endif
#ifdef SIGIOT
    { "IOT", SIGIOT },
#endif
    { "KILL", SIGKILL },
#ifdef SIGLOST
    { "LOST", SIGLOST },
#endif
    { "PIPE", SIGPIPE },
#ifdef SIGPOLL
    { "POLL", SIGPOLL },
#endif
#ifdef SIGPROF
    { "PROF", SIGPROF },
#endif
#ifdef SIGPWR
    { "PWR", SIGPWR },
#endif
    { "QUIT", SIGQUIT },
    { "SEGV", SIGSEGV },
#ifdef SIGSTKFLT
    { "STKFLT", SIGSTKFLT },
#endif
    { "STOP", SIGSTOP },
#ifdef SIGSYS
    { "SYS", SIGSYS },
#endif
    { "TERM", SIGTERM },
#ifdef SIGTRAP
    { "TRAP", SIGTRAP },
#endif
    { "TSTP", SIGTSTP },
    { "TTIN", SIGTTIN },
    { "TTOU", SIGTTOU },
#ifdef SIGUNUSED
    { "UNUSED", SIGUNUSED },
#endif
#ifdef SIGURG
    { "URG", SIGURG },
#endif
    { "USR1", SIGUSR1 },
    { "USR2", SIGUSR2 },
#ifdef SIGVTALRM
    { "VTALRM", SIGVTALRM },
#endif
#ifdef SIGWINCH
    { "WINCH", SIGWINCH },
#endif
#ifdef SIGXCPU
    { "XCPU", SIGXCPU },
#endif
#ifdef SIGXFSZ
    { "XFSZ", SIGXFSZ },
#endif
} ;

/* name -> signal number, case-insensitive; 0 if unknown. Linear search over the
 * unique-name table: same result as the skalibs bsearch, no ordering to keep. */
static int name_to_sig(char const *name)
{
    for (size_t i = 0 ; i < sizeof table / sizeof *table ; i++)
        if (!strcasecmp(name, table[i].name))
            return table[i].number ;
    return 0 ;
}

/* a bare name or a bare strict decimal number, no "SIG" prefix handling */
static size_t sig_parse_norec(char const *s, int *sig)
{
    int n = name_to_sig(s) ;
    if (n) {
        *sig = n ;
        return strlen(s) ;
    }

    uint32_t u ;
    size_t len = u32_scan_strict(s, &u) ;
    if (len)
        *sig = (int)u ;
    return len ;
}

size_t sig_parse(char const *s, int *sig)
{
    size_t len = sig_parse_norec(s, sig) ;
    if (len)
        return len ;

    if (!strncasecmp(s, "SIG", 3)) {
        len = sig_parse_norec(s + 3, sig) ;
        if (len)
            return 3 + len ;
    }

    return 0 ;
}
