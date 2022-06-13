#!/bin/bash

CURDIR=$(cd $(dirname $0)/ && pwd)

patchset=https://cdn.kernel.org/pub/linux/kernel/projects/rt/5.0/patch-5.0.21-rt16.patch.xz

cd $CURDIR
if [ ! -f feature.downloaded ]; then
    wget -c $patchset
    [ $? -eq 0 ] && xz -d `basename $patchset` && touch feature.downloaded
fi
