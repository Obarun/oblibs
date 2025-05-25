project('oblibs',
        'c',
        version: '0.3.4.0',
        meson_version: '>=1.1.0',
        license: 'ISC',
        default_options: [
          'c_std=c99',
          'prefix=/usr',
          'enable-shared=true',
          'with-pkgconfig=false',
          'warning_level=2',
          'optimization=2',
          'buildtype=plain',
          'default_library=shared'
        ])

message('See INSTALL_MESON.md for detailed configuration instructions and warnings.')

fs = import('fs')

# Versioning
version_skalibs = '>=2.14.3.0'
version_execline = '>=2.9.6.1'

# Compiler setup
cc = meson.get_compiler('c')

# Define dependency checks
# Common include and library paths
default_include_dirs = [get_option('prefix') / get_option('includedir')]
default_lib_dirs = [get_option('prefix') / get_option('libdir')]
user_include_dirs = get_option('with-include-dir')
user_staticlib_dirs = get_option('with-staticlib-dir')
user_dynamiclib_dirs = get_option('with-dynamiclib-dir')

# Combine default and user-provided paths
include_dirs = default_include_dirs + user_include_dirs
lib_dirs = default_lib_dirs + user_staticlib_dirs + user_dynamiclib_dirs

# Dependency: skalibs
skalibs_lib = cc.find_library('skarnet', dirs: lib_dirs, static: get_option('enable-static-deps'), required: false)
skalibs_header = cc.has_header('skalibs.h', args: ['-I', include_dirs], required: false)
if not skalibs_lib.found() # or not skalibs_header
  error('skalibs ' + version_skalibs + ' not found. Ensure libskalibs and skalibs.h are installed, or specify paths with with-include-dir and with-staticlib-dir/with-dynamiclib-dir.')
endif
dep_skalibs = declare_dependency(
  dependencies: skalibs_lib,
  include_directories: include_dirs
)

# Dependency: execline
execline_lib = cc.find_library('execline', dirs: lib_dirs, static: get_option('enable-static-deps'), required: false)
execline_header = cc.has_header('execline.h', args: ['-I', include_dirs], required: false)
if not execline_lib.found() or not execline_header
  error('execline ' + version_execline + ' not found. Ensure libexecline and execline/execline.h are installed, or specify paths with with-include-dir and with-staticlib-dir/with-dynamiclib-dir.')
endif
dep_execline = declare_dependency(
  dependencies: execline_lib,
  include_directories: include_dirs
)

# Define global dependencies
skalibs_dep = dep_skalibs
execline_dep = dep_execline

# Special case for default_options
INCLUDEDIR = get_option('prefix') / get_option('includedir') / 'oblibs'

# Common compiler flags
base_c_args = [
  '-pipe',
  '-fomit-frame-pointer',
  '-fno-exceptions',
  '-fno-unwind-tables',
  '-fno-asynchronous-unwind-tables',
  '-Werror=implicit-function-declaration',
  '-Werror=implicit-int',
  '-Werror=pointer-sign',
  '-Werror=pointer-arith',
  '-Wno-unused-value',
  '-Wno-parentheses',
  '-ffunction-sections',
  '-fdata-sections',
  '-D_POSIX_C_SOURCE=200809L',
  '-D_XOPEN_SOURCE=700',
  '-D_GNU_SOURCE',
  '-I' + get_option('includedir')
]

# Compiler flags
shared_c_args = base_c_args + ['-fPIC']
static_c_args = get_option('enable-all-pic') ? base_c_args + ['-fPIC'] : base_c_args
exe_c_args = base_c_args

# Linker flags
shared_link_args = [
  '-shared',
  '-Wl,--as-needed',
  '-Wl,--no-undefined',
  '-Wl,-O2',
  '-Wl,--sort-section=alignment',
  '-Wl,--sort-common'
]

if cc.has_link_argument('-Wl,--hash-style=both')
  shared_link_args += ['-Wl,--hash-style=both']
endif

static_link_args = get_option('enable-static-deps') ? ['-static', '-Wl,--gc-sections'] : []
exe_link_args = []
if get_option('enable-static-executable')
  exe_link_args += ['-static']
endif

# Add include paths
foreach inc : get_option('with-include-dir')
  if inc != '' and fs.is_dir(inc) and inc != meson.project_source_root() / 'src/include'
    add_project_arguments('-I' + inc, language: 'c')
  endif
endforeach

# Add library paths
foreach lib : get_option('with-staticlib-dir')
  if lib != '' and fs.is_dir(lib)
    shared_link_args += ['-L' + lib]
    static_link_args += ['-L' + lib]
    exe_link_args += ['-L' + lib]
  endif
endforeach
foreach dynlib : get_option('with-dynamiclib-dir')
  if dynlib != '' and fs.is_dir(dynlib)
    shared_link_args += ['-L' + dynlib]
    exe_link_args += ['-L' + dynlib]
  endif
endforeach

# Verify static libc availability if enable-static-executable is true
if get_option('enable-static-executable')
  static_libc_test = cc.find_library('c', static: true, required: false)
  if not static_libc_test.found()
    error('enable-static-executable requires a static libc, but it was not found on the system')
  endif
  # Test linking a static program
  static_link_test = cc.links('int main() { return 0; }', name: 'static executable test', args: ['-static'], dependencies: [static_libc_test])
  if not static_link_test
    error('enable-static-executable requires a toolchain capable of linking fully static executables')
  endif
endif

# Validate options to avoid conflicts
if get_option('enable-static-executable') and get_option('enable-shared')
  error('Cannot enable both enable-static-executable and enable-shared')
endif
if get_option('enable-static-deps') and get_option('enable-shared')
  error('Cannot enable both enable-static-deps and enable-shared')
endif
if get_option('enable-static-deps') and not get_option('enable-static')
  error('enable-static-deps requires enable-static to build liboblibs.a')
endif

# Include directories
incdir = include_directories(['src/include'])

# Subdirectories
subdir('src/include/oblibs')
subdir('src/')

if get_option('with-pkgconfig')
  # Generate and install pkg-config file for liboblibs
  private_libs = []
  if get_option('enable-static') or get_option('enable-static-deps')
    private_libs += ['-lskalibs', '-lexecline']
  endif

  pkg = import('pkgconfig')
  pkg.generate(
    name: 'liboblibs',
    filebase: 'liboblibs',
    description: 'Small Library for Obarun C project',
    version: meson.project_version(),
    libraries: ['-loblibs'],
    libraries_private: private_libs,
    subdirs: ['oblibs'], # Headers in ${includedir}/oblibs
  )
endif