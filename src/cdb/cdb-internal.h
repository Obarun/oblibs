/*
 * cdb-internal.h
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_CDB_INTERNAL_H
#define OB_CDB_INTERNAL_H

#include <stdint.h>

#include <oblibs/cdb.h>

#define OCDB_HASHSTART 5381    /* djb hash seed */
#define OCDB_HEADER 2048       /* fixed header size: 256 * (u32 pos + u32 len) */

/* A (hash, position) pair, as accumulated during build and laid out in the hash
 * tables. Stored as raw bytes inside ocdbmaker.hplist. */
typedef struct ocdb_hp_s ocdb_hp ;
struct ocdb_hp_s
{
    uint32_t h ;
    uint32_t p ;
} ;

/**
 * @brief Bounds-checked pointer into the map: returns map + pos if the @len
 * bytes starting at @pos lie within the database, 0 otherwise. The (len, pos)
 * argument order matches the skalibs cdb_p it derives from.
 */
extern char const *ocdb_p(ocdb const *c, uint32_t len, uint32_t pos) ;

#endif
