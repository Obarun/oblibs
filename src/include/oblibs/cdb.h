/*
 * cdb.h
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
 * domain) and from skalibs (https://skarnet.org/software/skalibs/ (ISC License).
 * The skalibs reading API (const cdb + find_state, cdb_p bounds-check, traverse) is kept;
 * the finish step uses DJB's heap allocation instead of the skalibs stack VLA.
 * Renamed (ocdb_*) and adapted for oblibs by Eric Vidal <eric@obarun.org> to
 * remove the skalibs dependency and to avoid symbol collision with libskarnet.
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
 * (a brace list is only valid in a declaration).*/
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
 *
 * Opens @path with `O_RDONLY | O_CLOEXEC | O_NONBLOCK` (retrying `open` on
 * `EINTR`), maps it through ocdb_init_fromfd(), then closes the fd. The fd is
 * always closed before returning, on both the success and the failure path; the
 * close preserves errno, so the failure errno reported here is the one from the
 * open or the mapping, never from the close. The mapping survives until
 * ocdb_free().
 *
 * @param[out] c    Filled with the mapping on success; left untouched on failure.
 * @param[in]  path Path to the cdb file. Must not be NULL.
 * @return 1 on success.
 * @return 0 on failure (errno set): the `open` errno if @path cannot be opened,
 *         otherwise the ocdb_init_fromfd() errno (the `fstat` errno, `EOVERFLOW`
 *         if the file size exceeds `UINT32_MAX`, or the `mmap` errno).
 * @see ocdb_init_fromfd
 */
extern int ocdb_init(ocdb *c, char const *path) ;

/**
 * @brief Like ocdb_init() but @path is resolved relative to directory fd @dirfd.
 *
 * Opens @path with `openat(dirfd, ..., O_RDONLY | O_CLOEXEC | O_NONBLOCK)`
 * (retrying on `EINTR`), maps it through ocdb_init_fromfd(), then closes the fd.
 * As with ocdb_init(), the fd is always closed before returning and the close
 * preserves errno. The mapping survives until ocdb_free().
 *
 * @param[out] c     Filled with the mapping on success; left untouched on failure.
 * @param[in]  dirfd Directory fd against which @path is resolved (or `AT_FDCWD`).
 * @param[in]  path  Path to the cdb file. Must not be NULL.
 * @return 1 on success.
 * @return 0 on failure (errno set): the `openat` errno if @path cannot be opened,
 *         otherwise the ocdb_init_fromfd() errno (the `fstat` errno, `EOVERFLOW`
 *         if the file size exceeds `UINT32_MAX`, or the `mmap` errno).
 * @see ocdb_init_fromfd
 */
extern int ocdb_init_at(ocdb *c, int dirfd, char const *path) ;

/**
 * @brief mmap an already-open, readable fd into @c.
 *
 * `fstat`s @fd, then maps its whole content `PROT_READ`/`MAP_SHARED` from offset
 * 0. The fd is NOT closed: it may be closed by the caller immediately afterwards
 * since the mapping keeps the pages valid until ocdb_free(). The cdb size is the
 * file size at the time of the call and is stored as a `uint32_t`; files larger
 * than `UINT32_MAX` (4 GiB minus one byte) are rejected, so a file of exactly
 * 4 GiB or more cannot be mapped.
 *
 * @param[out] c  Filled with the mapping (map base and size) on success; left
 *                untouched on failure.
 * @param[in]  fd An open fd readable and `fstat`-able. Need not be seekable.
 * @return 1 on success.
 * @return 0 on failure (errno set): the `fstat` errno if `fstat(fd)` fails;
 *         `EOVERFLOW` if the file size is greater than `UINT32_MAX`; otherwise
 *         the `mmap` errno.
 * @retval 0 errno is set to `EOVERFLOW` when the file size exceeds `UINT32_MAX`.
 */
extern int ocdb_init_fromfd(ocdb *c, int fd) ;

/**
 * @brief Release the mapping (no-op if @c is not mapped).
 *
 * `munmap`s the region and resets `c->map` to 0. errno is saved and restored, so
 * the call never clobbers a pending errno. Always succeeds (no return value).
 *
 * @param[in,out] c The cdb to unmap. If `c->map` is 0 the call does nothing.
 */
extern void ocdb_free(ocdb *c) ;

/**
 * @brief Find the first value stored under (@key, @len).
 *
 * Convenience wrapper that runs a single ocdb_findnext() over a fresh, zeroed
 * find state, returning only the first match. Use ocdb_findnext() directly to
 * walk further values sharing the same key.
 *
 * @param[in]  c   The mapped, immutable database.
 * @param[out] out On a return of 1, set to the found value: `out->s` points into
 *                 the mmap (not NUL-terminated) and `out->len` is its length. Not
 *                 modified when the return is 0 or -1.
 * @param[in]  key The key to look up. Read for @len bytes; may be NULL only if
 *                 @len is 0.
 * @param[in]  len Length of @key in bytes.
 * @return 1 if a value was found (@out filled).
 * @return 0 if no value is stored under @key.
 * @return -1 on a malformed database; errno is set to `EPROTO`. This is the only
 *         error: it is raised whenever a position/length read from the on-disk
 *         structures would fall outside the mapped region (failed bounds check).
 * @retval -1 errno is set to `EPROTO` on a malformed/out-of-bounds database.
 * @see ocdb_findnext
 */
extern int ocdb_find(ocdb const *c, ocdb_data *out, char const *key, uint32_t len) ;

/**
 * @brief Find the next value under (@key, @len), continuing from @d.
 *
 * Iterates the values sharing @key: on the first call (when `d->loop == 0`, e.g.
 * after OCDB_FIND_STATE_ZERO or ocdb_findstart()) it locates the key's bucket and
 * hash table; each subsequent call resumes from the position recorded in @d and
 * returns the next matching value. Returns 0 once the bucket is exhausted.
 *
 * @param[in]     c   The mapped, immutable database.
 * @param[out]    out On a return of 1, set to the found value: `out->s` points
 *                    into the mmap (not NUL-terminated) and `out->len` is its
 *                    length. Not modified when the return is 0 or -1.
 * @param[in]     key The key to look up. Read for @len bytes; may be NULL only if
 *                    @len is 0.
 * @param[in]     len Length of @key in bytes.
 * @param[in,out] d   Search cursor. Must be OCDB_FIND_STATE_ZERO (or reset with
 *                    ocdb_findstart()) before the first call for a given key, and
 *                    passed back unchanged on each subsequent call.
 * @return 1 if a value was found (@out filled, @d advanced).
 * @return 0 if there are no more values for @key (bucket empty or exhausted).
 * @return -1 on a malformed database; errno is set to `EPROTO`. This is the only
 *         error: it is raised whenever a position/length read from the on-disk
 *         structures would fall outside the mapped region (failed bounds check).
 * @retval -1 errno is set to `EPROTO` on a malformed/out-of-bounds database.
 */
extern int ocdb_findnext(ocdb const *c, ocdb_data *out, char const *key, uint32_t len, ocdb_find_state *d) ;

/**
 * @brief Read the next (key, data) record in insertion order.
 *
 * Sequential walk over the record area, independent of the hash tables. @pos is
 * the byte offset of the next record and is advanced past the record on a return
 * of 1; initialise it to OCDB_TRAVERSE_INIT (via ocdb_traverse_init()) before the
 * first call. The end of the record area is taken from the first table position
 * stored in the header.
 *
 * @param[in]     c    The mapped, immutable database.
 * @param[out]    key  On a return of 1, set to the record's key: `key->s` points
 *                     into the mmap (not NUL-terminated), `key->len` its length.
 * @param[out]    data On a return of 1, set to the record's data the same way.
 * @param[in,out] pos  Byte offset of the next record; advanced on a return of 1.
 *                     Start at OCDB_TRAVERSE_INIT.
 * @return 1 if a record was read (@key and @data filled, @pos advanced).
 * @return 0 at the end of the record area (no more records).
 * @return -1 on a malformed database; errno is set to `EPROTO`. Raised when the
 *         header end-of-data word, or the record header/payload at @pos, would
 *         fall outside the mapped region, or when advancing @pos by the record
 *         header would overflow 32 bits.
 * @retval -1 errno is set to `EPROTO` on a malformed/out-of-bounds database.
 */
extern int ocdb_traverse_next(ocdb const *c, ocdb_data *key, ocdb_data *data, uint32_t *pos) ;

/**
 * @brief Full djb hash of (@buf, @len).
 *
 * Computes the cdb hash: seed 5381, then for each byte `h = ((h << 5) + h) ^ byte`
 * with 32-bit wraparound. Bytes are read as `unsigned char`. Always succeeds.
 *
 * @param[in] buf Bytes to hash. May be NULL only if @len is 0.
 * @param[in] len Number of bytes to hash.
 * @return The 32-bit djb hash. For @len 0 the seed value 5381 is returned.
 */
extern uint32_t ocdb_hash(char const *buf, uint32_t len) ;

/**
 * @brief Fold one byte into a running djb hash.
 *
 * Applies one step of the cdb hash: `h = ((h << 5) + h) ^ c`, 32-bit wraparound.
 * Start a hash from the seed 5381 (see ocdb_hash()). Always succeeds.
 *
 * @param[in] h The running hash so far.
 * @param[in] c The byte to fold in.
 * @return The updated 32-bit hash.
 * @see ocdb_hash
 */
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
 * @brief Begin building a cdb on the seekable fd @fd.
 *
 * Resets the maker's `hplist` to empty and `pos` to 2048, initialises the
 * embedded ostream over @fd using the maker's own `buf`, then seeks @fd to offset
 * 2048 to leave room for the header that ocdb_make_finish() writes last. @fd must
 * be seekable and writable, and is positioned by this call; the caller is
 * responsible for having truncated it if reusing an existing file. The maker need
 * not be OCDBMAKER_ZERO beforehand since this call reinitialises the fields it
 * uses, but the maker must outlive the build (the ostream writes through `buf`).
 *
 * @param[out] c  The maker to initialise.
 * @param[in]  fd Seekable, writable fd to build the cdb on.
 * @return 1 on success.
 * @return 0 on failure (errno set): `EINVAL` if the ostream cannot be initialised,
 *         otherwise the `lseek` errno if seeking to offset 2048 fails.
 */
extern int ocdb_make_start(ocdbmaker *c, int fd) ;

/**
 * @brief Append a (key, data) record to the cdb being built.
 *
 * Writes the record header (key length, data length), the key and the data
 * through the buffered stream, records the (hash, position) pair in `hplist`, and
 * advances `pos`. On any failure the maker's `hplist` strbuf is freed (with
 * strbuf_free()) before returning; the embedded ostream and its caller-owned
 * `buf` are not touched. After a failure the maker must not be used further: do
 * not call ocdb_make_add() or ocdb_make_finish() on it again.
 *
 * @param[in,out] c    The maker, previously set up by ocdb_make_start().
 * @param[in]     key  The record key. Read for @klen bytes; may be NULL only if
 *                     @klen is 0.
 * @param[in]     klen Key length in bytes.
 * @param[in]     data The record data. Read for @dlen bytes; may be NULL only if
 *                     @dlen is 0.
 * @param[in]     dlen Data length in bytes.
 * @return 1 on success.
 * @return 0 on failure; errno is set and the maker's `hplist` is freed. errno is
 *         the ostream write errno when buffering the header, key or data fails;
 *         `ERANGE` if recording the (hash, position) pair would overflow the
 *         `hplist` size; `ENOMEM` if growing `hplist` fails or if the running
 *         write offset would wrap past 4 GiB; or the underlying `malloc` errno
 *         from growing `hplist`.
 * @retval 0 the maker's `hplist` strbuf is freed on every failure path.
 */
extern int ocdb_make_add(ocdbmaker *c, char const *key, uint32_t klen, char const *data, uint32_t dlen) ;

/**
 * @brief Write the hash tables and the header, finishing the cdb.
 *
 * Builds the 256 per-bucket open-addressing hash tables from the accumulated
 * (hash, position) pairs, appends them after the record area, flushes the stream,
 * seeks back to offset 0 and writes the 2048-byte header. The (hash, position)
 * pairs are laid out into a single heap buffer (`malloc`); on the success path
 * the maker's `hplist` strbuf is freed once that buffer has been populated. On a
 * failure that occurs before that point (`ENOMEM` paths below), `hplist` is left
 * allocated; on a failure after it, `hplist` is already freed. Either way the
 * maker must not be reused after this call.
 *
 * @param[in,out] c The maker, with all records already added via ocdb_make_add().
 * @return 1 on success (the cdb is fully written to the fd).
 * @return 0 on failure (errno set): `ENOMEM` if the total table size would exceed
 *         what `0xffffffff / sizeof(hash,pos pair)` allows, or if the `malloc` of
 *         the table buffer fails (errno from `malloc`); otherwise the ostream
 *         write errno (table emission, final flush, header write) or the `lseek`
 *         errno when rewinding to offset 0.
 * @retval 0 errno is set to `ENOMEM` when the computed table size overflows the
 *         per-element limit.
 */
extern int ocdb_make_finish(ocdbmaker *c) ;

#endif
