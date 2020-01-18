#!/bin/sh

skalibs_tag="v2.9.1.0"
execline_tag="v2.5.3.0"


## skalibs
build_skalibs() {

	git clone https://github.com/skarnet/skalibs
	cd skalibs
	git checkout tags/"${skalibs_tag}"
	./configure \
		--prefix=/usr \
		--with-default-path=/usr/bin \
		--enable-shared
	
	make install || return 1
	cd ..
}

## execline
build_execline() {

	git clone https://github.com/skarnet/execline
	cd execline
	git checkout tags/"${execline_tag}"
	./configure \
		--prefix=/usr \
		--libexecdir=/usr/libexec \
		--bindir=/usr/bin \
		--sbindir=/usr/bin \
		--shebangdir=/usr/bin \
		--enable-shared
	
	make install || return 1
	cd ..
}

_run() {

	if ! ${1} ; then
		printf "%s" "unable to build ${1#*_}"
		exit 1
	fi
}

## do it
_run build_skalibs
_run build_execline
