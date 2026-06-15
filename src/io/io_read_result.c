/*
 * io_read_result.h
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/types.h>
#include <errno.h>

#include <oblibs/io.h>

ssize_t io_read_result (ssize_t r)
{
  return r == -1 ? error_isagain(errno) ? (errno = 0, 0) : -1 : !r ? (errno = EPIPE, -1) : r ;
}
