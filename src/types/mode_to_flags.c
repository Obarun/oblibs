/*
 * mode_to_flags.c
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
 */

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <oblibs/types.h>

uint32_t mode_to_flags(mode_t st_mode)
{
    switch (st_mode & S_IFMT) {
        case S_IFREG: return FLAGS_REG ;
        case S_IFDIR: return FLAGS_DIR ;
        case S_IFLNK: return FLAGS_LNK ;
        case S_IFCHR: return FLAGS_CHR ;
        case S_IFBLK: return FLAGS_BLK ;
        case S_IFIFO: return FLAGS_FIFO ;
        case S_IFSOCK: return FLAGS_SOCK ;
        default: return 0 ;
    }
}
