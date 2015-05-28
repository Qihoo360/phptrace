#!/bin/bash
function logit() {
    echo "[php_build] $@" 1>&2
}

function build_from_tar()
{
    tar="$2"
    src=`echo $tar | sed 's/^.*\/\?\(php-[0-9]\+\.[0-9]\+\.[0-9]\+\)\.tar\.bz2$/\1/'`
    if [ -z "$src" ]; then
        return 1
    fi

    # prepare normal
    logit "extract tar ball"
    rm -fr $src && tar jxf $tar

    # build
    build $1 $src
}

function build()
{
    # init
    prefix=$1
    src=$2

    # debug
    param_debug=""
    prefix_debug=""
    if [ -n "$3" ]; then
        param_debug="--enable-debug"
        prefix_debug="-debug"
    fi

    # version related
    version=`grep ' PHP_VERSION ' $src/main/php_version.h | sed 's/^#define PHP_VERSION "\([0-9a-zA-Z\.]\+\)".*$/\1/'`
    buildname="php-${version}${prefix_debug}"
    logit "[$buildname] build"

    # prepare
    cd $src
    param_general="--disable-all $param_debug"
    param_sapi="--enable-cli --disable-cgi"
    # hack for PHP 5.2
    if [ ${version:0:3} == "5.2" ]; then
        # pcre: run-tests.php
        # spl:  spl_autoload_register in trace's tests
        param_ext="--with-pcre-regex --enable-spl"
    else
        param_ext=""
    fi
    cmd="./configure --quiet --prefix=$prefix/$buildname $param_general $param_sapi $param_ext"

    # configure
    logit "[$buildname] configure"
    logit "$cmd"
    $cmd

    # make and install
    logit "[$buildname] make"
    # NOT DO a meaningless "make clean"! it's just extracted
    make --quiet && \
    make install
    ret=$?

    if [ $ret -eq 0 ]; then
        logit "[$buildname] done"
    else
        logit "[$buildname] fail"
    fi
}

# main
if [ $# -lt 2 ]; then
    echo "usage: `basename $0` <prefix> <php-tarfile>"
    exit 1
fi

# argument
prefix="$1"
if [ ! -d "$prefix" ]; then
    logit "error: invalid prefix \"$prefix\""
    exit 1
fi
logit "prefix: $prefix"
tarfile="$2"
if [ ! -f "$tarfile" ]; then
    logit "error: invalid PHP tar file \"$tarfile\""
    exit 1
fi
logit "tarfile: $tarfile"

# build
build_from_tar $prefix $tarfile
