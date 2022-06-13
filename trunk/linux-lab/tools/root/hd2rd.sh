#!/bin/bash
#
# hd2rd.sh rootfs initrd -- harddisk fs image to initrd
#
# Copyright (C) 2016-2021 Wu Zhangjin <falcon@ruma.tech>
#

[ -z "$HROOTFS" ] && HROOTFS=$1
[ -z "$INITRD" ] && INITRD=$2

[ -z "${HROOTFS}" -o -z "${INITRD}" ] && echo "Usage: $0 hrootfs initrd" && exit 1

[ -z "${USER}" ] && USER=$(whoami)

ROOTDIR=$(echo ${INITRD} | sed -e "s%.cpio.gz%%g;s%.cpio%%g")

[ ! -f ${HROOTFS} ] && echo "Usage: ${HROOTFS} not exists" && exit 1

[ -d ${ROOTDIR} ] && rm -rf ${ROOTDIR}

# Creating directory
mkdir -p ${ROOTDIR}.tmp
sudo mount ${HROOTFS} ${ROOTDIR}.tmp

sudo cp -ar ${ROOTDIR}.tmp ${ROOTDIR}

sudo chown ${USER}:${USER} -R ${ROOTDIR}
sync
sudo umount ${ROOTDIR}.tmp
rmdir ${ROOTDIR}.tmp

# building cpio.gz
FS_CPIO_GZ=${INITRD}

if [ -d ${ROOTDIR} -a -d ${ROOTDIR}/bin -a -d ${ROOTDIR}/etc ]; then

  [ -f ${FS_CPIO_GZ} ] && rm ${FS_CPIO_GZ}

  # Add init/linuxrc for basic initramfs
  # ref: linux-stable/Documentation/admin-guide/initrd.rst
  if [ -f $ROOTDIR/linuxrc -a -f $ROOTDIR/busybox ]; then
    pushd $ROOTDIR >/dev/null
    ln -sf busybox linuxrc
    popd >/dev/null
  fi

  [ ! -f $ROOTDIR/init ] && cat <<EOF > $ROOTDIR/init
#!/bin/sh
# devtmpfs does not get automounted for initramfs
/bin/mount -t devtmpfs devtmpfs /dev
exec 0</dev/console
exec 1>/dev/console
exec 2>/dev/console
exec /sbin/init "\$@"
EOF

  [ ! -d $ROOTDIR/etc/init.d ] && mkdir -p $ROOTDIR/etc/init.d
  [ ! -f $ROOTDIR/etc/init.d/rcS ] && cat <<EOF > $ROOTDIR/etc/init.d/rcS
#!/bin/sh


# Start all init scripts in /etc/init.d
# executing them in numerical order.
#
for i in /etc/init.d/S??* ;do

     # Ignore dangling symlinks (if any).
     [ ! -f "$i" ] && continue

     case "$i" in
	*.sh)
	    # Source shell script for speed.
	    (
		trap - INT QUIT TSTP
		set start
		. $i
	    )
	    ;;
	*)
	    # No sh extension, so fork subprocess.
	    $i start
	    ;;
    esac
done
EOF

  sudo chmod a+x $ROOTDIR/init
  sudo chmod a+x $ROOTDIR/etc/init.d/rcS

  cd $ROOTDIR/ && find . | sudo cpio --quiet -R $USER:$USER -H newc -o | gzip -9 -n > ${FS_CPIO_GZ}

  rm -rf $ROOTDIR

  exit 0
fi

echo "ERR: ${HROOTFS} has no a valid rootfs." && exit 1
