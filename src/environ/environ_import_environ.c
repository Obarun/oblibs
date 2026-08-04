/*
* environ_import_environ.c
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
#include <stddef.h>
#include <sys/types.h>
#include <errno.h>

#include <oblibs/strbuf.h>
#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/environ.h>

uint8_t environ_import_environ(strbuf *modifs, char const *const *env)
{
    if (!env)
        return (errno = EINVAL, 0) ;

    size_t pos = 0 ;

    for (; env[pos] ; pos++) {

        ssize_t r = get_len_until(env[pos], '=') ;

        if (r <= 0) {
            log_warn("environment entry holds no key -- ignoring it: ", env[pos]) ;
            continue ;
        }

        if (get_len_until(env[pos] + r + 1, '\n') >= 0) {

            _alloc_strbuf_(key, (size_t)r + 1) ;

            if (!strbuf_copyb(&key, env[pos], (size_t)r) ||
                !strbuf_uncounted(&key))
                    return 0 ;

            log_warn("environment variable holds a newline -- ignoring it: ", key.s) ;
            continue ;
        }

        if (!strbuf_cats(modifs, env[pos]) || !strbuf_terminate(modifs))
            return 0 ;
    }

    return 1 ;
}
