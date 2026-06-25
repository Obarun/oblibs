/*
 * environ_untrim_value.c
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
#include <errno.h>

#include <oblibs/strbuf.h>
#include <oblibs/environ.h>

/** Inverse of the envfile file decoder (environ_trim), within the module's
 * invariant that an environment value never contains a literal newline (0x0a).
 * A 0x0a is rejected up front with EINVAL: although environ_trim could decode it
 * back, the NUL-separated <-> newline-joined duality used by environ_untrim /
 * environ_get_value / environ_substitute treats 0x0a as the entry separator, so
 * a newline-bearing value cannot round-trip through the whole module. Refusing it
 * here turns that latent corruption into an explicit caller-handled error. For
 * every value this function ACCEPTS, decode(encode(v)) == v holds on all paths.
 *
 * Two output forms, picked for minimality:
 *
 *   - RAW: the value is appended verbatim when every byte survives the unquoted
 *     value state of environ_trim unchanged. That state keeps '#', '=', internal
 *     spaces and any byte >= 0x20 (incl. UTF-8) literal; it only strips leading
 *     and trailing whitespace. So a value is raw-safe iff it is non-empty, has no
 *     leading/trailing space, and contains no '"', no '\\', and no byte < 0x20 or
 *     0x7f. The empty value is raw-safe too (it yields "KEY=" which decodes back
 *     to ""). This keeps clean values (tty1, :0, /usr/bin, café) unquoted.
 *
 *   - QUOTED: otherwise the whole value is wrapped in double quotes and every
 *     byte that the in-quote state treats specially is escaped with ONE canonical
 *     form: '"' -> \", '\\' -> \\, the control bytes 0x07,0x08,0x09,0x0b,0x0c,
 *     0x0d -> their letter escape (\a \b \t \v \f \r), and every other control
 *     byte (0x01..0x06, 0x0e..0x1f, 0x7f) -> a 3-digit octal escape \OOO.
 *     environ_trim caps octal at exactly 3 digits, so \OOO stays unambiguous even
 *     when followed by an octal digit. 0x0a (\n) never reaches this switch: it is
 *     rejected at the top of the function.
 */

static uint8_t value_needs_quote(char const *value)
{
    if (!*value)
        return 0 ;

    if (*value == ' ')
        return 1 ;

    char const *p = value ;
    for (; *p ; p++) {
        unsigned char c = (unsigned char)*p ;
        if (c == '"' || c == '\\')
            return 1 ;
        if (c < 0x20 || c == 0x7f) // controls (0x0a already rejected) */
            return 1 ;
    }

    if (p[-1] == ' ')
        return 1 ;

    return 0 ;
}

uint8_t environ_untrim_value(strbuf *out, char const *value)
{
    for (char const *p = value ; *p ; p++)
        if (*p == '\n') //unrepresentable: 0x0a is the module's entry separator
            return (errno = EINVAL, 0) ;

    if (!value_needs_quote(value))
        return strbuf_cats(out, value) ;

    if (!strbuf_catb(out, "\"", 1))
        return 0 ;

    for (char const *p = value ; *p ; p++) {
        unsigned char c = (unsigned char)*p ;
        char esc = 0 ;
        switch (c) {
            case '"'  : esc = '"' ;  break ;
            case '\\' : esc = '\\' ; break ;
            case '\a' : esc = 'a' ;  break ;
            case '\b' : esc = 'b' ;  break ;
            case '\t' : esc = 't' ;  break ;
            case '\v' : esc = 'v' ;  break ;
            case '\f' : esc = 'f' ;  break ;
            case '\r' : esc = 'r' ;  break ;
            default   : break ;
        }

        if (esc) {

            char buf[2] = { '\\', esc } ;

            if (!strbuf_catb(out, buf, 2))
                return 0 ;

        } else if (c < 0x20 || c == 0x7f) {

            char buf[4] = { '\\',
                            '0' + ((c >> 6) & 7),
                            '0' + ((c >> 3) & 7),
                            '0' + (c & 7) } ;

            if (!strbuf_catb(out, buf, 4))
                return 0 ;

        } else {

            char b = (char)c ;
            if (!strbuf_catb(out, &b, 1))
                return 0 ;
        }
    }

    return strbuf_catb(out, "\"", 1) ;
}
