#!/bin/bash

#
# Copyright 2021-2025 (c) Mohammed Isam
#
# This script is part of LaylaOS
#
# It will create the initial RAM Disk (initrd) and populate it with
# essential directories and files, needed by the kernel to run.
#

print_usage()
{
    echo "Usage:"
    echo "  $0 [root {hda[1..8] | sda[1..8]}] [sysroot PATH]"
}

# sudo is not available on LaylaOS yet
myname=`uname -s`
if [ "$myname" == "LaylaOS" ]; then
    SUDO=
else
    SUDO=sudo
fi

# process arguments
i=1
j=$#

ROOT_DISK=hda1
CWD=`pwd`

# use ./build/sysroot as the cross sysroot by default
SYSROOT=${CWD}/build/sysroot

while [ $i -le $j ]
do
    case $1 in
        root)
            i=$((i + 1))
            shift 1
            ROOT_DISK=$1
            ;;
        sysroot)
            i=$((i + 1))
            shift 1
            SYSROOT=$1
            ;;
    esac
    
    i=$((i + 1))
    shift 1
done

# check this before we go deeper
if ! [ -d "${CWD}/../others" ]; then
    echo "$0: Cannot find source dir others/"
    echo "    Are you sure you ran this script from the build-scripts/ dir"
    echo "    in the source tree?"
    exit 1
fi

# some useful constants
SIZE=6144              # 6 MiB
BLKSIZE=1024
MAXSIZE=$(( 16 * 1024 * 1024 ))		# max size we support is 16 MiB
DIR=./initrd_tmp
IMAGE=./initrd.img

# sanity check
oursize=$(( ${SIZE} * ${BLKSIZE} ))

#echo "sz $oursize"
#echo "mx $MAXSIZE"

if [ ${oursize} -gt ${MAXSIZE} ]; then
  echo "*** Invalid initrd size: ${oursize} bytes"
  echo "*** Maximum size supported is: ${MAXSIZE} bytes"
  exit 1
fi

# remove old rdisk, if any
if [ -e ${IMAGE} ]; then
   echo "=> Removing old initrd.img"
   rm ${IMAGE}
fi

if [ -e ${IMAGE}.gz ]; then
   echo "=> Removing old initrd.img.gz"
   rm ${IMAGE}.gz
fi

# create empty ramdisk file
echo "=> Creating ${SIZE}KiB initrd file"
dd if=/dev/zero of=${IMAGE} bs=${BLKSIZE} count=${SIZE}

# create mount point
if [ ! -d $DIR ]; then
   echo "=> Creating mount point: ${DIR}"
   mkdir ${DIR}
fi

#set ramdisk file for mounting
echo "=> Setting up ramdisk for mounting"

for lonum in $(seq 0 256)
do
    ${SUDO} losetup /dev/loop${lonum} ${IMAGE} >/dev/null 2>&1 && break
done

# make ext2 file system
# use -m 0 to prevent reserving space for root
echo "=> Making Ext2 filesystem"
mke2fs -F -m 0 -b ${BLKSIZE} -N 1000 ${IMAGE} ${SIZE} ||
                                     (${SUDO} losetup -d /dev/loop${lonum} && exit 1)

# mount!
echo "=> Mounting ramdisk on $DIR"
${SUDO} mount -text2 /dev/loop${lonum} ${DIR} || (${SUDO} umount /dev/loop${lonum} && 
                                      ${SUDO} losetup -d /dev/loop${lonum} && exit 1)

#populate the file system
echo "=> Populating initrd"
cd ${DIR}

echo "==> Creating base dirs"
#sudo mkdir bin dev etc lib mnt proc root sbin tmp usr var ||
${SUDO} mkdir bin dev etc rootfs ||
                             (${SUDO} umount /dev/loop${lonum} && 
                                  ${SUDO} losetup -d /dev/loop${lonum} && exit 1)

# set up appropriate permissions for dirs
${SUDO} chmod 0755 bin dev etc
${SUDO} chmod 0700 rootfs

echo "==> Populating dev/"
${SUDO} mknod dev/tty0 c 4 0

echo "==> Populating etc/"
${SUDO} touch etc/fstab

# fix the disk name in fstab
sed  -i -e "s/hda1/${ROOT_DISK}/" -e "s/sda1/${ROOT_DISK}/" ${CWD}/../others/etc_files/fstab

echo "   Populating bin/"
${SUDO} cp ${SYSROOT}/usr/bin/bash bin/
${SUDO} cp ${SYSROOT}/usr/bin/cat bin/
${SUDO} cp ${SYSROOT}/usr/bin/echo bin/
${SUDO} cp ${SYSROOT}/usr/bin/ls bin/
${SUDO} cp ${SYSROOT}/usr/bin/init bin/
${SUDO} cp ${SYSROOT}/usr/sbin/reboot bin/
${SUDO} ln -s /bin/bash bin/sh

#echo "   Populating sbin/"
#echo
#echo "=== (Add commands here to copy whatever is needed!) ==="
#echo

echo
echo "   Populating usr/"
${SUDO} mkdir -p usr/lib
#sudo cp ${SYSROOT}/usr/lib/ld.so usr/lib/
${SUDO} cp ${SYSROOT}/usr/lib/libc.so usr/lib/
${SUDO} cp ${SYSROOT}/usr/lib/libgcc_s.so.1 usr/lib/
${SUDO} cp ${SYSROOT}/usr/lib/libtinfo* usr/lib/

# musl's ld.so is a symlink to its libc.so
${SUDO} ln -s /usr/lib/libc.so usr/lib/ld.so

cd ${CWD}

# unmount the initial RAM Disk (initrd) we mounted as a loopback device
echo "=> Unmounting ramdisk"
sleep 5
${SUDO} umount /dev/loop${lonum}
${SUDO} umount ${DIR}

[ $? -ne 0 ] && echo "*** Failed to unmount ramdisk dir!"

echo "=> Releasing the loopback device"
${SUDO} losetup -d /dev/loop${lonum}

[ $? -ne 0 ] && echo "*** Failed to release loop device!"

# create the gzip archive
echo "=> Gzipping initrd"
[ -e ${IMAGE}.gz ] && rm ${IMAGE}.gz
gzip -k -9 ${IMAGE}

echo "=> Created ${CWD}/${IMAGE}.gz"

${SUDO} rmdir ${DIR}

echo "Done!"

