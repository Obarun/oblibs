#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <skalibs/stralloc.h>
#include <oblibs/environ.h>
#include <oblibs/stack.h>
#include <oblibs/sastr.h>

void compare(stralloc *sa, const char *a, const char *b)
{
    subst_t info = SUBST_ZERO ;
    sa->len = 0 ;
    assert(environ_merge_string(sa, a) == 1) ;
    assert(environ_substitute(sa, &info) == 1) ;
    assert(environ_rebuild(sa) == 1) ;
    assert(stralloc_0(sa) == 1) ;
    assert(strcmp(sa->s, b) == 0) ;
    subst_free(&info) ;
}

void basic()
{
    printf("Running test basic...\n") ;

    _alloc_sa_(sa) ;

    const char *a = "Args=!-n ${ArgsConfFile}\n\
ArgsConfFile=!-f /etc/bluetooth/main.conf" ;

    const char *b = "Args=!-n -f /etc/bluetooth/main.conf\n\
ArgsConfFile=!-f /etc/bluetooth/main.conf" ;

    compare(&sa, a, b) ;
}

void basic1()
{
    printf("Running test basic1...\n") ;

    _alloc_sa_(sa) ;

    const char *a = "Args=!%%tmpfiles%%/opentmpfiles.sh ${ArgsScript}\n\
ArgsScript=!--create --verbose --boot" ;

    const char *b = "Args=!%%tmpfiles%%/opentmpfiles.sh --create --verbose --boot\n\
ArgsScript=!--create --verbose --boot" ;

    compare(&sa, a, b) ;
}

void basic2()
{
    printf("Running test basic2...\n") ;

    _alloc_sa_(sa) ;

    const char *a = "Args=!--session --print-pid=4 --nofork --nopidfile --address=unix:path=${Socket}\n\
Socket=!@R/bus" ;

    const char *b = "Args=!--session --print-pid=4 --nofork --nopidfile --address=unix:path=@R/bus\n\
Socket=!@R/bus" ;

    compare(&sa, a, b) ;
}

void basic3()
{
    printf("Running test basic3...\n") ;

    _alloc_sa_(sa) ;

    const char *a = "Args=!${ArgsConfFile} -aN\n\
ArgsConfFile=!-c /etc/zfs/zpool.cache" ;

    const char *b = "Args=!-c /etc/zfs/zpool.cache -aN\n\
ArgsConfFile=!-c /etc/zfs/zpool.cache" ;

    compare(&sa, a, b) ;
}

void complex()
{
    printf("Running test complex...\n") ;

    _alloc_sa_(sa) ;

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

    _alloc_sa_(sa) ;

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

    _alloc_sa_(sa) ;

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

    _alloc_sa_(sa) ;

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

    _alloc_sa_(sa) ;

    const char *a = "Socket=/run/libvirt/libvirt-sock ${unknown}" ;

    const char *b = "Socket=/run/libvirt/libvirt-sock ${unknown}" ;

    compare(&sa, a, b) ;
}

void empty_line()
{
    printf("Running test empty_line...\n") ;

    _alloc_sa_(sa) ;

    const char *a = "Socket=/run/libvirt/libvirt-sock ${empty}\n\
\n\
\n\
empty=t\n\n" ;

    const char *b = "Socket=/run/libvirt/libvirt-sock t\n\
empty=t" ;

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

    printf("All tests passed successfully.\n") ;

    return 0 ;
}
