#!/bin/bash
function logit() {
    echo "[php_get] $@" 1>&2
}

function version_ge()
{
    php -nr "exit(version_compare('$1', '$2', '>=') ? 0 : 1);"
}

function download()
{
    url="$1"
    tar="$2"

    wget -t3 -T3 -O ${tar}.tmp $url

    ret=$?
    if [ $ret -eq 0 ]; then
        mv -f ${tar}.tmp $tar
        logit "done $tar"
    else
        rm -f ${tar}.tmp
        logit "fail"
    fi

    return $ret
}


# main
if [ -z "$1" ]; then
    echo "usage: `basename $0` <ver.si.on>"
    exit 1
fi
version="$1"
logit "version: $version"

# choose source
tarfile="php-${version}.tar.bz2"
if version_ge $1 "5.4.0"; then
    url="http://php.net/get/$tarfile/from/this/mirror"
else
    url="http://museum.php.net/php5/$tarfile"
fi

# download
logit "download from $url"
download $url $tarfile
