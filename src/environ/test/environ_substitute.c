#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <oblibs/strbuf.h>
#include <oblibs/environ.h>

void compare(strbuf *sa, const char *a, const char *b)
{
    subst_t info = SUBST_ZERO ;
    sa->len = 0 ;
    assert(environ_merge_string(sa, a) == 1) ;
    assert(environ_substitute(sa, &info) == 1) ;
    assert(environ_rebuild(sa) == 1) ;
    assert(strbuf_terminate(sa) == 1) ;
    assert(strcmp(sa->s, b) == 0) ;
    subst_free(&info) ;
}

void basic()
{
    printf("Running test basic...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Args=!-n ${ArgsConfFile}\n\
ArgsConfFile=!-f /etc/bluetooth/main.conf" ;

    const char *b = "Args=!-n -f /etc/bluetooth/main.conf\n\
ArgsConfFile=!-f /etc/bluetooth/main.conf" ;

    compare(&sa, a, b) ;
}

void basic1()
{
    printf("Running test basic1...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Args=!%%tmpfiles%%/opentmpfiles.sh ${ArgsScript}\n\
ArgsScript=!--create --verbose --boot" ;

    const char *b = "Args=!%%tmpfiles%%/opentmpfiles.sh --create --verbose --boot\n\
ArgsScript=!--create --verbose --boot" ;

    compare(&sa, a, b) ;
}

void basic2()
{
    printf("Running test basic2...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Args=!--session --print-pid=4 --nofork --nopidfile --address=unix:path=${Socket}\n\
Socket=!@R/bus" ;

    const char *b = "Args=!--session --print-pid=4 --nofork --nopidfile --address=unix:path=@R/bus\n\
Socket=!@R/bus" ;

    compare(&sa, a, b) ;
}

void basic3()
{
    printf("Running test basic3...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Args=!${ArgsConfFile} -aN\n\
ArgsConfFile=!-c /etc/zfs/zpool.cache" ;

    const char *b = "Args=!-c /etc/zfs/zpool.cache -aN\n\
ArgsConfFile=!-c /etc/zfs/zpool.cache" ;

    compare(&sa, a, b) ;
}

void complex()
{
    printf("Running test complex...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Args=!--nodaemonize ${ArgsConfFile} ${ArgsPidFile}\n\
ArgsConfFile=!--fpm-config /etc/php/php-fpm.conf\n\
LiveDirectory=!/run/php-fpm\n\
ArgsPidFile=!--pid ${LiveDirectory}/php-fpm.pid" ;

    const char *b = "Args=!--nodaemonize --fpm-config /etc/php/php-fpm.conf --pid /run/php-fpm/php-fpm.pid\n\
ArgsConfFile=!--fpm-config /etc/php/php-fpm.conf\n\
LiveDirectory=!/run/php-fpm\n\
ArgsPidFile=!--pid /run/php-fpm/php-fpm.pid" ;

    compare(&sa, a, b) ;
}

void complex1()
{
    printf("Running test complex1...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "ArgsStart=!-d -v ${ArgsConfFile} ${ArgsPidFile}\n\
ArgsStop=!-r\n\
ArgsConfFile=!-cf ${ConfDirectory}/${ConfFile}\n\
ArgsPidFile=!-pf ${PidFile}\n\
ConfDirectory=!/etc/dhcp\n\
ConfFile=dhclient-@I.conf\n\
LiveDirectory=!/run/dhclient-@I\n\
PidFile=${LiveDirectory}/dhclient.pid" ;

    const char *b = "ArgsStart=!-d -v -cf /etc/dhcp/dhclient-@I.conf -pf /run/dhclient-@I/dhclient.pid\n\
ArgsStop=!-r\n\
ArgsConfFile=!-cf /etc/dhcp/dhclient-@I.conf\n\
ArgsPidFile=!-pf /run/dhclient-@I/dhclient.pid\n\
ConfDirectory=!/etc/dhcp\n\
ConfFile=dhclient-@I.conf\n\
LiveDirectory=!/run/dhclient-@I\n\
PidFile=/run/dhclient-@I/dhclient.pid" ;

    compare(&sa, a, b) ;
}

void nope()
{
    printf("Running test nope...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "ConfFile=!%%initconf%%\n\
Timeout=!60000\n\
Verbosity=3" ;

    const char *b = "ConfFile=!%%initconf%%\n\
Timeout=!60000\n\
Verbosity=3" ;

    compare(&sa, a, b) ;
}

void comment()
{
    printf("Running test comment...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "ConfFile=!%%initconf%%\n\
## first comment\n\
## Second comment\n\
Timeout=!60000\n\
Verbosity=3" ;

    const char *b = "ConfFile=!%%initconf%%\n\
Timeout=!60000\n\
Verbosity=3" ;

    compare(&sa, a, b) ;
}

void unknown()
{
    printf("Running test unknown...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Socket=/run/libvirt/libvirt-sock ${unknown}" ;

    const char *b = "Socket=/run/libvirt/libvirt-sock ${unknown}" ;

    compare(&sa, a, b) ;
}

void empty_line()
{
    printf("Running test empty_line...\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    const char *a = "Socket=/run/libvirt/libvirt-sock ${empty}\n\
\n\
\n\
empty=t\n\n" ;

    const char *b = "Socket=/run/libvirt/libvirt-sock t\n\
empty=t" ;

    compare(&sa, a, b) ;
}

/* Indirect cycle: dollar_replace tracks the keys expanded in one chain, so a
 * two-step (or longer) cycle is caught instead of looping forever -- value used
 * to oscillate between ${A} and ${B}. The reaching of this assertion at all is
 * the regression guard: a re-introduced loop would hang the suite. The cyclic
 * variables are left literal and an indirect cycle is reported as failure (0),
 * unlike a direct self-reference which succeeds (1) and is left as-is. */
void cyclic_reference()
{
    printf("Running test cyclic_reference...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    subst_t info = SUBST_ZERO ;
    assert(environ_merge_string(&sa, "B=${A}\nA=${B}") == 1) ;
    assert(environ_substitute(&sa, &info) == 0) ;        /* terminates, not a hang */
    assert(environ_rebuild(&sa) == 1) ;
    assert(strbuf_terminate(&sa) == 1) ;
    assert(strcmp(sa.s, "B=${A}\nA=${B}") == 0) ;        /* left literal */
    subst_free(&info) ;

    /* a longer cycle terminates too */
    sa.len = 0 ;
    subst_t info2 = SUBST_ZERO ;
    assert(environ_merge_string(&sa, "A=${B}\nB=${C}\nC=${A}") == 1) ;
    assert(environ_substitute(&sa, &info2) == 0) ;
    subst_free(&info2) ;
}

/* Direct self-reference A=${A}: the anti-loop guard in dollar_replace
 * ("if (!strcmp(mkey, regex.s)) ... continue") catches a variable that refers to
 * itself and leaves the ${A} literal instead of recursing forever. Without the
 * guard this would loop; with it, A=${A} comes out verbatim. */
void self_reference()
{
    printf("Running test self_reference...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    compare(&sa, "A=${A}", "A=${A}") ;
    /* same guard, with surrounding literal text kept intact */
    sa.len = 0 ;
    compare(&sa, "A=x${A}y", "A=x${A}y") ;
}

/* Escaping is DEAD at the config-value pipeline (environ_merge -> environ_trim ->
 * environ_substitute): environ_trim consumes the backslash inside the quotes
 * before substitution ever runs, so "\${VAR}" is indistinguishable from ${VAR}
 * and the variable IS substituted. This pins the documented behavior -- a
 * backslash does NOT protect a variable here (escaping only survives in the
 * run-script pipeline, a different code path). */
void escape_is_dead()
{
    printf("Running test escape_is_dead...\n") ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    const char *a = "Key=\"\\${ArgsConfFile}\"\n\
ArgsConfFile=!-f /etc/bluetooth/main.conf" ;
    /* the backslash is gone after trim, so Key gets the substituted value */
    const char *b = "Key=-f /etc/bluetooth/main.conf\n\
ArgsConfFile=!-f /etc/bluetooth/main.conf" ;
    compare(&sa, a, b) ;
}

int main() {

    basic() ;
    basic1() ;
    basic2() ;
    basic3() ;
    complex() ;
    complex1() ;
    nope() ;
    comment() ;
    unknown() ;
    empty_line() ;
    cyclic_reference() ;
    self_reference() ;
    escape_is_dead() ;

    printf("All tests passed successfully.\n") ;

    return 0 ;
}
