/* 
 * error.h
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

#ifndef OB_ERROR_H
#define OB_ERROR_H


#include <unistd.h>
#include <errno.h>

#include <skalibs/gccattributes.h>
#include <skalibs/buffer.h>

#include <oblibs/display.h>

extern void stdout_msg (char const *, char const *, char const *, char const *, char const *, char const *, char const *) ;
extern void stdout_die (int, char const *, char const *, char const *, char const *, char const *, char const *, char const *) gccattr_noreturn ;
extern void stdout_msgsys (char const *, char const *, char const *, char const *, char const *, char const *) ;
extern void stdout_diesys (int, char const *, char const *, char const *, char const *, char const *, char const *) gccattr_noreturn ;
extern void die_nomem(char *func) gccattr_noreturn ;

#define stdout_msg1(x1, se) \
stdout_msg(x1, 0, 0, 0, 0, 0, se)

#define stdout_msg2(x1, x2,se) \
stdout_msg(x1, x2, 0, 0, 0, 0, se)

#define stdout_msg3(x1, x2, x3, se) \
stdout_msg(x1, x2, x3, 0, 0, 0, se)

#define stdout_msg4(x1, x2, x3, x4, se) \
stdout_msg(x1, x2, x3, x4, 0, 0, se)

#define stdout_msg5(x1, x2, x3, x4, x5, se) \
stdout_msg(x1, x2, x3, x4, x5, 0, se)

#define stdout_msg6(x1, x2, x3, x4, x5, x6, se) \
stdout_msg(x1, x2, x3, x4, x5, x6, se)

#define stdout_msg1x(x1) \
stdout_msg((x1), 0, 0, 0, 0, 0, 0)

#define stdout_msg2x(x1, x2) \
stdout_msg(x1, (x2), 0, 0, 0, 0, 0)

#define stdout_msg3x(x1, x2, x3) \
stdout_msg(x1, x2, (x3), 0, 0, 0, 0)

#define stdout_msg4x(x1, x2, x3, x4) \
stdout_msg(x1, x2, x3, (x4), 0, 0, 0)

#define stdout_msg5x(x1, x2, x3, x4, x5) \
stdout_msg(x1, x2, x3, x4, (x5), 0, 0)

#define stdout_msg6x(x1, x2, x3, x4, x5, x6) \
stdout_msg(x1, x2, x3, x4, x5, (x6), 0)

#define stdout_msgw1x(x1) \
stdout_msg2x(obcolor.warning,(x1))

#define stdout_msgw2x(x1,x2) \
stdout_msg3x(obcolor.warning,(x1), x2)

#define stdout_msgw3x(x1,x2,x3) \
stdout_msg4x(obcolor.warning,(x1), x2, x3)

#define stdout_msgw4x(x1,x2,x3, x4) \
stdout_msg5x(obcolor.warning,(x1), x2, x3, x4)

#define stdout_msgw5x(x1, x2, x3, x4, x5) \
stdout_msg6x(obcolor.warning,(x1), x2, x3, x4, x6)

/*

#define stdout_msg2(x1, x2) \
stdout_msg(x1, x2, 0, 0, 0, 0, 0)

#define stdout_msg3(x1, x2, x3) \
stdout_msg(x1, x2, x3, 0, 0, 0, 0)

#define stdout_msg4(x1, x2, x3, x4) \
stdout_msg(x1, x2, x3, x4, 0, 0, 0)

#define stdout_msg5(x1, x2, x3, x4, x5) \
stdout_msg(x1, x2, x3, x4, x5, 0, 0)

#define stdout_msg1x(x1) \
stdout_msg2(obcolor.warning, (x1))

#define stdout_msg2x(x1, x2) \
stdout_msg3(obcolor.warning, x1, (x2))

#define stdout_msg3x(x1, x2, x3) \
stdout_msg4(obcolor.warning, x1, x2, (x3))

#define stdout_msg4x(x1, x2, x3, x4) \
stdout_msg5(obcolor.warning, x1, x2, x3, (x4))*/

/** warm message with errno */
#define stdout_msgsys3(x1, x2, x3) \
stdout_msgsys(x1, x2, (x3), 0, 0, 0)

#define stdout_msgsys4(x1, x2, x3, x4) \
stdout_msgsys(x1, x2, x3, (x4), 0, 0)

#define stdout_msgsys5(x1, x2, x3, x4, x5) \
stdout_msgsys(x1, x2, x3, x4, (x5), 0)

#define stdout_msgsys6(x1, x2, x3, x4, x5, x6) \
stdout_msgsys(x1, x2, x3, x4, x5, (x6), 0)

#define stdout_msgsys1x(x1) \
stdout_msgsys3(obcolor.warning, (x1), " : ")

#define stdout_msgsys2x(x1, x2) \
stdout_msgsys4(obcolor.warning, (x1), x2, " : ")

#define stdout_msgsys3x(x1, x2, x3) \
stdout_msgsys5(obcolor.warning, (x1), x2, x3, " : ")

#define stdout_msgsys4x(x1, x2, x3, x4) \
stdout_msgsys6(obcolor.warning, (x1), x2, x3, x4, " : ")

/** die message */
#define stdout_die2(e, x1, x2) \
stdout_die(e, x1, x2, 0, 0, 0, 0, 0)

#define stdout_die3(e, x1, x2, x3) \
stdout_die(e, x1, x2, (x3), 0, 0, 0, 0)

#define stdout_die4(e, x1, x2, x3, x4) \
stdout_die(e, x1, x2, x3, (x4), 0, 0, 0)

#define stdout_die5(e, x1, x2, x3, x4, x5) \
stdout_die(e, x1, x2, x3, (x4), 0, 0, 0)

#define stdout_die1x(e, x1) \
stdout_die2(e, obcolor.error, x1)

#define stdout_die2x(e, x1, x2) \
stdout_die3(e, obcolor.error, x1, x2)

#define stdout_die3x(e, x1, x2, x3) \
stdout_die4(e, obcolor.error, x1, x2, x3)

#define stdout_die4x(e, x1, x2, x3, x4) \
stdout_die5(e, obcolor.error, x1, x2, x3, x4)

/** die message with errno */
#define stdout_diesys3(e, x1, x2, x3) \
stdout_diesys(e, x1, x2, (x3), 0, 0, 0)

#define stdout_diesys4(e, x1, x2, x3, x4) \
stdout_diesys(e, x1, x2, x3, (x4), 0, 0)

#define stdout_diesys5(e, x1, x2, x3, x4, x5) \
stdout_diesys(e, x1, x2, x3, x4, (x5), 0)

/*
#define stdout_diesys6(e, x1, x2, x3, x4, x5, x6) \
stdout_diesys(e, x1, x2, x3, x4, x5, (x6))
*/

#define stdout_diesys1x(e, x1) \
stdout_diesys3(e, obcolor.error, (x1), " : ")

#define stdout_diesys2x(e, x1, x2) \
stdout_diesys4(e, obcolor.error, x1, (x2), " : ")

#define stdout_diesys3x(e, x1, x2, x3) \
stdout_diesys5(e, obcolor.error, x1, x2, (x3), " : ")
/*
#define stdout_diesys4x(e, x1, x2, x3, x4) \
stdout_diesys6(e, obcolor.error, x1, x2, x3, (x4), " : ")
*/

/** normal message */
#define stdout_normal1x(x1) \
stdout_msg2x(obcolor.normal, (x1))

#define stdout_normal2x(x1, x2) \
stdout_msg3x(obcolor.normal, (x1), x2)

#define stdout_normal3x(x1, x2, x3) \
stdout_msg4x(obcolor.normal, (x1), x2, x3)

#define stdout_normal4x(x1, x2, x3, x4) \
stdout_msg5x(obcolor.normal, (x1), x2, x3, x4)

/** info message */
#define stdout_info1x(x1) \
stdout_msg2x(obcolor.info, (x1))

#define stdout_info2x(x1, x2) \
stdout_msg3x(obcolor.info, (x1), x2)

#define stdout_info3x(x1, x2, x3) \
stdout_msg4x(obcolor.info, (x1), x2, x3)

#define stdout_info4x(x1, x2, x3, x4) \
stdout_msg5x(obcolor.info, (x1), x2, x3, x4)

/** valid message */
#define stdout_valid1x(x1) \
stdout_msg2x(obcolor.valid, (x1))

#define stdout_valid2x(x1, x2) \
stdout_msg3x(obcolor.valid, (x1), x2)

#define stdout_valid3x(x1, x2, x3) \
stdout_msg4x(obcolor.valid, (x1), x2, x3)

#define stdout_valid4x(x1, x2, x3, x4) \
stdout_msg5x(obcolor.valid, (x1), x2, x3, x4)

#endif
