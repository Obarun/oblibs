/*
 * environ.h
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef OB_ENVIRON_H
#define OB_ENVIRON_H

#include <sys/types.h>

#include <oblibs/stack.h>
#include <oblibs/lexer.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

#include <execline/execline.h>

#define MAXVAR  50
#define MAXFILE 20
#define MAXENV 8191
#define VAR_UNEXPORT '!'

typedef struct exlsn_s exlsn_t, *exlsn_t_ref ;
struct exlsn_s
{
  stralloc vars ;
  stralloc values ;
  genalloc data ; // array of elsubst
  stralloc modifs ;
} ;

#define EXLSN_ZERO { \
    .vars = STRALLOC_ZERO, .values = STRALLOC_ZERO, \
    .data = GENALLOC_ZERO, .modifs = STRALLOC_ZERO }

extern void exlsn_free (exlsn_t *) ;

/** Please POSIX */
extern char **environ ;

/** Main parser
 * Store the result to @stk format.
 * Set errno.
 * Use https://skarnet.org/software/execline/envfile.html algorithm
 * @Return 0 on fail else 1
 */
extern int environ_trim(stack *stk, char const *file) ;

/** Convenient API reading file and checking validity of
 * the @stk size, then call environ_trim
 * Set errno.
 * @Return 0 on fail else 1
 * */
extern uint8_t environ_parse_file(stack *stk, const char *file) ;

/** Substitute value of @key with @val into @envp
 * @Return 1 on success
 * @Return 0 on fail*/
extern uint8_t environ_substitute(stralloc *modifs, exlsn_t *info) ;

/** Extract the key (meaning everything before '=') from @str,
 * removing " \t\r\n" characters.
 * Replace the content of @stk with the result of the process.
 * @Return 1 on success
 * @Return 0 on fail and set errno*/
extern uint8_t environ_get_key(stack *stk, const char *str) ;

/** Extract @key (meaning verything before '=') from @str,
 * removing " \t\r\n" characters.
 * Replace the content of @stk with the
 * result of the process.
 * @Return 1 on success
 * @Return 0 on fail and set errno*/
extern uint8_t environ_search_key(stack *stk, const char *str, const char *key, lexer_config *cfg) ;

/** Get the value (meaning everything after '=' and '\n' or EOF)
 * from @str. Replace the content of @stk with the result of
 * the process.
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern uint8_t environ_get_value(stack *stk, const char *str) ;

/** Get the value (meaning everything after '=' and '\n' or EOF)
 * of @key from @str. Replace the content of @stk with the
 * result of the process.
 * Set errno.
 * @Return 1 on success
 * @Return 0 on fail and set errno */
extern uint8_t environ_search_value(stack *stk, const char *str, const char *key) ;

/** Rebuild a string of sastr format
 * appending '\n' at the end of each line.
 * Replace the content of @sa with the
 * result of the process.
 * Set errno.
 * @Return 1 on success
 * @Return 0 on fail. */
extern uint8_t environ_rebuild(stralloc *sa) ;

/** Append @modifs with key=value pair from @file.
 * For a same key=value pair in @modifs and @file,
 * the one from the @file is kept.
 * Set errno
 * @Return 0 on fail else 1
 */
extern uint8_t environ_merge_file(stralloc *modifs, const char *file) ;

/** Append @modifs with key=value pair from @str.
 * For a same key=value pair in @modifs and @str,
 * the one from the @str is kept.
 * Set errno
 * @Return 0 on fail else 1
 */
extern uint8_t environ_merge_string(stralloc *modifs, const char *str) ;

/** Append @modifs with key=value pair from
 * a **@environ format.
 * Set errno.
 * @Return 0 on fail else 1
*/
extern uint8_t environ_import_arguments(stralloc *modifs, char const *const *environ, size_t len) ;

/** Remove '!' character at the begin of the value
 * Set errno.
 * */
extern uint8_t environ_clean_unexport(stralloc *modifs) ;

/** @PATH MUST POINT TO A DIRECTORY NOT A FILE.
 * For each file found at @path:
 * - it sort the list of the file found at @path.
 * - it clean each key=value pair.
 * - it remove any empty and commented line.
 * - it check if MAXFILE and MAXENV is respected.
 * - it drop every same key=value pair from the list found.
 * For a same key=value found on multiple file,
 * the last occurence is kept.
 * Set errno.
 * @Return 1 on success
 * @Return 0 on fail
 * WARNING: this function do not return 0 if @path is empty
 * or files are empty. Simply check the len of @modifs after the call
 * of this function.*/
extern uint8_t environ_merge_dir(stralloc *modifs, char const *path) ;

/** same as environ_merge_dir but also call environ_clean_unexport
 * Set errno.
*/
extern uint8_t environ_merge_dir_g(stralloc *modifs, const char *path) ;

/** Merge @new to @modifs. For a same key=value pair, @new
 * take precedence.
 * Set errno.
 * @Return 0 on fail else 1
 */
extern uint8_t environ_merge_environ(stralloc *modifs, stralloc *new) ;

/** Create an new @nenvp environ array merging
 * first @modifs to @env.For a same key=value pair, @modifs
 * take precedence.
 * @Return 0 on fail else 1
 */
extern uint8_t environ_create_environ(char const **nenvp, char const *const *env, stralloc *modifs) ;

/** Get the length of @e
 * @Return the length of @e
*/
extern size_t environ_length (char const *const *e) ;

/** Make @v with the string @s
 * @Return 1 on success else 0 and set errno
*/
extern int environ_make (char const **v, size_t argc, char const *s, size_t len) ;

/* Takes the environment with envlen variables contained in envp,
 * applies the modifications encoded in the modif modifs of length modiflen, and stores the result in v with a terminating NULL
 * @Return the number of elements now in v, or 0 if the resulting v would be larger than vmax
 */
extern size_t environ_merge (char const **v, size_t vmax, char const *const *envp, size_t envlen, char const *modifs, size_t modiflen) ;

#endif
