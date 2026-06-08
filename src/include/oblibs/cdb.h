/*
 * cdb.h
 *
 * Copyright (c) 2018-2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_CDB_H
#define OB_CDB_H

#include <stdint.h>

#include <oblibs/strbuf.h>
#include <oblibs/stream.h>

/* ===========================================================================
 * cdb -- constant database (D. J. Bernstein format), self-contained (no skalibs
 * dependency).
 *
 * On-disk format (32-bit, little-endian), byte-identical to DJB cdb and to the
 * skalibs cdb/cdbmake it replaces:
 *   - a 2048-byte header: 256 entries of (table position : u32, slot count : u32)
 *   - records from offset 2048: (key length : u32, data length : u32, key, data)
 *   - one open-addressing hash table per 256 bucket: slots of (hash : u32, pos : u32)
 *   - djb hash: h = 5381 ; for each byte h = ((h << 5) + h) ^ byte (32-bit wrap)
 *
 * Reading is mmap-only and keeps the cdb immutable: the mutable search state
 * lives in a separate ocdb_find_state, so the same ocdb can serve concurrent or
 * reentrant lookups. Building accumulates the (hash, position) pairs in a strbuf
 * and lays out the hash tables on the heap at finish time.
 *
 * The design, format and djb hash come from D. J. Bernstein's cdb (public
 * domain) and from skalibs (https://skarnet.org/software/skalibs/ , ISC, all
 * credits to Laurent Bercot). The skalibs reading API (const cdb + find_state,
 * cdb_p bounds-check, traverse) is kept; the finish step uses DJB's heap
 * allocation instead of the skalibs stack VLA. Renamed (ocdb_*) and adapted for
 * oblibs by Eric Vidal <eric@obarun.org> to remove the skalibs dependency and to
 * avoid symbol collision with libskarnet.
 * ===========================================================================*/

/* ---- reading ---- */

/**
 * @brief A memory-mapped constant database, immutable once initialized.
 * @param map  Base of the mmap, 0 when not mapped.
 * @param size Mapped size in bytes.
 */
typedef struct ocdb_s ocdb, *ocdb_ref ;
struct ocdb_s
{
    char const *map ;
    uint32_t size ;
} ;
#define OCDB_ZERO { .map = 0, .size = 0 }

/* A const zero value, for re-assignment of an already-declared ocdb
 * (`c = ocdb_zero ;`), where the OCDB_ZERO brace-initialiser cannot be used
 * (a brace list is only valid in a declaration). Mirrors the skalibs `cdb_zero`
 * global, so a 66 `c = cdb_zero ;` migrates straight to `c = ocdb_zero ;`. */
extern ocdb const ocdb_zero ;

/**
 * @brief A value (or key) returned by a lookup/traversal: a pointer into the
 * mmap and its length. Not NUL-terminated.
 */
typedef struct ocdb_data_s ocdb_data ;
struct ocdb_data_s
{
    char const *s ;
    uint32_t len ;
} ;

/**
 * @brief Mutable state of an in-progress lookup, kept out of the ocdb so the
 * latter can stay const and be shared. Initialize with OCDB_FIND_STATE_ZERO (or
 * ocdb_findstart) before the first ocdb_findnext on a key.
 */
typedef struct ocdb_find_state_s ocdb_find_state ;
struct ocdb_find_state_s
{
    uint32_t loop ;    /* number of hash slots already probed for this key */
    uint32_t khash ;   /* hash of the searched key */
    uint32_t kpos ;    /* current slot position in the bucket's hash table */
    uint32_t hpos ;    /* start position of the bucket's hash table */
    uint32_t hslots ;  /* number of slots in the bucket's hash table */
} ;
#define OCDB_FIND_STATE_ZERO { 0, 0, 0, 0, 0 }

/** @brief Reset a find state to start a fresh search for a key. */
#define ocdb_findstart(d) ((d)->loop = 0)

/** @brief Starting position for a traversal (the first record). */
#define OCDB_TRAVERSE_INIT 2048
#define ocdb_traverse_init(pos) (*(pos) = 2048)

/**
 * @brief Open @path read-only and mmap it into @c.
 * @return 1 on success, 0 on failure (errno set). The fd is closed; the mapping
 *         survives until ocdb_free().
 */
extern int ocdb_init(ocdb *c, char const *path) ;

/**
 * @brief Like ocdb_init but @path is resolved relative to directory fd @dirfd.
 * @return 1 on success, 0 on failure (errno set).
 */
extern int ocdb_init_at(ocdb *c, int dirfd, char const *path) ;

/**
 * @brief mmap an already-open, seekable fd into @c. The fd may be closed by the
 *        caller afterwards; the mapping stays valid until ocdb_free().
 * @return 1 on success, 0 on failure (errno set; EOVERFLOW if size > 4 GiB).
 */
extern int ocdb_init_fromfd(ocdb *c, int fd) ;

/** @brief Release the mapping (no-op if not mapped). */
extern void ocdb_free(ocdb *c) ;

/**
 * @brief Find the first value stored under (@key, @len).
 * @param out Written with the value (pointer into the map + length) when found.
 * @return 1 found, 0 not found, -1 on a malformed database (errno == EPROTO).
 */
extern int ocdb_find(ocdb const *c, ocdb_data *out, char const *key, uint32_t len) ;

/**
 * @brief Find the next value under (@key, @len), continuing from @d. Allows
 *        iterating over multiple values sharing the same key.
 * @return 1 found, 0 no more, -1 on a malformed database (errno == EPROTO).
 */
extern int ocdb_findnext(ocdb const *c, ocdb_data *out, char const *key, uint32_t len, ocdb_find_state *d) ;

/**
 * @brief Iterate over every (key, data) record in insertion order. Start with
 *        *@pos == OCDB_TRAVERSE_INIT.
 * @return 1 a record was read (and @pos advanced), 0 at end, -1 if malformed.
 */
extern int ocdb_traverse_next(ocdb const *c, ocdb_data *key, ocdb_data *data, uint32_t *pos) ;

/** @brief Full djb hash of (@buf, @len). */
extern uint32_t ocdb_hash(char const *buf, uint32_t len) ;
/** @brief Fold one byte into a running djb hash. */
extern uint32_t ocdb_hashadd(uint32_t h, unsigned char c) ;

/* ---- writing ---- */

/**
 * @brief State of a cdb being built. Initialize with OCDBMAKER_ZERO, then call
 * ocdb_make_start; the embedded ostream is set up by ocdb_make_start.
 * @param hplist (hash, position) pairs accumulated as an append-only array.
 * @param pos    Current write offset in the file (starts at 2048).
 * @param b      Buffered output over the target fd.
 * @param buf    Backing storage for @b.
 */
typedef struct ocdbmaker_s ocdbmaker, *ocdbmaker_ref ;
struct ocdbmaker_s
{
    strbuf hplist ;
    uint32_t pos ;
    ostream b ;
    char buf[8192] ;
} ;
#define OCDBMAKER_ZERO { .hplist = STRBUF_ZERO, .pos = 2048 }

/**
 * @brief Begin building a cdb on the seekable fd @fd (truncated/positioned by
 *        the caller). Initializes the maker and seeks past the header.
 * @return 1 on success, 0 on failure (errno set).
 */
extern int ocdb_make_start(ocdbmaker *c, int fd) ;

/**
 * @brief Append a (key, data) record to the cdb being built.
 * @return 1 on success, 0 on failure (errno set; the maker is freed on failure).
 */
extern int ocdb_make_add(ocdbmaker *c, char const *key, uint32_t klen, char const *data, uint32_t dlen) ;

/**
 * @brief Write the hash tables and the header, finishing the cdb. Flushes and
 *        rewrites the 2048-byte header at offset 0.
 * @return 1 on success, 0 on failure (errno set).
 */
extern int ocdb_make_finish(ocdbmaker *c) ;

#endif
