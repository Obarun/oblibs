# Changelog for oblibs

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
