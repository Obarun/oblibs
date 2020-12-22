# Changelog for oblibs

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
