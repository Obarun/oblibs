/*
 * genbuf.h
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
 *
 * A genbuf is a strbuf reinterpreted as a growable array of fixed-size typed
 * elements -- the oblibs counterpart of skalibs' genalloc (which is, likewise,
 * a stralloc plus a few typed macros). Same concept and same ergonomics: the
 * element type is named at each operation (C has no generics), and the length
 * is expressed in ELEMENTS. A genbuf IS a strbuf, so it grows and frees exactly
 * like one.
 */

#ifndef OB_GENBUF_H
#define OB_GENBUF_H

#include <oblibs/strbuf.h>

typedef strbuf genbuf, *genbuf_ref ;

#define GENBUF_ZERO STRBUF_ZERO

/** Typed base pointer to the elements (cast of the underlying storage). */
#define genbuf_s(type, g) ((type *)(g)->s)

/** Number of elements currently held. */
#define genbuf_len(type, g) ((g)->len / sizeof(type))

/** Append one element pointed to by @e. @return 1 on success, 0 on error. */
#define genbuf_append(type, g, e) strbuf_catb((g), (char const *)(e), sizeof(type))

/** Release the storage and reset to GENBUF_ZERO. */
#define genbuf_free(g) strbuf_free(g)

#endif
