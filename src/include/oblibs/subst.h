/*
 * subst.h
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
 * Variable substitution, oblibs-native replacement for execline's
 * el_substitute/exlsn_t machinery. Resolves ${VAR} and $VAR references in a
 * word list against a resolved environment, honouring backslash escaping
 * (\${VAR} -> literal ${VAR}). The state machine is a faithful port of
 * execline's, restricted to the value-count domain oblibs produces (0 or 1
 * value per variable), so the output is byte-for-byte identical to
 * el_substitute on that domain -- proven by a differential test.
 *
 * subst_t replaces exlsn_t: oblibs now owns the type and its free function,
 * which is what lets the per-variable array be a genbuf (our own) instead of a
 * skalibs genalloc locked by libexecline's ABI.
 */

#ifndef OB_SUBST_H
#define OB_SUBST_H

#include <sys/types.h>
#include <stddef.h>

#include <oblibs/strbuf.h>
#include <oblibs/genbuf.h>

/* One resolved variable: name at @var into ctx->vars, value at @value into
 * ctx->values, @n the number of values it expands to (0 or 1 here). Same
 * layout as execline's elsubst_t. */
typedef struct subst_entry_s
{
    size_t var ;
    size_t value ;
    unsigned int n ;
} subst_entry_t ;

/* Resolved substitution context (replaces exlsn_t). Fully strbuf/genbuf:
 * no skalibs, no execline. */
typedef struct subst_s
{
    strbuf vars ;     /* variable names, NUL-separated   */
    strbuf values ;   /* resolved values, NUL-separated  */
    genbuf data ;     /* array of subst_entry_t       */
    strbuf modifs ;   /* unexport list (read by callers) */
} subst_t ;

#define SUBST_ZERO { \
    .vars = STRBUF_ZERO, .values = STRBUF_ZERO, \
    .data = GENBUF_ZERO, .modifs = STRBUF_ZERO }

/** @brief Release every buffer of @ctx and reset it to SUBST_ZERO. */
extern void subst_free(subst_t *ctx) ;

/**
 * @brief Substitute ${VAR}/$VAR in @src using the resolved context @ctx.
 *
 * @src is a list of @len bytes holding NUL-separated words; each word has its
 * variable references replaced (backslash escaping honoured) and the resulting
 * NUL-separated words are appended to @dst (existing @dst content is preserved).
 *
 * @return the number of words produced, or -1 on error (errno set).
 */
extern int subst(strbuf *dst, char const *src, size_t len, subst_t const *ctx) ;

#endif
