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

set_color_t const obcolor = {
	.info = BOLD ":: " NOCOLOR,
	.valid = BGREEN ":: " NOCOLOR,
	.warning = BYELLOW ":warning: " NOCOLOR,
	.blink = BL_BBLUE ":: " NOCOLOR,
	.error = BRED ":fatal: " NOCOLOR,
	.off = NOCOLOR
};
