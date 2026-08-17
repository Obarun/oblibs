# Changelog for oblibs

---

# In 0.4.0.1

Maintenance release. No API change, no behaviour change in the library.

- Build:
    - `tree_copy`: bind the result of the best-effort `lchown`. glibc marks
      `lchown()`, `write()` and `system()` `warn_unused_result` when
      `_FORTIFY_SOURCE` is defined, which distribution builds do and the default
      meson setup does not, so these warnings only surfaced downstream.

- Tests:
    - `inotify`: zero-initialize the watchers handed to calls that fail before
      touching them. `test_inotify_second_watcher_refused` asserted on
      `sse_watcher_active()` over an uninitialized watcher, an indeterminate read
      that is 0 at `-O0` and non-zero once inlining moves the stack frames around
      under `-O2 -flto`, so the test failed in distribution builds while the
      default meson build stayed green.
    - The `write()` calls carrying a test premise become assertions, a short
      write silently invalidating everything asserted after it. The best-effort
      `system()` teardown calls bind their result to a deliberately unused
      variable.

---

# In 0.4.0.0

This is a major release. oblibs is now **self-contained**: the skalibs and
execline dependencies are removed entirely and replaced by native modules.
Consumers must adapt — many interfaces changed name or signature.

- Dependency removal (breaking):
    - Drop the skalibs dependency entirely; drop the skalibs installation scheme.
    - Drop the execline dependency (native `environ` substitution engine).
    - Prune unused includes across the tree; drop obsolete meson options and
      `build_toolchain.sh` after the removals.
    - Remove `get_nbline`, `get_wasted_line`, `scan_isspace`, `scan_timeout`,
      `scan_uid`, `scan_uint32`, `version_cmp`, `version_scan`.
    - Remove the deprecated `graph2.h` compatibility header.

- New event-loop and streaming APIs:
    - `sse`: complete epoll-driven event loop unifying I/O, signals, timers,
      child processes, inotify, eventfd and cron-based scheduling through fd
      readiness. Includes a Quartz Scheduler cron-expression parser with
      next-fire-time computation.
    - `sse_stream` / `stream_message`: SSE-integrated stream I/O with ring
      buffering, fd passing and structured messaging.
    - `sse_streamux`: unix-socket multiplexing helpers on top of `sse_stream`
      (`sse_streamux_create_server`, `sse_streamux_create_client`,
      `sse_streamux_accept`).
    - `rbuffer`, `io_rb`, `io_message`: native ring-buffer primitives and
      structured message parsing over them.

- New native modules (skalibs replacements):
    - `strbuf`: growable buffer replacing `stralloc`.
    - `genbuf`: typed growable array replacing `genalloc`.
    - `sbl`: merged `stack` + `sastr`, backed by `strbuf`.
    - `stream` (`istream` / `ostream`): buffered fd I/O, three-layer read API.
    - `fd`: native descriptor helpers (`close_fd`, `chdir_fd`, `move_fd`,
      `lock_fd`, `remap_fds`, `cloexec_fd`/`uncloexec_fd`, ...).
    - `ocdb`: native constant database (DJB/skalibs cdb port).
    - `opt`: native command-line argument parser (operands, dynamic help
      alignment, hidden options, dispatched sub-command help, epilog, ...).
    - `spawn` / `exec`: posix_spawn-only child API and execve-with-PATH.
    - `attributes`: native compiler-attribute macros.
    - `clock`: native timestamps and timespec helpers (`clock_tai64n_scan`).
    - `hash`: native hash module replacing uthash.
    - `types`: self-contained int/uid/gid/pid conversions, strict scan family,
      `sig_parse` (native `sig0_scan` replacement), endian pack/unpack helpers.
    - `lx_inotify`, `lx_signalfd`, `lx_signal`: inotify/signalfd/signal APIs.

- New functions:
    - `io`: `io_read_result`, `io_write_result`, `io_open_mode`, `io_readv`,
      `io_writev`, `io_allread`/`io_allwrite`, `io_readnclose`/`io_writenclose`.
    - `files`: native `file_copy`, `tree_copy`, `file_tmpname`, `file_tryunlink`;
      path-based whole-file read/write over `io_*nclose`; `file_write_atomic_at`,
      atomic write with a caller-chosen temporary directory, so the target
      directory never shows a partial or supernumerary file to a concurrent
      scanner.
    - `socket`: `socket_private`, `socket_setfd_flags`, `socket_unsetfd_flags`,
      `socketunix_getucred`, `socketunix_bind_reuse`.
    - `queue`: `queue_insert_ptr`, `queue_remove_ptr`, `queue_free_ptr`.
    - `bitset`: add `bitset8_t` and bit-count tracking.
    - `process`: `process_wait` (blocking waitpid with EINTR retry).
    - `log`: formatted `flog_*` macros.
    - `environ`: the envfile value encoder, inverse of `environ_trim` —
      `environ_untrim_value` (one raw value to envfile syntax, verbatim when
      clean, double-quoted with canonical escapes otherwise),
      `environ_untrim_kv` (one `key=value` entry) and `environ_untrim` (a whole
      NUL-separated sbl back to envfile text). Parsing the output reproduces any
      accepted value byte-for-byte; a value containing a newline is rejected
      with EINVAL.
    - `environ_import_environ`: append a live `char **environ` to an accumulator,
      skipping with a warning what the module cannot represent — an entry with no
      key (no `=`, or a leading one), and an entry whose value holds a newline
      (`environ_get_value` would truncate it silently). A raw environment has been
      through no parser of this module, so the validation belongs at that
      boundary; an empty array is a success appending nothing, unlike
      `environ_import_arguments`.
    - `sbl`: `sbl_sort_cmp`, the general form of `sbl_sort` taking a
      caller-supplied comparator.
    - `strbuf`: `strbuf_uncounted`, the C-string terminator idiom (a NUL written
      past the counted length).
    - `string`: `str_search` (native `bytestr` replacement), `auto_strbuf` /
      `auto_strbuf_builder` for strbuf list-append.
    - `directory`: `dir_close` (native `direntry` replacement), native
      `dir_destroy` replacing skalibs `rm_rf`.
    - `spawn`: `spawn_path_full`, taking file actions and a SETSID flag.
    - `istream`: `istream_getall` for resumable fixed-size reads.
    - `attributes`: `attribute_fallthrough`, `attribute_sentinel`,
      `attribute_format`.

- Interface changes:
    - `io`: rename `io_set_flags`/`io_unset_flags` to `io_setfl`/`io_unsetfl`;
      change `io_read`/`io_write` parameter order to match `read`/`write`.
    - `socket`: rename `socket_accept`/`socket_bind` to `socketunix_accept`/
      `socketunix_bind`; define `SOCKETUNIX_PATH_MAX`, rename `SOCK_BACKLOG` to
      `SOCKETUNIX_BACKLOG`.
    - `queue`: accept a single-element queue at `queue_sort`.
    - `bitset`: rename `bitset_t` to `bitset32_t`.
    - `types` (breaking): rename `uid_fmt`/`gid_fmt`/`pid_fmt` and their `scan`
      counterparts to `uid_format`/`uid_parse` (plus `*_format_base` /
      `*_parse_base` for an explicit base); drop the octal/hex/binary
      convenience variants. The old names collided as macros with
      `skalibs/types.h`, breaking any translation unit including both.
    - `opt`: an option placed after an operand is now reported as a usage error
      at the leaf instead of being silently dropped. `opt_scan` stops at the
      first operand (POSIX) and reports a consumed `--` terminator through
      `opt_scan_t.terminated`, so operands forced by `--` are left untouched.
    - `sbl`: `sbl_insert` rejects an offset at or past the list length, and the
      empty list, with EINVAL — appending is `sbl_add`'s job. Rename
      `sbl_replace_g` to `sbl_replace_nline`, which states what it does (split
      per line, replace, rejoin).
    - `lx_inotify`: `lx_inotify_del` takes a path instead of a wd; add
      `lx_inotify_search` (path to wd, ENOENT when unknown) and
      `lx_inotify_forget` to drop a dead watch's bookkeeping.
      `lx_inotify_iterator` no longer skips `IN_IGNORED`, so consumers see a
      watch dying at runtime; those that do not handle watch death must now
      filter `IN_IGNORED` explicitly.
    - `lx_signalfd`: `lx_signalfd_read` returns `ssize_t` and fills a
      caller-provided `signalfd_siginfo` (1 read, 0 nothing pending, -1 fatal),
      instead of conflating signal number and status in an `int`.
    - `sse`: the caller owns EPOLLHUP/EPOLLRDHUP handling and resource freeing;
      `SSE_EPOLL_ZERO` and `SSE_WATCHER_ZERO` initialisers are provided; the
      loop is set running at creation; `sse_poll` takes an explicit timeout
      instead of hardcoding `SSE_TIMEOUT_INFINITE`;
      `sse_streamux_create_server` returns the bind lock fd through a required
      out-parameter so the caller can close it in a non-exec child.
    - `log`: the `flog_*` macros are reentrant. They formatted into a static
      per-translation-unit buffer declared in the header; they now format into
      their own stack buffer via a `log_fmt` helper in `log.c`.
    - `environ_substitute` is re-entrant on the same `subst_t`: several sources
      may be fed to it in a row, the call order carrying the precedence, and the
      last entry for a name wins on all three effects. A name appears at most
      once in `info->vars`, carries at most one entry in `info->data` (rewritten
      in place), and at most one occurrence in the unexport list `info->modifs`,
      where a value without the `!` marker now REMOVES the key instead of leaving
      it — a key re-exported by a later source must stop being unset. Previously a
      repeated name appended an entry whose `var` offset pointed past the names
      blob: the substitution engine read out of bounds, and a name appended later
      bound that stale entry to the wrong variable. Idempotence is on the table,
      not on the content: each source must still be passed exactly once.

- Bug fixes:
    - `environ_substitute`: detect indirect cyclic references.
    - `environ`: reject NUL and newline bytes in parsed values; make
      `environ_get_value` a pure extractor (no re-parse).
    - graph: fix cycle-detection indexing and per-search visited reset; guard the
      zero-length VLA in `check_cycle_recursive`.
    - `io_coe`/`io_uncoe` act on `FD_CLOEXEC` via `F_GETFD`/`F_SETFD`.
    - `parse_cron`: stack-buffer-overflow on an expression of 256 bytes or more,
      copied into a fixed strbuf stack seed through an unbounded raw write.
    - `cron_next`: honour `expr->tz` across the whole computation.
    - `lexer_loop`: the style branch read past the searched string when fewer
      bytes remained than the pattern length; reached through `sbl_replace` from
      `environ_substitute`'s recursive path.
    - `sse_modify_io`: an fd change was silently ignored — the new fd was never
      added to epoll and the old one stayed armed.
    - `sse_dispatch_schedule`: a cron-armed schedule fired once and went silent;
      the one-shot timerfd is now re-armed on normal expiration too. Clock-change
      recovery keys on ECANCELED (a `TFD_TIMER_CANCEL_ON_SET` timer never returns
      EOF), so a schedule no longer stops firing after the wall clock is set.
    - `sse_dispatch_signal`: deliver every distinct pending signal instead of
      draining the signalfd queue and keeping only the last siginfo.
    - `sse_dispatch`: free-safe inside callbacks (self-free, sibling-free);
      `sse_stream` defers the stream free and skips already-closed streams.
    - `sse_start_inotify`: refuse a second watcher with EEXIST. The inotify
      backend is a process-global singleton, so a second watcher could never
      receive events yet was accepted and marked active.
    - `lx_inotify_add`: store the stable path copy as the `hh_path` hash key.
    - `io_rb_encode_cmsg`: stop double-consuming `fdout` on flush, which
      overran the read pointer and emitted phantom `SCM_RIGHTS` descriptors from
      stale memory; add `rbuffer_peek`. Enforce the fd-passing contract with
      explicit guards, stop faulting in ring-buffer storage on init and shrink
      the fd-passing rings to their actual need.
    - `strbuf_insertb`: guard the memmoves against zero-length NULL calls.
    - `socket_set_coe`: expanded to the undeclared `socket_setfd`; it now calls
      `socket_setfd_flags`, so any use of the macro compiles.
    - `sbl`: `sbl_dir_get_element` returns -1 on error, distinguishable from the
      "no match" success; `sbl_replace` rejects a NULL replacement with EINVAL
      instead of dereferencing it; `sbl_sort` sorts pointers rather than copying
      each element into a fixed 4096-byte slot, removing the per-element cap.
    - `tree_copy`: replace an existing destination symlink instead of erroring.
    - Error reporting: `account`, `directory` and `environ_clean_unexport` now
      report failures through errno.

- Documentation:
    - Doxygen contracts on the public headers: for every function, the meaning
      of each return value and the errno set on each failure path.

---

# In 0.3.4.0

- Bug Fixes:
  - Fixed incorrect casting to compare same value types (`f118e41`).
  - Prevented invalid `table[0]` access (`e33bb3b`).
  - Fixed memory management to avoid freeing bad addresses (`d492c0b`).
  - Corrected type casting for reliability (`2ef0780`).
  - Fixed library path for `check` (`993c1b6`).
  - Ensured proper installation before `check` (`4e7263b`).

- Enhancements:
  - Switched to Meson build system for better cross-platform support and efficiency (`2760a05`). Traditional `configure` and `make` remain functional. See `INSTALL_MESON.md`.
  - Made `bitset_t` struct and functions public, split from graph algorithm (`67b6bc6`).
  - Removed outdated graph algorithm; renamed `graph2.h`/`graph2.c` to `graph.h`/`graph.c` (`b5f71c8`). A `graph2.h` is kept only for compatibility and will be removed in a future release.
  - Minor code formatting improvements (`a4ec3cc`).

---

# In 0.3.3.0

- Bugs Fix:

    - queue_remove: Properly cast variable type.
    - graph_free: fix leak by freeing bitset_t structure.
    - stack_copy_stack macro: be sure to use the correct length of the string to copy

- API changes:

    - graph structure: change name of the graph structure to graph_t structure.
    - graph_add_edge: remove useless boolean parameter `requiredby`.
    - ob_basename: allow root directory and set errno at crash.

- Remove function sastr_find_element_byname function

---

## In 0.3.2.1

- Hot Fix:

    - environ_make: close the array at the end of array.

---

## In 0.3.2.0

- Bugs Fix:

    - environ_substitute: ensure that all dollars from a same line are handled.

- New functions:
    - environ_create_environ: create an environment array merging first a modified environment to the current one.
    - environ_length, environ_make, environ_merge: a static copy of env_len and env_make, env_merge respectively from skalibs.
    - io_xxx family: for handling flags for an fd.

- API changes:
    - graph_matrix_build: handle missing flag to avoid crashing on none existing vertex. This is a temporary workaround for 66. The actual graph functions will be drop in a near future.

---

## In 0.3.1.1

- Bugs fix:
    - environ_substitute: allow recursive replacement of a variable.
    - environ_rebuild: Do not add a new line to rebuild the environment string.

- Interface changes:
    - fd_set_flags: Replaced by io_set_flags

---

## In 0.3.1.0

- Bugs fix:

    - environ_merge_environ(): Prevent overflow and set errno.
    - sastr_replace(): Allow to get a regex even at begins of the line.
    - environ_merge_string(): provide the source code.
    - configure.sh: set --disable-static enabled.

- New functions:
    - Add socket familly function and macro.

---

## In 0.3.0.1

- Provide exlsn_free prototype.

---

## In 0.3.0.0

- New changes and changes of functions:

    - Add lexer_xxx familly. This is complete replacement of the mill_xxx functions. It is faster, easier to debug and brings features not possible with the old mill functions.
    - Add io_read and io_open to read and open files.
    - Revamp environ_substitute. This function now integrate the el_transform process to be able to merge environment with another environment substituting variable by value from getenv call.
    - Revamps of environ_xxx function familly finishing the WIP started at 0.2.1.1 version.
    - Add macro to freed automatically stralloc avoiding to use stralloc_free() function.
    - Improving stack_xxx familly functions providing macro to allocate the stack array with HEAP memory when the size of the array exceed MAX_ENV value.
    - Remove sastr_get_double_quote, scan_uidlist_wdelim, sastr_split_element_in_nline.
    - Replaces sastr_clean_element  by sastr_clean_string.
    - Replaces sastr_clean_string_wdelim by lexer_trim_with_delim.
    - Adapt all sastr_xxx functions with the new lexer_xxx functions familly.
    - Rename sastr_replace_all by sastr_replace_g.
    - Rename stack_string_rebuild by stack_string_rebuild_with_delim.

---

## In 0.2.1.3

- Bugs fix:

    - Retain existing environment variables when calling a module's `configure` script. This issue arose with the changes introduced in the `0.2.1.1` tag.

---

## In 0.2.1.2

- Bugs fix:

    - fix typo at environ_parse_files function name

---

## In 0.2.1.1

- Replace stack_split_element_in_nline by macro stack_split_string_in_nelement. Provide generic stack_split_string function.
- Bugs fix:

    - invocating relative path as a file at dir_beabsolute.c.
    - comparison of key names at environ_get_val_of_key.c. This also fix the 66 [issue](https://git.obarun.org/Obarun/66/-/issues/11)

- [WIP] Revamp of environ_xxx function familly. These functions are badly formatted, designed, named with also bad algorythm.

---

In 0.2.1.0

- Remove obstr_alloc, obstr_del, free_mem and getopt_args function

---

In 0.2.0.2

- Fix macro definition of stack_xxx familly.

---

In 0.2.0.1

- Bugs fix
- New functions:
    - Add stack_clean_string_g, stack_copy_g and stack_copy_stack macro
    - Add stack_clean_element, stack_split_element_in_nline and stack_clean_string function

---
In 0.2.0.0

- Adapt to skalibs 2.14.0.1

- Adapt to execline 2.9.4.0

- Bugs fix

- New functions:
    - Add graph_xxx familly function.
    - Provide char **environ prototype (Thanks POSIX not providing it).
    - Add auto_buf_builder function and auto_buf macro.
    - Add dir_rm_rf function. Skarnet rm_rf function do not freed satmp stralloc.
    - Add FLAGS_SET, FLAGS_CLEAR and FLAGS_GET macro.
    - Add sastr_clean_string_flush_sa function.
    - Add stack_xxx familly function.

- Deprecate functions:
    - remove oblist.h header file.
    - remove obstr_{trim,equal,replace,dup,sep} function.

- change interface of sastr_dir_get and sastr_dir_get_recursive.c allowing to pass multiple string at exclude parameter.

- mill_xxx function use STACK instead of HEAP memory.

---

In 0.1.4.0

- Adapt to skalibs 2.11.0.0

- Adapt to execline 2.8.1.0

- Remove deprecated functions basename() and dirname().

- Bug fix:
    - environ_get_val_of_key: be sure to deal with environment variable.

- Remove slashpackage convention.
---

In 0.1.3.0

- New functions:
    - environ_{drop_commented_line,drop_previous_same_key,get_commented_line,keyvalue_isvalid,clean_envfile_unexport}: see *environ.h* headers file.

- Deprecate functions:
    - environ_get_{envfile,envfile_error,envfile_n_merge}

- Complete revamp of the environ_clean_envfile function: this function now deal with multiple file and handle correctly a same `key=value` found on different file. That means that for a same `key=value` at file A1 et A2, the `key=value` pair keep is the one from A2. The `execl-envfile` reflect this behavior too.

---

In 0.1.2.0

- Adapt to skalibs 2.10.0.0

- Adapt to execline 2.7.0.0

- Bug fix:
    - sastr_replace: don't keep the last 0 at the lenght of the string.

- New function:
    - file_write_unsafe_g: same as file_write_unsafe but accept path containing the name of the file to write.

---

In 0.1.1.1

- Bug fix:
    - respect order of the file at environment parse process

---

In 0.1.1.0

- Bugs fix:
    - sastr_replace():
        - prevent infinite loop when @regex and @by are equal
        - prevent infinite loop when @regex match the end of the line
    - sastr_replace_all(): close the string when the parse is finished

- New function:
    - str_contain(): search for a regex on a string
    - dir_is_child(): look if @child is a submount of @parent
    - sastr_{dir_drop_child,drop_same_element,remove_element,sort,sortndrop_element,to_char}: see sastr include file
    - str_start_with(): look if @str begin by @search.
    - dir_unslash(): remove the last slash of a str.
    - add macro N_ELEMENTS: this macro point to nb_el macro

- Rename COPYING file to LICENSE

---

In 0.1.0.0

- Bugs fix:
    - environ_get_envfile_error() break element of the switch statement
    - environ_get_envfile() check the correct return value of environ_clean_envfile()
    - cross compilation

- MAXENV has a value now of 8191

- New function:
    - version_cmp(): compare two version

---

In 0.0.9.0

- log.c file:
    - allow to set the color by COLOR_ENABLED environment variable.

- new functions:
    - get_gidbygroup: get the group's gid.
    - get_gidbyname: get the user's gid.
    - get_groupbyid: get the name's group.

---

In 0.0.8.0

- improve log.c file:
    - better portability for color declaration
    - add ablink and blue color at set_color_s struct
    - add set_double_output, set_trailing_newline, set_switch_stream,set_default_msg, set_clock_enabled, set_clock_timestamp and set_redirfd_{1,2} function
    - add macro log_1_trace family, log_fatal
    - add environment variable DOUBLE_OUTPUT, TRAILING_NEWLINE, SWITCH_STREAM, DEFAULT_MSG, CLOCK_ENABLED, CLOCK_TIMESTAMP and REDIRFD_{1,2}
- sastr_replace:
    - deny regex to search and regex to replace as the same
- fix environ_clean_envfile, environ_get_clean_env and environ_clean_envfile in case of empty environment file
- add version_scan function
- file_readputsa return a sa->len of zero if the file is empty(previously one due of the stralloc_0() call)
- add file_readputsa_g function
- fix sastr_dir_get_recursive:
    - avoid possible loop with symlink using lstat_at instead of stat_at
- add sastr_find_element_by{id,name}, sastr_nelement function

---

In 0.0.7.0

- add sastr_dir_get_element function
- add log_1_warn? macro

---

In 0.0.6.1

- Bug fix: implement again oblist.h to keep compatibility with 66 version 0.2.5.2 or lower

---

In 0.0.6.0

- Rename basename and dirname to ob_basename and ob_dirname respectively
- add macro log_info_nclean_return at log.h hearder file
- add sastr_dir_get_recursive funciton
- remove oblist.h function

---

In 0.0.5.1

- Configuration files build fixs
- Remove useless function:
    - byte_tosearch
    - byte_tozero
    - dir_search
    - dir_unslash
    - bytes.h
    - all oblist_?

---

In 0.0.5.0

- Add auto_? function familly
- Fix handle of config.h file at distclean

---

In 0.0.4.0

- Add log_? familly function:
- allow to pass any number of string instead of specified it at every use.
- easy implemetation of the color
    - standardization of the exit code
    - standardization of the verbosity without the need to check it at every output
    - easy to hack and add additional output function
    - Remove useless file:
        - error.h, error2.h
        - stdout_die_?
        - obcolor

---

In 0.0.3.2

- Bug fix: fix empty value for a key at the end of a file

---

In 0.0.3.1

- Fix install file

---

In 0.0.3.0

- Remove deprecated function:
    - all stralist
    - all strakeyval
    - directory: dir_cmp, dir_create_under,dir_get,dir_scan_absopath
    - string: clean_val,clean_val_wdelim,get_nbline_ga,str_append_check
- Remove TAINNOW_LIB as sysdeps

---

In 0.0.2.1

- Fix sastr_replace: replace multiple regex on a same line

---

In 0.0.2.0

- Bugs fix
- Implementation of the mill parser
- New functions: get_sep_before, sastr_?, environ_?
- Function dir_create_under is deprecated, use dir_create_parent instead
- Rewrite of scan_timeout, scan_uidlist, scan_uidlist_wdelim
- Deprecated function: All strakeyval, all stralist,dir_{get,cmp,absopath}, clean_{val,val_wdelim},get_nbline_ga,str_append_check

---

In 0.0.1.3

- Fix directory.h headers file

---

In 0.0.1.2

- Add dir_create_parent, dir_beabsolute, basename, dirname, sastr_?, get_flags functions
- Check multiple flags at scan_mode, dir_get functions

---

In 0.0.1.1

- Heap memory management

---

In 0.0.1.0

- build everything as PIC by default
- fix HEAP memory management
- add get_rstrlen_until
- strerr_dieusage take arguments instead of USAGE

---

In 0.0.0.3

- add dir_cmp
