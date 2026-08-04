# oblibs - A general-purpose C library for the Obarun ecosystem

oblibs is a small, self-contained C library used by [obarun.org](https://web.obarun.org)
projects.
It gathers the low-level building blocks — data structures, I/O, process and
event handling — that these projects share, behind a consistent and explicit API.

As of the latest release, oblibs is **fully independent**. Every primitive it used to borrow now has a native
implementation, so linking against oblibs pulls in nothing but the C library.

## What it provides

- Build text without thinking about allocation: growable buffers that live on
  the stack until they outgrow it, string lists you can sort, deduplicate,
  split and rejoin, and a configurable tokenizer for line-oriented formats.
- Talk to a file descriptor at whichever level suits you — raw reads and
  writes, buffered streams, ring buffers, or framed messages — and pass open
  descriptors between processes over a unix socket.
- Read and write whole files, including atomic replacement, so a concurrent
  scanner never observes a partial or supernumerary file.
- Drive an entire process from a single event loop. I/O readiness, signals,
  timers, child exits, filesystem changes and cron-style schedules all arrive
  as descriptor events: no polling, no signal-handler races.
- Keep data in structures sized for a long-lived process: a hash table that
  never copies or frees your elements, a fixed-capacity variant that never
  allocates at all, a priority queue, a directed graph with cycle detection,
  bitsets, and a constant database.
- Parse, merge and rewrite environment files, with a native variable
  substitution engine.
- Spawn and reap children, resolve users and groups, manipulate directories,
  and convert timestamps.
- Parse a command line — short and long options, operands, sub-commands — and
  let the help text be generated from the same description.
- Log at a level, with or without formatting.

Each of these is a single header under `src/include/oblibs/`, all installed.

## Installation

See the INSTALL.md file.

## Documentation

Public functions are documented directly in the header files
(`src/include/oblibs/`).

## Contact information

* Email:
  Eric Vidal `<eric@obarun.org>`

* Web site:
  https://web.obarun.org/

* XMPP Channel:
  obarun@conference.xmpp.obarun.org

## Supports the project

Please consider to make [donation](https://web.obarun.org/donate/)

## License

ISC license. See the LICENSE file.
