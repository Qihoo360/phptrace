#!/bin/bash
cd `dirname $0`
BC_res="\e[0m"
BC_YEL="\e[1;33m"
BC_RED="\e[1;31m"
BC_GRE="\e[1;32m"

function try_compile()
{
    path=$1
    php="$path/bin/php"
    phpize="$path/bin/phpize"
    phpcfg="$path/bin/php-config"
    if [ ! -f $phpcfg ]; then
        echo -e "${BC_RED}invalid path${BC_res} $path"
        return 1
    fi

    vertext=`$php -n --version 2>/dev/null`
    if [ $? -eq 0 ]; then
        version="`echo $vertext | awk '{print $2}'`"
    else
        version="`basename $path`"
        vertext="path:$version"
    fi
    echo -e "${BC_GRE}$version${BC_res} @ ${BC_YEL}$path${BC_res}"

    # clean
    make clean      >/dev/null 2>&1
    $phpize --clean >/dev/null 2>&1

    # prepare, make
    mkdir -vp modules_test
    $phpize && \
    ./configure --with-php-config=$phpcfg && \
    make EXTRA_CFLAGS=-DTRACE_DEBUG && \
    cp -v modules/trace.so modules_test/trace.so.${version}
    ret=$?

    if [ $ret -eq 0 ]; then
        echo -e "${BC_GRE}[DONE]${BC_res} \c"
    else
        echo -e "${BC_RED}[FAIL]${BC_res} \c"
    fi
    echo -e "${BC_GRE}$version${BC_res} @ ${BC_YEL}$path${BC_res}"

    return $ret
}

if [ $# -lt 1 ]; then
    echo "Usage: $0 <php-path>..."
fi

# Main
for php_path in $@; do
    try_compile $php_path && make test
done
