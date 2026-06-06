# Build Instructions

## Requirements

To build and install the oblibs project, you need:

- A POSIX-compliant C development environment (conforming to POSIX.1-2008, available at Open Group).

- `Meson` version `1.1.0` or later: [mesonbuild.com](https://mesonbuild.com).

- `Ninja` (typically installed with Meson).

- Linux API headers version 5.8 or later (required for Linux systems): [gnu.org/software/libc](https://gnu.org/software/libc).

oblibs has no external library dependency: it links against `libc` only.

The software is designed to install on any operating system implementing `POSIX.1-2008`.

## Standard Usage

For most users, the following commands will configure, build, and install the oblibs project with default settings:

```bash
meson setup build
meson compile -C build
meson install -C build
```

This installs:

- Shared libraries (e.g., liboblibs.so) to `/usr/lib` (or the system’s library directory).
- Header files to `/usr/include/oblibs`.

To reduce binary size, you can strip symbols before installation:

```bash
meson compile -C build --strip
meson install -C build
```

## Customization

You can customize the build using Meson options. To see all available options, run:

```bash
meson configure build
```

Example customization:

```bash
meson setup build -D prefix=/usr/local -D enable-shared=false -D enable-static=true -D test=true
meson compile -C build
meson install -C build
```

## Key options include:

- `with-pkgconfig`: Build and install a `/usr/lib/pkgconfig/liboblibs.pc` file(default: `false`).
- `enable-shared`: Build shared libraries (e.g., `liboblibs.so`) for dynamic linking (default: `true`).
- `enable-static`: Build static libraries (e.g., `liboblibs.a`) for static linking (default: `false`).
- `enable-static-executable`: Build fully static executables, including a static `libc`, for maximum portability; requires a static `libc` (e.g., `libc.a`) on the system (default: `false`).
- `enable-all-pic`: Compile static libraries with position-independent code (`PIC`) for use in shared libraries or `PIE` executables (default: `false`).
- `enable-pie`: Build executables as position-independent (`PIE`) for enhanced security via Address Space Layout Randomization (`ASLR`) (default: `false`).
- `test`: Build and run tests (default: `false`).

## Option Combinations

- You can enable both `enable-shared` and `enable-static` to build **both** shared and static libraries.
- `enable-static-executable` conflicts with `enable-shared` and requires a static `libc`.
- `enable-pie` is compatible with most options but may not work with `enable-static-executable` on some systems due to toolchain limitations.
- `enable-all-pic` applies only to static libraries and is compatible with all options.

## Environment Variables

Meson supports a few environment variables for build customization, but passing options directly to meson setup is preferred for clarity:

- `CC`: Overrides the compiler (e.g., `CC=clang meson setup build`). When cross-compiling, the `--cross-file` option may prefix the compiler with the target triplet.

- `CFLAGS`, `CPPFLAGS`, LDF`LAGS: Appended to Meson’s default flags. To override defaults, use Meson options or build variables instead.

## Build Variables

You can pass variables to meson compile or meson install for fine-grained control:

- `CC`, `CFLAGS`, `CPPFLAGS`, `LDFLAGS`, `LDLIBS`: Override compiler, flags, or libraries.
- `AR`, `RANLIB`, `STRIP`, `INSTALL`: Customize archiver, ranlib, strip, or install tools.
- `DESTDIR`: Specify a staging directory for installation.

Example:

```bash
CFLAGS="-O3 -march=native" meson compile -C build
DESTDIR=/tmp/staging meson install -C build
```

# Static Binaries

By default, executables are linked dynamically with `libc` and other dependencies, even if `enable-static` is used. To build fully static executables (including a static `libc`):

- Use `enable-static-executable`. This requires a static `libc` (e.g., `libc.a`) on the system, such as `libc6-dev` (Debian/Ubuntu), `glibc-static` (Fedora), or `musl-dev` (Alpine Linux).

- Note: GNU `libc` produces larger static binaries compared to alternatives like `musl`. For smaller, portable binaries, consider using `musl`.

## Cross-Compilation

oblibs has no external library dependency beyond `libc`, so cross-compilation is
just the standard Meson cross-file workflow:

- Create a Meson cross file (e.g., `cross-file.ini`) specifying the target triplet (e.g., `arm-linux-gnueabihf`) and toolchain paths.
- Ensure the cross-toolchain binaries (e.g., `arm-linux-gnueabihf-gcc`) are in your `PATH`.

Example:

```bash
meson setup build --cross-file=cross-file.ini
meson compile -C build
meson install -C build
```

## Using liboblibs with pkg-config

The build generates a `liboblibs.pc` file for use with `with-pkgconfig`, installed to `${libdir}/pkgconfig` (e.g., `/usr/lib/pkgconfig`). To link against `liboblibs`:

```bash
pkg-config --cflags --libs liboblibs
```

## Notes

- If `enable-static-executable` is enabled, the build will fail with a clear error if a static `libc` is not found. Ensure the appropriate development package is installed (e.g., `libc6-dev`, `musl-dev`).

- For security-sensitive systems, consider enabling `enable-pie` to benefit from `ASLR`.

