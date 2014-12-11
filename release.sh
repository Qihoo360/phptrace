#!/bin/bash
TAG=${1:-`git describe --tags --abbrev=0 master`}
VER=${TAG#v*}
echo pack tag $TAG
git archive --format=tar --prefix=phptrace-$VER/ $TAG | gzip > phptrace-$VER.tar.gz
