#!/bin/sh

tag=1

if [ "$1" == "commit" ]; then
    tag=0
fi

skalibs_tag="v2.14.0.1"
execline_tag="v2.9.4.0"

check_tag(){

    if ((tag)); then
        git checkout tags/"${1}"
    fi
}

## skalibs
build_skalibs() {

    git clone https://github.com/skarnet/skalibs
    cd skalibs
    check_tag "${skalibs_tag}"
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
    check_tag "${execline_tag}"
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
