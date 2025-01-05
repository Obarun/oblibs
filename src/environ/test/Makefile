PROGRAM = environ_substitute

CC = gcc


CFLAGS=-g -Wall -Wextra -std=c99 -D_XOPEN_SOURCE=700

LIBS_PATH = -L/usr/lib/skalibs \
			-L/usr/lib/execline \
            -L/usr/lib/oblibs \

LIBS = 	-loblibs \
		-lexecline \
		-lskarnet

build:

	$(CC) $(CFLAGS) -c $(PROGRAM).c
	$(CC) $(CFLAGS) $(PROGRAM).o  $(LIBS) $(LIBS_PATH) -o $(PROGRAM)

.PHONY: build