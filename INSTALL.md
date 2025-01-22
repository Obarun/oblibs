Build Instructions
------------------

## Requirements

- A POSIX-compliant C development environment
- GNU make version 3.81 or later
- skalibs version 2.14.3.0 or later: http://skarnet.org/software/skalibs/
- execline version 2.9.6.1 or later: http://skarnet.org/software/execline/
- If cross-compiling: the sysdeps for your target architecture (see the "Cross-compilation" section below)

This software will install on any operating system that implements POSIX.1-2008, available at [opengroup](http://pubs.opengroup.org/onlinepubs/9699919799/)


## Standard usage

`./configure && make && sudo make install` will work for most users.
It will install the static libraries in /usr/lib/oblibs, the shared libraries in /lib.

Please note that static libraries in /usr/lib/oblibs *will not* be found by a default linker invocation: you need -L/usr/lib/oblibs.
Other [obarun](https://web.obarun.org) software automatically handles that case if the default configuration is used, but if you change the configuration, remember to use the appropriate *--with-lib* configure option.

You can strip the libraries of their extra symbols via `make strip` before the `make install` phase. It will shave a few bytes off them.


## Customization

You can customize the installation process via flags given to configure. See `./configure --help` for a list of all available configure options.


## Environment variables

Controlling a build process via environment variables is a big and dangerous hammer. You should try and pass flags to configure instead; nevertheless, a few standard environment variables are recognized.

If the CC environment variable is set, its value will override compiler detection by configure. The *--host=HOST* option will still add a HOST- prefix to the value of CC.

The values of *CFLAGS*, *CPPFLAGS* and *LDFLAGS* will be appended to the default flags set by configure. To override those defaults instead of appending to them, use the *CPPFLAGS*, *CFLAGS* and *LDFLAGS* *make variables* instead of environment variables.


## Make variables

You can invoke make with a few variables for more configuration.

*CC*, *CFLAGS*, *CPPFLAGS*, *LDFLAGS*, *LDLIBS*, *AR*, *RANLIB*, *STRIP*, *INSTALL* and *CROSS_COMPILE* can all be overridden on the make command line. This is an even bigger hammer than running `./configure` with environment variables, so it is advised to only do this when it is the only way of obtaining the behaviour you want.

*DESTDIR* can be given on the `make install` command line in order to install to a staging directory.

## Static binaries
By default, binaries are linked against static versions of all the libraries they depend on, except for the libc. You can enforce linking against the static libc with *--enable-static-libc*.

If you are using a GNU/Linux system, be aware that the GNU libc behaves badly with static linking and produces huge executables, which is why it is not the default. Other libcs are better suited to static linking, for instance [musl](http://musl-libc.org/).

## Cross-compilation
skarnet.org packages centralize all the difficulty of cross-compilation in one place: skalibs. Once you have
cross-compiled skalibs, the rest is easy.

- Use the *--host=HOST* option to configure, *HOST* being the triplet for your target.
- Make sure your cross-toolchain binaries (i.e. prefixed with HOST-) are accessible via your *PATH* environment variable.
- Make sure to use the correct version of skalibs for your target, and the correct sysdeps directory, making use of the *--with-include*, *--with-lib*, *--with-dynlib* and *--with-sysdeps*
options as necessary.

## Out-of-tree builds

obarun.org packages do not support out-of-tree builds. They are small, so it does not cost much to duplicate the entire source tree if parallel builds are needed.
