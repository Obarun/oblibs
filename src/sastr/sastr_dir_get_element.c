/*
 * sastr_dir_get_element.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/sastr.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

#include <skalibs/stralloc.h>
#include <skalibs/unix-transactional.h>

int sastr_dir_get_element(stralloc *sa, char const *srcdir,char const *element,mode_t mode,uint8_t include_path)
{
    size_t base = sa->len ;
    int wasnull = !sa->s ;
    int fdsrc, found = 0 ;

    DIR *dir = opendir(srcdir) ;
    if (!dir)
        return 0 ;

    fdsrc = dirfd(dir) ;

    for (;;)
    {
        struct stat st ;
        struct dirent *d ;
        d = readdir(dir) ;
        if (!d) break ;
        if (d->d_name[0] == '.')
        if (((d->d_name[1] == '.') && !d->d_name[2]) || !d->d_name[1])
            continue ;
        if (stat_at(fdsrc, d->d_name, &st) < 0)
            goto err ;
        mode_t r = get_flags(mode,st.st_mode) ;
        if (r > 0)
        {
            if (!strcmp(d->d_name,element)) {
                if (include_path) {
                    if (!stralloc_cats(sa,srcdir) ||
                    !stralloc_cats(sa,"/")) goto err ;
                }
                if (!stralloc_catb(sa,d->d_name,strlen(d->d_name) + 1)) goto err ;
                found++ ;
            }
        }
    }
    dir_close(dir) ;

    return found ;
    err:
        dir_close(dir) ;
        if (wasnull) stralloc_free(sa) ;
        else sa->len = base ;
        return 0 ;
}
