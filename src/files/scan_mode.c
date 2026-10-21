/*
 * scan_mode.c
 *
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>

#include <oblibs/types.h>

ssize_t scan_mode(char const *str, uint32_t flags)
{
    struct stat st ;

    if (!flags || flags & ~FLAGS_ALL)
        return (errno = EINVAL, 0) ;

    if (stat(str, &st) == -1)
        return 0 ;

    if (!(mode_to_flags(st.st_mode) & flags))
        return (errno = EINVAL, -1) ;

    return 1 ;
}
