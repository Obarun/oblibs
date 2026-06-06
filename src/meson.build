fs = import('fs')

libob_dir = meson.current_source_dir()
find_cmd = run_command('sh', '-c', 'find "@0@" -type f -name "*.c" ! -path "*/test/*"'.format(libob_dir), check: false)
if find_cmd.returncode() != 0
  error('Failed to run find command: ' + find_cmd.stderr())
endif

libob_sources = []
foreach file : find_cmd.stdout().strip().split('\n')
  if file != '' and fs.is_file(file)
    libob_sources += files(file)
  endif
endforeach

if libob_sources.length() == 0
  warning('No .c files found in src/')
endif

soversion_ob = meson.project_version()
libob_shared = []
libob_static = []
is_shared = get_option('enable-shared')
is_static = get_option('enable-static')

if is_shared
  libob_shared = shared_library('oblibs',
    sources: libob_sources,
    include_directories: incdir,
    install: true,
    install_dir: get_option('libdir'),
    soversion: '@0@'.format(soversion_ob),
    c_args: shared_c_args,
    link_args: shared_link_args,
    install_rpath: get_option('libdir'),
    build_rpath: meson.current_build_dir()
  )
endif

if is_static
  lib_static_path = join_paths(get_option('libdir'))
  libob_static = static_library('oblibs',
    sources: libob_sources,
    include_directories: incdir,
    install: true,
    install_dir: lib_static_path,
    c_args: static_c_args,
    link_args: static_link_args
  )
endif

if is_shared == false and is_static == false
  error('At least one of enable-shared or enable-static must be true to define libob_static')
endif

libob_dep = declare_dependency(
  include_directories: incdir,
  link_with: get_option('enable-shared') ? libob_shared : libob_static
)

header_dir = meson.project_source_root() / 'src/include/oblibs'
find_headers_cmd = run_command('sh', '-c', 'find "@0@" -type f -name "*.h"'.format(header_dir), check: false)
if find_headers_cmd.returncode() != 0
  error('Failed to run find command for headers: ' + find_headers_cmd.stderr())
endif

header_files = []
foreach file : find_headers_cmd.stdout().strip().split('\n')
  if file != '' and fs.is_file(file)
    header_files += file
  endif
endforeach

if header_files.length() > 0
  install_headers(header_files, subdir: 'oblibs')
else
  warning('No header files found in src/include/oblibs')
endif

if get_option('test')
  find_test_dirs_cmd = run_command('sh', '-c', 'find "@0@" -type d -name "test"'.format(libob_dir), check: false)
  if find_test_dirs_cmd.returncode() != 0
    error('Failed to run find command for test directories: ' + find_test_dirs_cmd.stderr())
  endif

  test_dirs = find_test_dirs_cmd.stdout().strip().split('\n')
  foreach test_dir : test_dirs
    if test_dir != '' and fs.is_dir(test_dir)
      find_test_files_cmd = run_command('sh', '-c', 'find "@0@" -type f -name "*.c"'.format(test_dir), check: false)
      if find_test_files_cmd.returncode() != 0
        error('Failed to run find command for test files in ' + test_dir + ': ' + find_test_files_cmd.stderr())
      endif

      test_files = find_test_files_cmd.stdout().strip().split('\n')
      foreach test_file : test_files
        if test_file != '' and fs.is_file(test_file)
          # Extract test file name without extension for executable and test name
          test_name = fs.stem(test_file)  # e.g., 'io_inotify' from 'io_inotify.c'
          subdir_name = test_dir.split('/').get(-2)  # e.g., 'io' from 'src/io/test'
          test_exe = executable('test_@0@_@1@'.format(subdir_name, test_name),
            sources: [test_file],
            include_directories: incdir,
            dependencies: [libob_dep],
            build_by_default: false,
            c_args: exe_c_args,
            link_args: get_option('enable-static-executable') ? static_link_args : exe_link_args,
            pie: get_option('enable-pie')
          )
          test('@0@_@1@_test'.format(subdir_name, test_name), test_exe, env: {'LD_LIBRARY_PATH': meson.current_build_dir()})
        endif
      endforeach
    endif
  endforeach
endif
