#
# This Makefile requires GNU make.
#
# Do not make changes here.
# Use the included .mak files.
#

it: all

make_need := 3.81
ifeq "" "$(strip $(filter $(make_need), $(firstword $(sort $(make_need) $(MAKE_VERSION)))))"
fail := $(error Your make ($(MAKE_VERSION)) is too old. You need $(make_need) or newer)
endif

CC = $(error Please use ./configure first)

-include config.mak
include package/deps.mak

version_m := $(basename $(version))
version_M := $(basename $(version_m))
version_l := $(basename $(version_M))
CPPFLAGS_ALL := $(CPPFLAGS_AUTO) $(CPPFLAGS)
CFLAGS_ALL := $(CFLAGS_AUTO) $(CFLAGS)
LDFLAGS_ALL := $(LDFLAGS_AUTO) $(LDFLAGS)
LDLIBS_ALL := $(LDLIBS_AUTO) $(LDLIBS)
REALCC = $(CROSS_COMPILE)$(CC)
AR := $(CROSS_COMPILE)ar
RANLIB := $(CROSS_COMPILE)ranlib
STRIP := $(CROSS_COMPILE)strip
INSTALL := ./tools/install.sh

TYPES := size uid gid pid time dev ino

ALL_SRCS := $(wildcard src/*/*.c)
ALL_DOBJS := $(ALL_SRCS:%.c=%.lo)
ifeq ($(strip $(STATIC_LIBS_ARE_PIC)),)
ALL_SOBJS := $(ALL_SRCS:%.c=%.o)
CFLAGS_SHARED := -fPIC
else
ALL_SOBJS := $(ALL_DOBJS)
CFLAGS_SHARED :=
endif
ALL_INCLUDES := $(wildcard src/include/$(package)/*.h)
ALL_LIBS := $(SHARED_LIBS) $(STATIC_LIBS)
ALL_DATA := $(wildcard src/etc/*)

all: $(ALL_LIBS) $(ALL_DATA) $(ALL_INCLUDES)

clean:
	@exec rm -f $(ALL_LIBS) $(ALL_BINS) $(ALL_SOBJS) $(ALL_DOBJS)

distclean: clean
	@exec rm -rf config.mak package/deps.mak

txz: distclean
	@./tools/gen-deps.sh > package/deps.mak 2> /dev/null && \
	. package/info && \
	rm -rf /tmp/$$package-$$version && \
	cp -a . /tmp/$$package-$$version && \
	cd /tmp && \
	tar -Jpcv --owner=0 --group=0 --numeric-owner --exclude=.git* -f /tmp/$$package-$$version.tar.xz $$package-$$version && \
	exec rm -rf /tmp/$$package-$$version

strip: $(ALL_LIBS)
ifneq ($(strip $(STATIC_LIBS)),)
	exec $(STRIP) -x -R .note -R .comment -R .note.GNU-stack $(STATIC_LIBS)
endif
ifneq ($(strip $(SHARED_LIBS)),)
	exec $(STRIP) -R .note -R .comment -R .note.GNU-stack $(SHARED_LIBS)
endif

install: install-data install-dynlib install-lib install-include
install-data: $(ALL_DATA:src/etc/%=$(DESTDIR)$(datadir)/%)
install-dynlib: $(SHARED_LIBS:lib%.so.xyzzy=$(DESTDIR)$(dynlibdir)/lib%.so)
install-lib: $(STATIC_LIBS:lib%.a.xyzzy=$(DESTDIR)$(libdir)/lib%.a)
install-include: $(ALL_INCLUDES:src/include/$(package)/%.h=$(DESTDIR)$(includedir)/$(package)/%.h)

ifneq ($(exthome),)

$(DESTDIR)$(exthome): $(DESTDIR)$(home)
	exec $(INSTALL) -l $(notdir $(home)) $(DESTDIR)$(exthome)

update: $(DESTDIR)$(exthome)

global-links: $(DESTDIR)$(exthome) $(SHARED_LIBS:lib%.so.xyzzy=$(DESTDIR)$(sproot)/library.so/lib%.so.$(version_M))

$(DESTDIR)$(sproot)/library.so/lib%.so.$(version_M): $(DESTDIR)$(home)/library.so/lib%.so.$(version_M)
	exec $(INSTALL) -D -l ..$(subst $(sproot),,$(exthome))/library.so/$(<F) $@

.PHONY: update global-links

endif

$(DESTDIR)$(datadir)/%: src/etc/%
	exec $(INSTALL) -D -m 644 $< $@

$(DESTDIR)$(dynlibdir)/%.so: %.so.xyzzy
	$(INSTALL) -D -m 755 $< $@.$(version) && \
	$(INSTALL) -l $(@F).$(version) $@.$(version_m) && \
	$(INSTALL) -l $(@F).$(version_m) $@.$(version_M) && \
	exec $(INSTALL) -l $(@F).$(version_M) $@

$(DESTDIR)$(libdir)/lib%.a: lib%.a.xyzzy
	exec $(INSTALL) -D -m 644 $< $@

$(DESTDIR)$(includedir)/$(package)/%.h: src/include/$(package)/%.h
	exec $(INSTALL) -D -m 644 $< $@

%.o: %.c
	exec $(REALCC) $(CPPFLAGS_ALL) $(CFLAGS_ALL) -c -o $@ $<

%.lo: %.c
	exec $(REALCC) $(CPPFLAGS_ALL) $(CFLAGS_ALL) $(CFLAGS_SHARED) -c -o $@ $<

liboblibs.a.xyzzy: $(ALL_SOBJS)
	exec $(AR) rc $@ $^
	exec $(RANLIB) $@

liboblibs.so.xyzzy: $(ALL_DOBJS)
	exec $(REALCC) -o $@ $(CFLAGS_ALL) $(CFLAGS_SHARED) $(LDFLAGS_ALL) $(LDFLAGS_SHARED) -Wl,-soname,liboblibs.so.$(version_M) $^ 

.PHONY: it all clean distclean txz strip install install-data install-dynlib install-lib install-include
