/* 
 * obgetopt.h
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
 
#ifndef OBGETOPT_H
#define OBGETOPT_H

#include <skalibs/sgetopt.h>

/** This function is a modified version of subgetopt_r function coming
 * from skarnet library.
 * Copyright (c) 2011-2018 by Laurent Bercot <ska-skaware@skarnet.org>.
 * Modified by Eric Vidal <eric@obarun.org>.
 * The original version do not allow multiple arguments in the command
 * line after the definition of the options, this version does.
 * Also this version return -2 if the first arguments do not begin by
 * a '-' character.*/
extern int getopt_args (int argc, char const *const *argv, char const *opts, subgetopt_t *o) ;

#endif
