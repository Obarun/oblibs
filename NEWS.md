# Changelog for oblibs

---

## In 0.3.2.0

- Bugs Fix:

    - environ_substitute: ensure that all dollars from a same line are handled.
    -

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
