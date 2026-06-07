/*
 * cdb_dataset.h
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

/*
 * Deterministic key/value dataset shared between the skalibs oracle (which
 * produces the golden cdb, see ../../../tools/cdb-oracle/) and the ocdb tests.
 * Both sides MUST build the exact same set of pairs so the byte-for-byte
 * comparison is meaningful. Keys are binary and made unique by a 4-byte
 * big-endian counter prefix; values are variable-length binary blobs. A
 * reproducible LCG (the MMIX constants, as in src/types/test/num.c) drives the
 * lengths and contents -- no rand(), identical on every run and every target.
 */

#ifndef OB_CDB_TEST_DATASET_H
#define OB_CDB_TEST_DATASET_H

#include <stdint.h>

#define CDB_DATASET_N 500

typedef struct cdb_pair_s
{
    unsigned char key[32] ;
    uint32_t klen ;
    unsigned char val[64] ;
    uint32_t vlen ;
} cdb_pair ;

static inline uint64_t cdb_ds_lcg(uint64_t *s)
{
    *s = *s * 6364136223846793005ULL + 1442695040888963407ULL ;
    return *s ;
}

/* Fill @a with CDB_DATASET_N deterministic, unique-keyed pairs. */
static inline void cdb_dataset_fill(cdb_pair *a)
{
    uint64_t s = 0x0123456789abcdefULL ;
    for (unsigned int i = 0 ; i < CDB_DATASET_N ; i++) {
        cdb_pair *p = a + i ;

        uint32_t tail = (uint32_t)(cdb_ds_lcg(&s) % 12) ;       /* 0..11 extra key bytes */
        p->klen = 4 + tail ;
        p->key[0] = (unsigned char)(i >> 24) ;                  /* unique prefix */
        p->key[1] = (unsigned char)(i >> 16) ;
        p->key[2] = (unsigned char)(i >> 8) ;
        p->key[3] = (unsigned char)i ;
        for (uint32_t j = 0 ; j < tail ; j++)
            p->key[4 + j] = (unsigned char)(cdb_ds_lcg(&s) & 0xff) ;

        p->vlen = (uint32_t)(cdb_ds_lcg(&s) % 41) ;             /* 0..40 value bytes */
        for (uint32_t j = 0 ; j < p->vlen ; j++)
            p->val[j] = (unsigned char)(cdb_ds_lcg(&s) & 0xff) ;
    }
}

#endif
