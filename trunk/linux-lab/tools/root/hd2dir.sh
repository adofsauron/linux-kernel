#!/bin/bash
#
# hd2dir.sh hrootfs rootdir -- harddisk fs image to directory, not mount
#
# Copyright (C) 2016-2021 Wu Zhangjin <falcon@ruma.tech>
#

[ -z "$HROOTFS" ] && HROOTFS=$1
[ -z "$ROOTDIR" ] && ROOTDIR=$2

[ -z "${HROOTFS}" -o -z "${ROOTDIR}" ] && echo "Usage: $0 rootfs rootdir" && exit 1

[ -z "${USER}" ] && USER=$(whoami)

ROOTDIR=$(echo ${ROOTDIR} | sed -e "s%/$%%g")

[ ! -f ${HROOTFS} ] && echo "Usage: ${HROOTFS} not exists" && exit 1

[ -d ${ROOTDIR} ] && rm -rf ${ROOTDIR}

mkdir -p ${ROOTDIR}.tmp
sudo mount ${HROOTFS} ${ROOTDIR}.tmp

sudo cp -ar ${ROOTDIR}.tmp ${ROOTDIR}

sudo chown ${USER}:${USER} -R ${ROOTDIR}
#sync
sudo umount ${ROOTDIR}.tmp

rmdir ${ROOTDIR}.tmp
