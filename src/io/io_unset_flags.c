/*
 * io_unset_flags.c
 *
 * Copyright (c) 2024-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <fcntl.h>

int io_unset_flags(int fd, int flags)
{
    int f ;
    f = fcntl(fd, F_GETFL) ;
    if (f < 0 || fcntl(fd, F_SETFL, f & ~flags) < 0)
        return 0 ;

    return 1 ;
}
