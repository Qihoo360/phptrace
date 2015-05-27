#!/bin/bash
function logit() {
    echo "[ext_build] $@" 1>&2
}

function build()
{
    php_path=`cd $2; pwd`
    php="$php_path/bin/php"
    phpize="$php_path/bin/phpize"
    phpcfg="$php_path/bin/php-config"
    if [ ! -f $phpcfg ]; then
        logit "invalid PHP path $php_path"
        return 1
    fi

    # change to extension path
    cd $1

    # clean
    make clean      >/dev/null 2>&1
    $phpize --clean >/dev/null 2>&1

    # configure, make
    $phpize &&
    ./configure --with-php-config=$phpcfg && \
    make EXTRA_CFLAGS=-DTRACE_DEBUG && \
    make install
    ret=$?

    if [ $ret -eq 0 ]; then
        logit "done"
    else
        logit "fail"
    fi

    return $ret
}

# main
if [ $# -lt 2 ]; then
    echo "usage: `basename $0` <extension> <php-path>"
    exit 1
fi

# argument
ext_path="$1"
if [ ! -d "$ext_path" ]; then
    logit "error: invalid extension \"$ext_path\""
    exit 1
fi
logit "ext_path: $ext_path"
php_path="$2"
if [ ! -d "$php_path" ]; then
    logit "error: invalid PHP path \"$php_path\""
    exit 1
fi
logit "php_path: $php_path"

# build
build $ext_path $php_path
