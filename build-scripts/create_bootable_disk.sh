#!/bin/bash

##
# Copyright 2021-2024 (c) Mohammed Isam
#
# This script is part of LaylaOS

# Script to create a bootable harddisk image from LaylaOS build tree.
#
# Use as:
#   create_bootable_disk.sh [x86_64|i686] [sysroot PATH] [outdir PATH]
#
# If you don't specify an architecture, x86_64 is selected by default.
# If you don't specify sysroot path, the directory ./build/sysroot/ is
#   used by default.
# If you don't specify outdir path, the current working directory is
#   used by default.
#

# build 64bit OS by default
ARCH=x86_64

ROOTDEV=hda4
CWD=`pwd`

# use ./build/sysroot as the cross sysroot by default
SYSROOT=${CWD}/build/sysroot

# use ./ as the output dir by default
OUTDIR=${CWD}

# now process arguments to see if the user wants to change anything
i=1
j=$#

while [ $i -le $j ]
do
    case $1 in
        help)
            echo "$0: create a bootable harddisk image from LaylaOS build tree"
            echo
            echo "Usage:"
            echo "  $0 [x86_64|i686] [sysroot PATH] [outdir PATH]"
            echo
            echo "If you don't specify an architecture, x86_64 is selected by default."
            echo "If you don't specify sysroot path, the directory ./build/sysroot/ is"
            echo "used by default."
            echo "If you don't specify outdir path, the current working directory is"
            echo "used by default."
            echo
            exit 0
            ;;
        x86-64 | x86_64)
            ARCH=x86_64
            ;;
        i686)
            ARCH=i686
            ;;
        sysroot)
            i=$((i + 1))
            shift 1
            SYSROOT="$1"
            ;;
        outdir)
            i=$((i + 1))
            shift 1
            OUTDIR="$1"
            ;;
        *)
            # anything else is trash
            echo "$0: unknown argument: $1"
            exit 1
            ;;
    esac
    
    i=$((i + 1))
    shift 1
done

echo
echo "  Build arch:    ${ARCH}"
echo "  Root device:   ${ROOTDEV}"
echo "  Sysroot:       ${SYSROOT}"
echo "  Output path:   ${OUTDIR}"
echo

##################################################
# get rid of these checks early on
##################################################

for d in others vdso; do
    if ! [ -d "${CWD}/../${d}" ]; then
        echo "$0: Cannot find source dir ${d}/"
        echo "    Are you sure you ran this script from the build-scripts/ dir"
        echo "    in the source tree?"
        exit 1
    fi
done

##################################################
# some definitions to use in the commands below
##################################################

# bytes per sector
BYTES_PER_SECTOR=512

# calculate the sector count

# first, find out how much we need for /etc stuff that is not under
# the sysroot, i.e. things we will copy from our source tree
ETC_SECTOR_COUNT=`du -B ${BYTES_PER_SECTOR} -s ${CWD}/../others | awk '{print \$1}'`

# we find it by getting the size of sysroot in 512-units then extracting
# the first column
SECTOR_COUNT=`du -B ${BYTES_PER_SECTOR} -s ${SYSROOT} | awk '{print \$1}'`

# we then need to add space for the EFI and GRUB system partitions
# (this magic number comes from gdisk - see the comments below!)
SECTOR_COUNT=$((${SECTOR_COUNT} + 200704))

# and add the space need for etc/ files, and throw an extra 100MiB for fun
# (the magic number is 100 MiB divided by 512)
SECTOR_COUNT=$((${SECTOR_COUNT} + ${ETC_SECTOR_COUNT} + 204800))

# this is the last data sector on disk
LAST_SECTOR=$((${SECTOR_COUNT} - 34))

#
# for more info, see:
#    https://wiki.osdev.org/GRUB
#    https://wiki.osdev.org/Bootable_Disk
#    https://medium.com/@ThyCrow/creating-hybrid-image-file-from-compiled-linux-kernel-and-initramfs-with-grub-c7599895b742
#    https://www.linuxfromscratch.org/blfs/view/11.2/postlfs/grub-setup.html
#

IMAGE="${OUTDIR}/bootable_disk.img"

# remove the old img
[ $IMAGE ] && rm -f $IMAGE

echo "  Sector size:   ${BYTES_PER_SECTOR}"
echo "  Sector count:  ${SECTOR_COUNT}"
echo "  Last sector:   ${LAST_SECTOR}"
echo "  Output path:   ${IMAGE}"
echo

# create an empty img with the given size
dd if=/dev/zero of=${IMAGE} bs=${BYTES_PER_SECTOR} count=${SECTOR_COUNT} || exit 1


# partition it
# we use the following gdisk commands (DO NOT REMOVE the empty lines!):
#   o - Create a new GPT partition table
#   y - yes
#   n - Create a new partition
#       \n - Select partition (use default, first partition)
#       \n - First sector to use (use default, 2048)
#       +1M - Choose 1M for BIOS partition
#       Change type to 'BIOS boot partition'
#   n - Create a new partition
#       \n - Select partition (use default, second partition)
#       \n - First sector to use (use default, 4096)
#       +48M - Choose 48M for EFI partition
#       Change type to 'EFI system partition'
#   n - Create a new partition
#       \n - Select partition (use default, third partition)
#       \n - First sector to use (use default, 102400)
#       +48M - Choose 48M for EFI partition
#       \n - Choose default type (Linux filesystem)
#   n - Create a new partition
#       \n - Select partition (use default, fourt partition)
#       \n - First sector to use (use default, 200704)
#       \n - Choose default size (for our data partition)
#       \n - Choose default type (Linux filesystem)
#   p - Print partition table
#   w - Write partition table to our 'disk' and exit.
gdisk $IMAGE <<EOF
o
y
n


+1M
ef02
n


+48M
ef00
n


+48M

n




p
w
y
EOF


# Create loopdevices:
#    1st lodev for the entire disk
#    2nd lodev for 2nd (EFI) partition
#    3nd lodev for 3rd (Ext2) partition

for lonum0 in $(seq 0 256)
do
    sudo losetup /dev/loop${lonum0} $IMAGE && break
done

for lonum1 in $(seq 0 256)
do
    sudo losetup --offset $((4096*512)) \
        --sizelimit $(((102399-4096+1)*512)) \
        /dev/loop${lonum1} ${IMAGE} >/dev/null 2>&1 \
            && break
done

for lonum2 in $(seq 0 256)
do
    sudo losetup --offset $((102400*512)) \
        --sizelimit $(((200703-102400+1)*512)) \
        /dev/loop${lonum2} ${IMAGE} >/dev/null 2>&1 \
            && break
done

for lonum3 in $(seq 0 256)
do
    sudo losetup --offset $((200704*512)) \
        --sizelimit $(((${LAST_SECTOR}-200704+1)*512)) \
        /dev/loop${lonum3} ${IMAGE} >/dev/null 2>&1 \
            && break
done


# Format partitions:
#    FAT32 for EFI partition
#    Ext2 for our data partition (with 1k blocks)

sudo mkdosfs -F 32 /dev/loop${lonum1}
sudo mkdosfs -F 32 /dev/loop${lonum2}
sudo mke2fs -b1024 /dev/loop${lonum3}


# Create temp dirs
mkdir tefi tgrub tdrive

sudo mount /dev/loop${lonum1} tefi
sudo mount /dev/loop${lonum2} tgrub
sudo mount /dev/loop${lonum3} tdrive

# Install GRUB EFI and compatibility i386 modules
sudo grub-install --target=x86_64-efi --efi-directory=tefi \
    --bootloader-id=GRUB \
    --modules="normal part_msdos part_gpt ext2 multiboot" \
    --root-directory=tgrub --boot-directory=tgrub/boot \
    --no-floppy /dev/loop${lonum0}

sudo grub-install --target=i386-pc \
    --modules="normal part_msdos part_gpt ext2 multiboot" \
    --root-directory=tgrub --boot-directory=tgrub/boot \
    --no-floppy /dev/loop${lonum0}


# create the disk
./copy_files_to_disk.sh tdrive ${SYSROOT}

sudo sed -i -e "s/hda1/${ROOTDEV}/" -e "s/sda1/${ROOTDEV}/" tdrive/etc/fstab

#sudo mkdir -p tdrive/boot/grub

echo "Copying kernel img"
sudo cp "${SYSROOT}/boot/laylaos.kernel" tgrub/boot/laylaos.kernel

echo "Copying kernel symbol table System.map"
sudo cp "${SYSROOT}/boot/System.map" tgrub/boot/System.map

echo "Copying boot modules"
sudo cp "${SYSROOT}/boot/modules/"* tgrub/boot/

echo "Making initrd img"
./make_initrd.sh root ${ROOTDEV}

echo "Copying initrd img"
sudo mv initrd.img.gz tgrub/boot/

echo "Copying vdso img"
sudo cp ${CWD}/../vdso/vdso.so tgrub/boot/

echo "default=0
timeout=0
menuentry \"Layla OS\" {
  multiboot /boot/laylaos.kernel root=/dev/${ROOTDEV}
  module --nounzip /boot/initrd.img.gz \"INITRD\"
  module /boot/System.map \"SYSTEM.MAP\"
  module /boot/acpica.o \"ACPICA\"
  module /boot/vdso.so \"VDSO\"
}" | sudo tee tgrub/boot/grub/grub.cfg > /dev/null


echo
echo "GRUB partition contents:"
ls tgrub

echo
echo "Main partition contents:"
ls tdrive

sleep 3

# unmount and free the loop device
echo
echo "Finishing"
echo "---------"
sudo umount /dev/loop${lonum1}
sudo umount /dev/loop${lonum2}
sudo umount /dev/loop${lonum3}
sudo losetup -d /dev/loop${lonum0}
sudo losetup -d /dev/loop${lonum1}
sudo losetup -d /dev/loop${lonum2}
sudo losetup -d /dev/loop${lonum3}

# remove tmp dirs
sudo sync
sudo rm -rf tefi
sudo rm -rf tgrub
sudo rm -rf tdrive

MBSIZE=$(( ${SECTOR_COUNT} * ${BYTES_PER_SECTOR} / 1048576 ))

echo
echo "  Disk image path:   ${IMAGE}"
echo "  Disk image size:   ${MBSIZE} MiB"
echo

echo "Creating bochsrc"
chmod +x ./create_bochsrc.sh
./create_bochsrc.sh ${IMAGE} ${OUTDIR}

echo "Creating qemu script"
if [ ${ARCH} == 'i686' ]; then
    QEMU_ARCH=i386
else
    QEMU_ARCH=x86_64
fi

cat << EOF > "${OUTDIR}/qemu.sh"
qemu-system-${QEMU_ARCH} -accel tcg,thread=single -cpu core2duo -m 512 -M pc -no-reboot -no-shutdown -drive format=raw,file=${IMAGE},index=0,media=disk -boot d -serial stdio -smp 1 -usb -vga std -net nic,model=ne2k_pci -net tap,ifname=tap0,script=no,downscript=no,id=net0 -device intel-hda,debug=4 -device hda-duplex -audiodev id=pa,driver=pa,server=/run/user/1000/pulse/native
EOF

chmod +x "${OUTDIR}/qemu.sh"

echo "Done!"

