/*
 * environ-internal.h
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

#ifndef OB_ENVIRON_INTERNAL_H
#define OB_ENVIRON_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

#include <oblibs/strbuf.h>

/**
 * @brief Internal: look up a key's value directly in an already-decoded sbl.
 *
 * Walks the NUL-separated `key=value` entries in `[@s, @s + @len)` (as produced
 * by `environ_trim`) and, on the first entry whose key (the bytes before the
 * first `=`) equals @key exactly, copies that entry's value verbatim into @out
 * via `environ_get_value`. The value is returned AS STORED — no re-decoding.
 *
 * This is the sbl-native counterpart of `environ_search_value`, for the internal
 * substitution/merge engine which already holds decoded entries: re-running
 * `environ_trim` on them (as `environ_search_value` does, for raw text) would
 * double-decode and corrupt values containing `\` or `"`. `environ_search_value`
 * stays the right tool for RAW text from outside the module.
 *
 * @param[out] out   Destination buffer; on a match it holds the value (which may
 *                   be empty for a `key=` entry).
 * @param[in] s      Start of the NUL-separated entries.
 * @param[in] len    Total byte length of the entries (including the final NUL).
 * @param[in] key    Key to look up. Matched exactly (no prefix match).
 *
 * @return 1 if @key is found (@out filled).
 * @return 0 if @key is not found (errno `EINVAL`), or on an `environ_get_value`
 *         strbuf failure (errno as left by it).
 */
extern uint8_t environ_sbl_value(strbuf *out, char const *s, size_t len, char const *key) ;

#endif
