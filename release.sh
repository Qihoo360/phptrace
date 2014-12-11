#!/bin/bash
TAG=${1:-`git describe --tags --abbrev=0 master`}
VER=${TAG#v*}
PKG_NAME=php-trace
echo pack tag $TAG
git archive --format=tar --prefix=$PKG_NAME-$VER/ $TAG | gzip > $PKG_NAME-$VER.tar.gz
