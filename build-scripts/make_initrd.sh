#
# Copyright 2021-2024 (c) Mohammed Isam
#
# This script is part of LaylaOS
#
# It will create the initial RAM Disk (initrd) and populate it with
# essential directories and files, needed by the kernel to run.
#

print_usage()
{
    echo "Usage:"
    echo "  $0 [root {hda[1..8] | sda[1..8]}]"
}

# process arguments
i=1
j=$#
ROOT_DISK=hda1
CWD=`pwd`

while [ $i -le $j ]
do
    case $1 in
        root)
            i=$((i + 1))
            shift 1
            ROOT_DISK=$1
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
SIZE=4096              # 4 MiB
BLKSIZE=1024
MAXSIZE=$(( 16 * 1024 * 1024 ))		# max size we support is 16 MiB
DIR=./initrd_tmp
IMAGE=./initrd.img

# sanity check
oursize=$(( $SIZE * $BLKSIZE ))

#echo "sz $oursize"
#echo "mx $MAXSIZE"

if [ $oursize -gt $MAXSIZE ]; then
  echo "*** Invalid initrd size: $oursize bytes"
  echo "*** Maximum size supported is: $MAXSIZE bytes"
  exit 1
fi

# remove old rdisk, if any
if [ -e $IMAGE ]; then
   echo "=> Removing old initrd.img"
   rm $IMAGE
fi

if [ -e ${IMAGE}.gz ]; then
   echo "=> Removing old initrd.img.gz"
   rm ${IMAGE}.gz
fi

# create empty ramdisk file
echo "=> Creating ${SIZE}KiB initrd file"
dd if=/dev/zero of=$IMAGE bs=$BLKSIZE count=$SIZE

# create mount point
if [ ! -d $DIR ]; then
   echo "=> Creating mount point: $DIR"
   mkdir $DIR
fi

#set ramdisk file for mounting
echo "=> Setting up ramdisk for mounting"

for lonum in $(seq 0 256)
do
    sudo losetup /dev/loop${lonum} $IMAGE >/dev/null 2>&1 && break
done

# make ext2 file system
# use -m 0 to prevent reserving space for root
echo "=> Making Ext2 filesystem"
mke2fs -F -m 0 -b $BLKSIZE -N 1000 $IMAGE $SIZE ||
                                     (sudo losetup -d /dev/loop${lonum} && exit 1)

# mount!
echo "=> Mounting ramdisk on $DIR"
sudo mount -text2 /dev/loop${lonum} $DIR || (sudo umount /dev/loop${lonum} && 
                                      sudo losetup -d /dev/loop${lonum} && exit 1)

#populate the file system
echo "=> Populating initrd"
cd $DIR

echo "==> Creating base dirs"
#sudo mkdir bin dev etc lib mnt proc root sbin tmp usr var ||
sudo mkdir bin dev etc rootfs ||
                             (sudo umount /dev/loop${lonum} && 
                                  sudo losetup -d /dev/loop${lonum} && exit 1)

# set up appropriate permissions for dirs
sudo chmod 0755 bin dev etc
sudo chmod 0700 rootfs

echo "==> Populating dev/"
sudo mknod dev/tty0 c 4 0

# fix the disk name in fstab
sed  -i -e "s/hda1/$ROOT_DISK/" -e "s/sda1/$ROOT_DISK/" ${CWD}/../others/etc_files/fstab

echo "   Populating bin/"
sudo cp $SYSROOT/usr/bin/bash bin/
sudo cp $SYSROOT/usr/bin/cat bin/
sudo cp $SYSROOT/usr/bin/ls bin/
sudo ln -s './bash' bin/init

#echo "   Populating sbin/"
#echo
#echo "=== (Add commands here to copy whatever is needed!) ==="
#echo

#echo "   Populating usr/"
#echo
#echo "=== (Add commands here to copy whatever is needed!) ==="
#echo

cd $CWD

# unmount the initial RAM Disk (initrd) we mounted as a loopback device
echo "=> Unmounting ramdisk"
sleep 5
sudo umount /dev/loop${lonum}
sudo umount $DIR

[ $? -ne 0 ] && echo "*** Failed to unmount ramdisk dir!"

echo "=> Releasing the loopback device"
sudo losetup -d /dev/loop${lonum}

[ $? -ne 0 ] && echo "*** Failed to release loop device!"

# create the gzip archive
echo "=> Gzipping initrd"
[ -e $IMAGE.gz ] && rm $IMAGE.gz
gzip -k -9 $IMAGE

echo "=> Created ${CWD}/${IMAGE}.gz"

sudo rmdir $DIR

echo "Done!"

