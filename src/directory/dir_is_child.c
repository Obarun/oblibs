/*
 * dir_is_child.c
 *
 * Copyright (c) 2020 Eric Vidal <eric@obarun.org>
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

#include <oblibs/string.h>
#include <oblibs/directory.h>

int dir_is_child(char const *parent, char const *child)
{
    if ((parent[0] != '/') || (child[0] != '/'))
        return 0 ;

    /* str_start_with() follows the strcmp convention: it returns 0 when @child
     * starts with @parent (non-zero otherwise). The previous code tested `!r`,
     * which inverted the logic and made dir_is_child() ALWAYS report "not a
     * child" -- a silent no-op. Fixed to `if (r)`.
     *
     * NOTE FOR CONSUMER MIGRATION: this changes behaviour. Callers that relied
     * (knowingly or not) on dir_is_child() never matching -- e.g. 66-ns child
     * mount detection `dir_is_child(...) > 0`, and sbl_dir_drop_child() -- will
     * START detecting children once they link against this. Re-check those
     * sites when migrating. */
    int r = str_start_with(child,parent) ;

    if (r) return 0 ;

    size_t len = strlen(parent) ;

    if (parent[len - 1] ==  '/') len-- ;

    if ((child[len] != '/'))
        return 0 ;

    return 1 ;

}
