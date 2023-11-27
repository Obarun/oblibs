/*
 * getopt_args.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/obgetopt.h>

#include <skalibs/sgetopt.h>

int getopt_args (int argc, char const *const *argv, char const *opts, subgetopt *o)
{
    o->arg = 0 ;

    if ((o->ind >= argc) || !argv[o->ind]) return -1 ;

    if (o->ind == 1 && !o->pos)
        if((argv[o->ind][o->pos] != '-') && (opts[0] !='>'))
            return -2 ;

    if (o->pos && !argv[o->ind][o->pos])
    {
        o->ind++ ;
        o->pos = 0 ;
        if ((o->ind >= argc) || !argv[o->ind]) return -1 ;
    }

    if (!o->pos)
    {
        if (argv[o->ind][0] != '-') return -1 ;
        else
        {
            char c ;
            o->pos++ ;
            c = argv[o->ind][1] ;
            if (c == '-') o->ind++ ;
            if (!c || (c == '-'))
            {
                o->pos = 0 ;
                return -1 ;
            }
        }
    }
    {
        int oldind ;
        char c = argv[o->ind][o->pos++] ;
        char const *s = opts ;
        char retnoarg = (*s == ':') ? (s++, ':') : '?' ;
        while (*s)
        {
            if (c == *s)
            {
                if (s[1] == ':')
                {
                    o->arg = argv[o->ind++] + o->pos ;
                    o->pos = 0 ;

                    if (!*o->arg)
                    {
                        o->arg = argv[o->ind] ;
                        if ((o->ind >= argc) || !o->arg || (*o->arg == '-'))
                        {
                            o->problem = c ;
                            return retnoarg ;
                        }

                        o->ind++ ;
                        /** avoid to have args in opts like -opts foo foo -opts */
                        if ( (argv[o->ind]) && argv[o->ind][o->pos] != '-' )
                        {
                            oldind = o->ind ;

                            while(o->ind < (unsigned int)argc-1 )
                            {
                                if ( argv[o->ind][0] == '-')
                                {
                                    o->problem = c ;
                                    return retnoarg ;
                                }
                                o->ind++ ;
                            }
                            o->ind = oldind ;
                        }

                    }

                }

                if (o->pos)
                {
                    oldind = o->ind ;
                    if ( (argv[o->ind+1]) && argv[o->ind+1][0] != '-'  )
                    {
                        o->ind++ ;
                        while(o->ind < (unsigned int)argc-1 )
                        {
                            if ( argv[o->ind][0] == '-')
                            {
                                o->problem = c ;
                                return retnoarg ;
                            }
                            o->ind++ ;
                        }
                        o->ind = oldind ;
                    }
                }

            return c ;
            }
            if (*++s == ':') s++ ;
        }
        o->problem = c ;
    }
    return '?' ;
}
