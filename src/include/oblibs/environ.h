/*
 * environ.h
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 *
 * @brief Environment file parsing, key=value manipulation and environ merging.
 *
 * This module parses `envfile`-style text into a NUL-separated list of
 * `key=value` entries stored in a `strbuf` (the "sbl" layout: each line is a
 * C string terminated by a NUL, walked with `FOREACH_SBL`), and provides the
 * primitives to extract keys and values, substitute `${...}` references, merge
 * several sources together, and finally turn the result into a `char **`
 * `environ` array suitable for `execve`.
 *
 * The text parser (`environ_trim`) implements the execline `envfile`
 * grammar (https://skarnet.org/software/execline/envfile.html), including
 * comment stripping, whitespace trimming, quoting and `\` / octal / hex escape
 * decoding. Most functions exchange data through a `strbuf` holding the
 * NUL-separated `key=value` list; lengths are bounded by the `MAXENV` and
 * `MAXFILE` limits below.
 *
 * Most functions follow the project's `uint8_t` boolean convention: `1` on
 * success, `0` on failure (errno set where noted). A few functions deviate and
 * are documented individually: `environ_trim` and `environ_make` return `int`,
 * `environ_length` returns a `size_t` count, `environ_merge` returns a
 * `size_t` element count (`0` meaning failure/overflow), and
 * `environ_clean_unexport` returns `uint8_t`.
 *
 * @note Out-of-memory while growing a `strbuf` does not produce an error
 *       return in the text parser: `environ_trim` calls `log_die_nomem`, which
 *       terminates the process. The other functions propagate strbuf failures
 *       as a `0` return.
 */

#ifndef OB_ENVIRON_H
#define OB_ENVIRON_H

#include <stdint.h>
#include <sys/types.h>

#include <oblibs/strbuf.h>
#include <oblibs/lexer.h>
#include <oblibs/subst.h>

/**
 * @brief Maximum number of files parsed from a single directory.
 *
 * Enforced by `environ_merge_dir`: when the number of regular files found in
 * the directory exceeds `MAXFILE`, it fails with errno `E2BIG`.
 */
#define MAXFILE 20

/**
 * @brief Maximum byte size accepted for a single environment source.
 *
 * Enforced as a strict upper bound (`>= MAXENV` is rejected) by
 * `environ_parse_file` (on the file size) and by `environ_merge_string` (on
 * the input string length). Exceeding it yields errno `ENAMETOOLONG`.
 */
#define MAXENV 8191

/**
 * @brief Marker prefixing a value that must NOT be exported.
 *
 * A `'!'` as the first character of a value flags the entry as "unexport".
 * `environ_substitute` records such keys in `info->modifs` and strips the
 * marker; `environ_clean_unexport` removes the marker in place.
 */
#define VAR_UNEXPORT '!'

/**
 * @brief How `environ_substitute` records a variable whose value is empty.
 *
 * The right choice depends on what the caller later hands to `subst`:
 *  - `ENVIRON_EMPTY_DROP`: the entry gets `n` = 0, so every word holding the
 *    reference vanishes. For a list of words (an argv), an empty variable then
 *    removes its argument.
 *  - `ENVIRON_EMPTY_KEEP`: the entry gets `n` = 1 with the value `""`, so the
 *    reference is replaced by nothing and the rest of the word stays. For a
 *    whole text passed as ONE word (a script), where dropping the word would
 *    drop the entire text.
 */
#define ENVIRON_EMPTY_DROP 0
#define ENVIRON_EMPTY_KEEP 1

/** Please POSIX */
extern char **environ ;

/**
 * @brief Parse `envfile`-style text into a NUL-separated key=value list.
 *
 * Runs the execline `envfile` finite-state parser over @file (a C string, NOT
 * a path) and appends the resulting NUL-separated `key=value` entries to @sb.
 * Comments (`#` to end of line), surrounding whitespace, quoting and `\`,
 * octal and hex escapes are decoded per
 * https://skarnet.org/software/execline/envfile.html. On a syntax error the
 * parser stops and the function returns failure.
 *
 * A value that decodes to contain a NUL (`0x00`) or a newline (`0x0a`) — only
 * reachable via a `\0`/`\012`/`\x00` or `\n`/`\012`/`\x0a` escape inside a quoted
 * value — is REJECTED as an error: an environment value must not contain either,
 * as they are the separators the sbl layout and the joined text form reserve
 * (used by `environ_untrim`/`environ_get_value`/`environ_substitute` to walk
 * entries). Rejecting the newline is a deliberate divergence from execline's
 * `envfile`, which permits it.
 *
 * @param[in,out] sb  Destination buffer; parsed entries are appended to its
 *                    current contents (it is not cleared first). On the
 *                    NUL/newline rejection it is rolled back to its length on
 *                    entry.
 * @param[in] file    Text to parse as a C string (envfile content, not a path).
 *
 * @return 1 on success.
 * @return 0 on a syntax error in @file, or on a value containing a NUL (`0x00`)
 *         or a newline (`0x0a`); a warning is logged (errno is NOT set by these
 *         paths), or if the final `strbuf_uncounted` close fails.
 *
 * @note On `strbuf` growth failure during parsing the process is terminated via
 *       `log_die_nomem`; this is not reported as a `0` return.
 */
extern int environ_trim(strbuf *sb, char const *file) ;

/**
 * @brief Read a file from disk and parse it with `environ_trim`.
 *
 * Stats @file, validates its size against `MAXENV`, reads it into a temporary
 * buffer, then appends its parsed `key=value` entries to @sb via
 * `environ_trim`.
 *
 * @param[in,out] sb  Destination buffer; parsed entries are appended.
 * @param[in] file    Path to the file to read.
 *
 * @return 1 on success.
 * @return 0 on failure:
 *      - errno `EINVAL` if @file is an empty (zero-byte) file
 *      - errno `ENAMETOOLONG` if the file size is `>= MAXENV` (a warning is
 *        logged)
 *      - the `stat` errno if @file cannot be stat'd (file_get_size returned
 *        -1; errno is forwarded unchanged, e.g. `ENOENT`, `EACCES`)
 *      - `strbuf_read_file` failure (errno from the underlying read), or a
 *        syntax error reported by `environ_trim` (no errno set on that path)
 */
extern uint8_t environ_parse_file(strbuf *sb, const char *file) ;

/**
 * @brief Expand `${...}` references across an environment in place.
 *
 * For each `key=value` entry in @modifs, expands `${VAR}` references found in
 * the value, recording the resolved variables and values into the substitution
 * context @info (`info->vars`, `info->values`, `info->modifs`, `info->data`).
 * A `${VAR}` is resolved first from the current process environment (`getenv`),
 * then from the other entries of @modifs. A variable referencing itself
 * directly is skipped with a warning; an indirect cycle (A=${B}, B=${A}) is
 * detected and broken with a warning. Values starting with `VAR_UNEXPORT`
 * (`'!'`) are tracked: their key is recorded in `info->modifs` and the marker
 * is stripped before storing the value. On success @modifs is rewritten with
 * the expanded, NUL-separated entries.
 *
 * @par Re-entrance: the last entry wins.
 * The function is RE-ENTRANT on the same @info: several sources may be fed to
 * it in a row (inherited environment, service section, imported files, values
 * published at runtime), the CALL ORDER carrying the precedence. For a name
 * registered by more than one source, the LATEST call wins, on all three
 * effects. This is the module's policy throughout — see `environ_merge_environ`,
 * `environ_merge_dir` and `environ_create_environ`. Three invariants hold
 * without exception:
 *  - `info->vars`: a name appears AT MOST ONCE. A name already recorded reuses
 *    its offset instead of being appended again.
 *  - `info->data`: AT MOST ONE entry per name. A name already carrying an entry
 *    has it REWRITTEN (new `value` offset, `n` recomputed) rather than getting a
 *    second one. `n` is 1 for a non-empty resolved value. For an empty one it
 *    follows @empty: 0 under `ENVIRON_EMPTY_DROP` (nothing is then appended to
 *    `info->values`), 1 with the value `""` under `ENVIRON_EMPTY_KEEP`.
 *  - `info->modifs`: AT MOST ONE occurrence per key, same policy applied
 *    symmetrically. A value carrying `VAR_UNEXPORT` adds the key if absent; a
 *    value WITHOUT the marker REMOVES it if present — a real removal from the
 *    NUL-separated list, since a caller reads those records as bare `KEY`
 *    entries that UNSET the variable, so a key re-exported by a later source
 *    must leave the list.
 *
 * The resolved value is always APPENDED at the end of `info->values`, so the
 * offsets already handed out stay valid; rewriting an entry therefore leaves its
 * former value unreferenced in the blob. Those few dead bytes are deliberate:
 * the context is short-lived and reindexing would cost far more.
 *
 * @warning The idempotence is on the TABLE, not on the CONTENT: each source must
 * be passed EXACTLY ONCE. Re-passing an already expanded @modifs would expand it
 * a second time — a reference left literal on the first pass because the variable
 * did not exist yet would get resolved on the second. A caller adding a source to
 * an accumulated environment must pass that source ALONE, then merge it into its
 * accumulator; it must not re-submit the whole accumulator.
 *
 * @note Within a SINGLE @modifs, a key present more than once yields one entry
 * (the invariant above), but its value is the one `environ_sbl_value` resolves,
 * i.e. that of the FIRST occurrence in @modifs — the value is looked up by key
 * over the whole source, not taken from the entry being walked.
 *
 * @pre No value may contain a newline (`0x0a`). `environ_get_value` copies up to
 * the first `'\n'`, so such a value would be silently TRUNCATED in the table.
 * `environ_parse_file` already rejects them, so the normal pipeline is safe; a
 * raw import (an actual `char **environ`) can introduce one and it is the
 * CALLER's job to filter it out. No guard is enforced here.
 *
 * @param[in,out] modifs  Environment to expand, as a NUL-separated key=value
 *                        list; rewritten in place on success. An empty @modifs
 *                        is a no-op success.
 * @param[in,out] info    Substitution context that accumulates the resolved
 *                        variables, values, unexport markers and per-entry
 *                        metadata. On any failure, `info->vars` and
 *                        `info->values` are rolled back to the lengths they had
 *                        when the failing entry was entered; `info->modifs` and
 *                        `info->data` are NOT rolled back.
 * @param[in] empty       `ENVIRON_EMPTY_DROP` or `ENVIRON_EMPTY_KEEP`: how an
 *                        empty value is recorded, see those constants.
 *
 * @return 1 on success (including the empty-@modifs case).
 * @return 0 on failure:
 *      - errno `EINVAL` when @empty is neither `ENVIRON_EMPTY_DROP` nor
 *        `ENVIRON_EMPTY_KEEP`; checked first, @modifs and @info are untouched
 *      - errno `EOVERFLOW` when a `strbuf`/`sbl` copy or append cannot grow
 *      - errno as left by a failing sub-call (`environ_get_key`,
 *        `environ_get_value`, `environ_search_value`, `dollar_replace`,
 *        `genbuf_append`); see those for the exact value
 *        (e.g. `EINVAL` from `environ_get_value` when an entry has no `=`)
 */
extern uint8_t environ_substitute(strbuf *modifs, subst_t *info, uint8_t empty) ;

/**
 * @brief Extract the key (everything before the first `=`) of an entry.
 *
 * Lexes @str up to the first `=`, trimming surrounding `" \t\r\n"`, and
 * replaces the contents of @sb with the resulting key.
 *
 * @param[out] sb  Destination buffer; cleared and filled with the key.
 * @param[in] str  A `key=value` entry as a C string.
 *
 * @return 1 on success.
 * @return 0 on failure (errno as left by `lexer_trim`; typically a malformed
 *         entry or an allocation/overflow inside the lexer).
 */
extern uint8_t environ_get_key(strbuf *sb, const char *str) ;

/**
 * @brief Find the entry whose key equals @key within @str and confirm a match.
 *
 * Prepends a `\n` to @str and lexes it line by line, extracting each key
 * (trimming `" \t\r\n"`) and comparing it to @key. On the first match the
 * @cfg cursor (`cfg->opos`) is left positioned at the matching entry so a
 * caller can resume from it, and @sb holds the matched key.
 *
 * @param[out] sb     Destination buffer; on a match it holds the matched key.
 * @param[in] str     The environment text to search (C string). Must be
 *                    non-empty.
 * @param[in] key     The key to look for. Must be non-empty.
 * @param[in,out] cfg Lexer cursor; configured and advanced by this function,
 *                    and on success points at the matched entry.
 *
 * @return 1 if an entry with key @key is found.
 * @return 0 on failure:
 *      - errno `EINVAL` if @str or @key is empty, if the lexer fails on an
 *        entry, or if no entry matches @key (the no-match exhaustion path)
 *      - errno `EOVERFLOW` if the working buffer cannot be grown
 */
extern uint8_t environ_search_key(strbuf *sb, const char *str, const char *key, lexer_config *cfg) ;

/**
 * @brief Extract the value (everything after the first `=`) of an entry, verbatim.
 *
 * Copies everything after the first `=` up to `\n` or end of string into @sb,
 * with no parsing. @str MUST already be a parsed `key=value` entry (as produced
 * by `environ_trim`); this function deliberately does NOT unescape, so passing
 * raw envfile syntax leaves the escapes literal.
 *
 * @param[out] sb  Destination buffer; cleared and filled with the value (which
 *                may be empty if the entry is `key=`).
 * @param[in] str  An already-parsed `key=value` entry as a C string.
 *
 * @return 1 on success.
 * @return 0 on failure:
 *      - errno `EINVAL` if @str contains no `=`
 *      - `strbuf_reserve` failure (errno as left by the reserve) if the
 *        destination cannot be grown
 */
extern uint8_t environ_get_value(strbuf *sb, const char *str) ;

/**
 * @brief Encode a raw value into envfile syntax, appended to @out.
 *
 * Inverse of the file decoder `environ_trim` at the value granularity: for any
 * value this function ACCEPTS, parsing the appended bytes back reproduces @value
 * byte-for-byte (proven for every byte `0x01..0xff` except `0x0a`). A "clean"
 * value (non-empty, no leading/trailing space, and free of `"`, `\`, bytes
 * `< 0x20` and `0x7f`) is appended verbatim so common values stay unquoted and
 * readable; otherwise it is wrapped in double quotes with a single canonical
 * escape per byte (`\"`, `\\`, the letter escapes `\a \b \t \v \f \r` for
 * `0x07,0x08,0x09,0x0b,0x0c,0x0d`, and 3-digit octal `\OOO` for any other control
 * byte). `#`, `=` and high (UTF-8) bytes are NOT escaped: the parser keeps them
 * literal in a value. The empty value appends nothing (decodes back to "").
 *
 * A value containing a literal newline (`0x0a`) is REJECTED with errno `EINVAL`
 * and nothing is appended: an environment value must not contain a newline. The
 * module's NUL-separated/newline-joined value duality (`environ_get_value`,
 * `environ_untrim`, `environ_substitute`) uses `0x0a` as the entry separator, so
 * a newline-bearing value could not round-trip through the whole module; refusing
 * it at the encode boundary makes that an explicit, caller-handled error instead
 * of silent downstream corruption. The guarantee is therefore total: every value
 * this function accepts round-trips on every decode path of the module.
 *
 * @param[in,out] out   Destination buffer; the encoded value is appended to its
 *                      current contents (it is not cleared first). Left unchanged
 *                      on the `EINVAL` rejection.
 * @param[in] value     Raw value to encode (a C string; no embedded NUL).
 *
 * @return 1 on success.
 * @return 0 on failure:
 *      - errno `EINVAL` if @value contains a literal newline (`0x0a`)
 *      - a `strbuf` append failure (errno as left by the strbuf call, typically
 *        `ERANGE`)
 */
extern uint8_t environ_untrim_value(strbuf *out, char const *value) ;

/**
 * @brief Append a full `key=value` entry, value-encoded and newline-terminated.
 *
 * Appends `@key`, `=`, the `environ_untrim_value` encoding of @value, then `\n`
 * to @out. @key is appended verbatim and must be a valid envfile key (no `=`, no
 * `\n`, no surrounding whitespace, not starting with `#`); the function does not
 * validate it. @value is subject to `environ_untrim_value`'s newline rejection.
 *
 * @param[in,out] out   Destination buffer; the entry is appended. May be left
 *                      partially appended (key and `=`) on a @value rejection.
 * @param[in] key       Entry key, appended verbatim.
 * @param[in] value     Raw value, encoded via `environ_untrim_value`.
 *
 * @return 1 on success.
 * @return 0 on failure:
 *      - errno `EINVAL` if @value contains a literal newline (`0x0a`), forwarded
 *        from `environ_untrim_value`
 *      - a `strbuf` append failure (errno as left by the strbuf call, typically
 *        `ERANGE`)
 */
extern uint8_t environ_untrim_kv(strbuf *out, char const *key, char const *value) ;

/**
 * @brief Serialize a NUL-separated key=value list into envfile text. Inverse of
 *        `environ_trim`.
 *
 * Walks the `sbl` entries of @sbl (as produced by `environ_trim`) and appends,
 * for each, a `key=<encoded value>\n` line to @out (via `environ_untrim_kv`).
 * Each value is re-encoded with `environ_untrim_value`, so parsing the result
 * back with `environ_trim` reproduces @sbl byte-for-byte. The key is appended
 * verbatim. An empty @sbl appends nothing.
 *
 * @param[in,out] out  Destination buffer; the envfile text is appended to its
 *                     current contents (it is not cleared first).
 * @param[in] sbl      Source entries as a NUL-separated key=value list.
 *
 * @return 1 on success (including the empty case).
 * @return 0 on failure:
 *      - errno `EINVAL` if an entry has no `=`, or forwarded from
 *        `environ_untrim_value` if a value contains a newline (`0x0a`)
 *      - a `strbuf` append failure (errno as left by the strbuf call)
 */
extern int environ_untrim(strbuf *out, strbuf const *sbl) ;

/**
 * @brief Look up the value associated with @key inside raw environment text.
 *
 * Locates the entry whose key is @key (via `environ_search_key`), parses that
 * single entry with `environ_trim` (so @str may be raw, unparsed envfile text),
 * then extracts its value into @sb. Used by `environ_substitute` to resolve
 * `${...}` references.
 *
 * @param[out] sb  Destination buffer; cleared and filled with the value.
 * @param[in] str  Environment text (raw or parsed) as a C string.
 * @param[in] key  The key whose value is wanted.
 *
 * @return 1 on success.
 * @return 0 on failure:
 *      - errno `EINVAL` if @key is not found (forwarded from
 *        `environ_search_key`), or if @str/@key is empty
 *      - failure forwarded from `environ_trim` (no errno set on its syntax
 *        path) or from `environ_get_value` (errno `EINVAL` if the matched
 *        entry has no `=`)
 */
extern uint8_t environ_search_value(strbuf *sb, const char *str, const char *key) ;

/**
 * @brief Merge the key=value entries of a file into @modifs.
 *
 * Parses @file (via `environ_parse_file`) and merges its entries into @modifs.
 * If @modifs is empty it is filled verbatim; otherwise entries are merged via
 * `environ_merge_environ` so that, for a key present in both, the entry coming
 * from @file wins.
 *
 * @param[in,out] modifs  Accumulator environment; merged in place.
 * @param[in] file        Path to the file to merge in.
 *
 * @return 1 on success.
 * @return 0 on failure:
 *      - errno `EINVAL` if @file is empty (zero bytes)
 *      - errno `EOVERFLOW` if copying the parsed content into @modifs overflows
 *      - failure forwarded from `environ_parse_file` (e.g. `ENAMETOOLONG`,
 *        `EINVAL`, or the `stat`/read errno) or from `environ_merge_environ`
 */
extern uint8_t environ_merge_file(strbuf *modifs, const char *file) ;

/**
 * @brief Merge the key=value entries parsed from a string into @modifs.
 *
 * Parses @str (via `environ_trim`) and merges its entries into @modifs. If
 * @modifs is empty it is filled verbatim; otherwise entries are merged via
 * `environ_merge_environ` so that, for a key present in both, the entry coming
 * from @str wins.
 *
 * @param[in,out] modifs  Accumulator environment; merged in place.
 * @param[in] str         Envfile text to parse and merge (C string).
 *
 * @return 1 on success.
 * @return 0 on failure:
 *      - errno `EINVAL` if @str is NULL
 *      - errno `ENAMETOOLONG` if `strlen(@str) >= MAXENV` (a warning is logged)
 *      - errno `EOVERFLOW` if copying the parsed content into @modifs overflows
 *      - failure forwarded from `environ_trim` (no errno set on its syntax
 *        path) or from `environ_merge_environ`
 */
extern uint8_t environ_merge_string(strbuf *modifs, const char *str) ;

/**
 * @brief Append a `char **environ`-style array of entries to @modifs.
 *
 * Appends the first @len strings of @environ to @modifs, each NUL-terminated,
 * producing the NUL-separated `sbl` layout. No parsing, deduplication or
 * trimming is performed.
 *
 * @param[in,out] modifs  Accumulator environment; entries are appended.
 * @param[in] environ     Array of `key=value` C strings. Must be non-NULL and
 *                        its first element non-NULL.
 * @param[in] len         Number of leading elements of @environ to import.
 *
 * @return 1 on success.
 * @return 0 on failure:
 *      - errno `EINVAL` if @environ is NULL or `@environ[0]` is NULL
 *      - a `strbuf` append/terminate failure (errno as left by the strbuf call)
 */
extern uint8_t environ_import_arguments(strbuf *modifs, char const *const *environ, size_t len) ;

/**
 * @brief Strip the `VAR_UNEXPORT` (`'!'`) marker from the start of each value.
 *
 * For every `key=value` entry in @modifs whose value begins with `'!'`, removes
 * that leading character; other entries are copied unchanged. @modifs is
 * rewritten in place. An empty @modifs is a no-op success.
 *
 * @param[in,out] modifs  Environment to clean in place.
 *
 * @return 1 on success (including the empty case).
 * @return 0 on failure (errno is always set):
 *      - errno `EINVAL` if an entry contains no `=` (`get_len_until` returns -1)
 *      - errno `EOVERFLOW` on a `strbuf` append/terminate failure while building
 *        the cleaned copy, or on the final copy back into @modifs
 */
extern uint8_t environ_clean_unexport(strbuf *modifs) ;

/**
 * @brief Merge every environment file found in a directory into @modifs.
 *
 * @path MUST be a directory, not a file. Collects the regular files in @path,
 * sorts them by name, and merges each (via `environ_merge_file`) into @modifs
 * in sorted order. Each file is parsed, cleaned and stripped of empty and
 * comment lines by the underlying parser; for a key present in several files,
 * the occurrence from the last file in sort order wins.
 *
 * @param[in,out] modifs  Accumulator environment; merged in place.
 * @param[in] path        Path to a directory.
 *
 * @return 1 on success, including when @path is empty (no regular files) — in
 *         that case @modifs is left untouched.
 * @return 0 on failure:
 *      - errno `E2BIG` if the directory holds more than `MAXFILE` files (a
 *        warning is logged)
 *      - failure forwarded from `sbl_dir_get`, `sbl_sort`, the path build, the
 *        final `strbuf_uncounted`, or `environ_merge_file` (which can leave
 *        `EINVAL`, `ENAMETOOLONG`, `EOVERFLOW` or a `stat`/read errno)
 *
 * @note This function does NOT return 0 when @path or the files are empty:
 *       check `modifs->len` after the call to detect an empty result.
 */
extern uint8_t environ_merge_dir(strbuf *modifs, char const *path) ;

/**
 * @brief Merge a directory of environment files, then strip unexport markers.
 *
 * Calls `environ_merge_dir` then `environ_clean_unexport` on @modifs.
 *
 * @param[in,out] modifs  Accumulator environment; merged and cleaned in place.
 * @param[in] path        Path to a directory.
 *
 * @return 1 on success.
 * @return 0 on failure, forwarded from `environ_merge_dir` (errno `E2BIG` or
 *         the sub-call errno) or from `environ_clean_unexport` (errno `EINVAL`
 *         or `EOVERFLOW`).
 */
extern uint8_t environ_merge_dir_g(strbuf *modifs, const char *path) ;

/**
 * @brief Merge a second environment @new into @modifs, @new taking precedence.
 *
 * Merges the `key=value` entries of @new into @modifs: for a key present in
 * both, the entry from @new replaces the one in @modifs. Entries of @modifs
 * whose key is absent from @new are kept. @modifs is rewritten in place. If
 * @modifs is empty the function returns success without touching it (the @new
 * entries are NOT copied in — the caller is expected to seed @modifs first; see
 * the source report).
 *
 * @param[in,out] modifs  Accumulator environment; rewritten in place.
 * @param[in] new         Environment whose entries take precedence; read only
 *                        (its keys are looked up directly on its sbl).
 *
 * @return 1 on success (including the empty-@modifs case).
 * @return 0 on failure:
 *      - errno `EOVERFLOW` on a `strbuf`/`sbl` copy or append that cannot grow
 *      - failure forwarded from `environ_get_key`
 */
extern uint8_t environ_merge_environ(strbuf *modifs, strbuf *new) ;

/**
 * @brief Build a new `environ` pointer array from @env merged with @modifs.
 *
 * Computes a worst-case element count, then calls `environ_merge` to produce
 * @nenvp: the entries of @env overlaid with the `key=value` entries of @modifs
 * (for a shared key, the entry from @modifs wins). The resulting array is
 * NULL-terminated.
 *
 * @param[out] nenvp   Caller-provided array of pointers to receive the result,
 *                     including the terminating NULL. Must be large enough for
 *                     `environ_length(env) + 1 + sbl_count(modifs)` elements.
 * @param[in] env      Source environment array (NULL-terminated).
 * @param[in] modifs   Overlay entries as a NUL-separated key=value list.
 *
 * @return 1 on success.
 * @return 0 if `environ_merge` reports overflow (the computed bound was
 *         exceeded). errno is NOT set by this function.
 */
extern uint8_t environ_create_environ(char const **nenvp, char const *const *env, strbuf *modifs) ;

/**
 * @brief Count the entries of a NULL-terminated `environ` array.
 *
 * @param[in] e  NULL-terminated array of C strings. Must not be NULL.
 *
 * @return The number of elements before the terminating NULL. Always succeeds
 *         (no error path, no errno).
 */
extern size_t environ_length (char const *const *e) ;

/**
 * @brief Build a pointer array @v over the @argc strings packed in @s.
 *
 * Treats @s as @argc consecutive NUL-terminated strings occupying @len bytes,
 * and fills @v with a pointer to each, then a terminating NULL. The pointers
 * reference into @s; no copy is made.
 *
 * @param[out] v     Array receiving @argc pointers plus a terminating NULL.
 *                   Must hold at least `@argc + 1` elements.
 * @param[in] argc   Number of strings packed in @s.
 * @param[in] s      Buffer of @argc NUL-terminated strings, end to end.
 * @param[in] len    Number of bytes available in @s.
 *
 * @return 1 on success.
 * @return 0 with errno `EINVAL` if the strings do not fit: a string's length
 *         (including its NUL) exceeds the remaining @len, i.e. @s is shorter
 *         than @argc strings imply.
 */
extern int environ_make (char const **v, size_t argc, char const *s, size_t len) ;

/**
 * @brief Apply modification entries to an environment, producing @v.
 *
 * Takes the @envlen variables of @envp and applies the @modiflen bytes of
 * NUL-separated `key=value` (or bare `key`) modifications in @modifs, storing
 * the result in @v with a terminating NULL. For each modification: an existing
 * variable with the same key is removed; if the modification carries a value
 * (`key=value`) it is then added, while a bare `key` (no `=value`) only removes
 * the variable (an "unset"). The pointers in @v reference @envp and @modifs;
 * no copy is made.
 *
 * @param[out] v        Array receiving the resulting pointers plus a
 *                      terminating NULL.
 * @param[in] vmax      Capacity of @v in elements (including the NULL slot).
 * @param[in] envp      Source environment (array of `key=value` C strings).
 * @param[in] envlen    Number of entries in @envp.
 * @param[in] modifs    NUL-separated modification entries.
 * @param[in] modiflen  Number of bytes in @modifs.
 *
 * @return The number of elements now in @v, INCLUDING the terminating NULL
 *         (i.e. the count written, terminator counted).
 * @return 0 if the result would not fit: at any point the required element
 *         count reaches @vmax (initial @envlen already `>= vmax`, or an
 *         addition, or the terminating NULL would overflow). errno is NOT set.
 */
extern size_t environ_merge (char const **v, size_t vmax, char const *const *envp, size_t envlen, char const *modifs, size_t modiflen) ;

/**
 * @brief Append a live `char **environ` to @modifs, skipping what the module
 *        cannot represent.
 *
 * Walks the NULL-terminated array @env and appends each entry to @modifs,
 * NUL-terminated, producing the `sbl` layout. The same append as
 * `environ_import_arguments`, plus the validation a REAL environment needs: such
 * an array has been through no parser of this module.
 *
 * Two kinds of entry are skipped, each with a warning:
 *  - an entry carrying no KEY, i.e. holding no `=` at all (`novalue`) or opening
 *    on one (`=value`, `=`): `environ_get_key` rejects both, and either would
 *    make `environ_substitute` fail on the whole accumulator. `execve` validates
 *    nothing, so a real environment can carry them. An entry with an empty VALUE
 *    (`KEY=`) is legitimate and IS kept;
 *  - an entry whose value holds a newline (`0x0a`): `environ_get_value` copies up
 *    to the first one, so the value would reach the consumers TRUNCATED.
 *    `environ_parse_file` refuses those on its own path; this is the same rule,
 *    applied at the raw-import boundary.
 *
 * Skipping is not losing: a caller that later overlays @modifs onto the original
 * array (`environ_create_environ`) still delivers the skipped entry verbatim. It
 * is only absent from what @modifs feeds -- typically the substitution table of
 * `environ_substitute`.
 *
 * No parsing, no expansion, no deduplication: entries are appended verbatim, in
 * order. Seed @modifs with this before merging the other sources, so that they
 * override it -- `environ_merge_environ` and friends make the later source win.
 *
 * @param[in,out] modifs  Accumulator environment; entries are appended to its
 *                        current contents (it is not cleared first).
 * @param[in] env         NULL-terminated array of `key=value` C strings. An EMPTY
 *                        array (`env[0] == NULL`) is a success appending nothing:
 *                        a program may legitimately be started with no
 *                        environment at all. This is a deliberate divergence from
 *                        `environ_import_arguments`, which rejects that case.
 *
 * @return 1 on success, including when every entry was skipped.
 * @return 0 on failure:
 *      - errno `EINVAL` if @env is NULL
 *      - a `strbuf` copy/append failure (errno as left by the strbuf call,
 *        typically `ERANGE`); @modifs then keeps the entries appended so far
 */
extern uint8_t environ_import_environ(strbuf *modifs, char const *const *env) ;

#endif
