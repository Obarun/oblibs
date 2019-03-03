/* 
 * obstr_color.c
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

#include <string.h>

#include <oblibs/display.h>

obstr_color const obcolor = {
	.normal = BOLD ":: " NOCOLOR,
	.valid = BGREEN ":: " NOCOLOR,
	.info = BBLUE ":: " NOCOLOR,
	.warning = BYELLOW ":warning: " NOCOLOR,
	.error = BRED ":fatal: " NOCOLOR
};
