CC = gcc
CFLAGS=-g -Wall -Wextra -std=c99 -D_XOPEN_SOURCE=700 -I../../include

LIBS_PATH = -L/usr/lib/skalibs \
			-L/usr/lib/execline

LIBS = 	-lexecline \
		-lskarnet \
		../../../liboblibs.so.xyzzy

LDFLAGS = $(LIBS_PATH) $(LIBS)
# List all source files for tests
# parse_bracket.c another_test.c some_other_test.c
TEST_SOURCES = graph.c
TEST_BINARIES = $(TEST_SOURCES:.c=)

all: $(TEST_BINARIES)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

run: $(TEST_BINARIES)
	@for binary in $(TEST_BINARIES); do \
		echo "Executing $$binary"; \
		LD_PRELOAD=$(abspath ../../../liboblibs.so.xyzzy) LD_LIBRARY_PATH=../../../:$$LD_LIBRARY_PATH ./$$binary || exit 1; \
	done

clean:
	@exec rm -f $(TEST_BINARIES) $(wildcard *.o *.lo)

.PHONY: run clean