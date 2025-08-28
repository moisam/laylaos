#!/bin/bash

##
# Copyright 2021-2025 (c) Mohammed Isam
#
# This script is part of LaylaOS

# Script to create a bootable harddisk image from LaylaOS build tree.
#
# Use as:
#   create_bootable_disk.sh [x86_64|i686] [sysroot PATH] [outdir PATH] [rootdev ROOTDEV]
#
# If you don't specify an architecture, x86_64 is selected by default.
# If you don't specify sysroot path, the directory ./build/sysroot/ is
#   used by default.
# If you don't specify outdir path, the current working directory is
#   used by default.
#
# Please DO NOT set ROOTDEV unless you know what you are doing. The main
# reason this option exists is to allow us to test AHCI driver code (in 
# which case a value like 'sda4' would be expected).
#

# sudo is not available on LaylaOS yet
myname=`uname -s`
if [ "$myname" == "LaylaOS" ]; then
    SUDO=
else
    SUDO=sudo
fi

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
            echo "  $0 [x86_64|i686] [sysroot PATH] [outdir PATH] [rootdev ROOTDEV]"
            echo
            echo "If you don't specify an architecture, x86_64 is selected by default."
            echo "If you don't specify sysroot path, the directory ./build/sysroot/ is"
            echo "used by default."
            echo "If you don't specify outdir path, the current working directory is"
            echo "used by default."
            echo
            echo "Please DO NOT set ROOTDEV unless you know what you are doing. The main"
            echo "reason this option exists is to allow us to test AHCI driver code (in "
            echo "which case a value like 'sda4' would be expected)."
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
        rootdev)
            i=$((i + 1))
            shift 1
            ROOTDEV="$1"
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

# and add the space need for etc/ files, and throw an extra 4 GiB to
# account for waste space, e.g. directories can be 1-4 KiB in size
# (the magic number is 4 GiB divided by 512)
SECTOR_COUNT=$((${SECTOR_COUNT} + ${ETC_SECTOR_COUNT} + 8388608))

# account for the reserved sectors at the disk's end, which will take out
# some of our calculated sector count
SECTOR_COUNT=$((${SECTOR_COUNT} + 34))

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
echo "  Zeroing out disk image"

# create an empty img with the given size
dd if=/dev/zero of=${IMAGE} bs=${BYTES_PER_SECTOR} count=${SECTOR_COUNT} || exit 1

echo "  Partitioning disk image: ${IMAGE}"

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
gdisk "$IMAGE" <<TILLEOF
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
TILLEOF

[ $? -ne 0 ] && echo "Failed to partition disk" && exit 1


# Create loopdevices:
#    1st lodev for the entire disk
#    2nd lodev for 2nd (EFI) partition
#    3nd lodev for 3rd (Ext2) partition

echo "  Finding loop device (1/4)"
for lonum0 in $(seq 0 256)
do
    ${SUDO} losetup /dev/loop${lonum0} $IMAGE && break
done

echo "  Finding loop device (2/4)"
for lonum1 in $(seq 0 256)
do
    ${SUDO} losetup --offset $((4096*512)) \
        --sizelimit $(((102399-4096+1)*512)) \
        /dev/loop${lonum1} ${IMAGE} >/dev/null 2>&1 \
            && break
done

echo "  Finding loop device (3/4)"
for lonum2 in $(seq 0 256)
do
    ${SUDO} losetup --offset $((102400*512)) \
        --sizelimit $(((200703-102400+1)*512)) \
        /dev/loop${lonum2} ${IMAGE} >/dev/null 2>&1 \
            && break
done

echo "  Finding loop device (4/4)"
for lonum3 in $(seq 0 256)
do
    ${SUDO} losetup --offset $((200704*512)) \
        --sizelimit $(((${LAST_SECTOR}-200704+1)*512)) \
        /dev/loop${lonum3} ${IMAGE} >/dev/null 2>&1 \
            && break
done

echo "  Using: /dev/loop${lonum0} for boot"
echo "         /dev/loop${lonum1} for EFI"
echo "         /dev/loop${lonum2} for GRUB"
echo "         /dev/loop${lonum3} for data"

# Format partitions:
#    FAT32 for EFI partition
#    Ext2 for our data partition (with 1k blocks)

${SUDO} mkdosfs -F 32 /dev/loop${lonum1}
[ $? -ne 0 ] && echo "Failed to create FAT32 partition (1/2)" && exit 1

${SUDO} mkdosfs -F 32 /dev/loop${lonum2}
[ $? -ne 0 ] && echo "Failed to create FAT32 partition (2/2)" && exit 1

${SUDO} mke2fs -b1024 /dev/loop${lonum3}
[ $? -ne 0 ] && echo "Failed to create Ext2 partition" && exit 1


# Create temp dirs
mkdir tefi tgrub tdrive

${SUDO} mount /dev/loop${lonum1} tefi
[ $? -ne 0 ] && echo "Failed to mount EFI partition" && exit 1

${SUDO} mount /dev/loop${lonum2} tgrub
[ $? -ne 0 ] && echo "Failed to mount GRUB partition" && exit 1

${SUDO} mount /dev/loop${lonum3} tdrive
[ $? -ne 0 ] && echo "Failed to mount main disk partition" && exit 1

# Install GRUB EFI and compatibility i386 modules
${SUDO} grub-install --target=x86_64-efi --recheck --removable \
    --uefi-secure-boot --efi-directory=tefi --boot-directory=tefi/boot \
    --modules="normal part_msdos part_gpt ext2 multiboot"
#${SUDO} grub-install --target=x86_64-efi --efi-directory=tefi \
#    --bootloader-id=GRUB \
#    --modules="normal part_msdos part_gpt ext2 multiboot" \
#    --root-directory=tgrub --boot-directory=tgrub/boot \
#    --no-floppy /dev/loop${lonum0}
[ $? -ne 0 ] && echo "Failed to install EFI module" && exit 1

${SUDO} grub-install --target=i386-pc --recheck \
    --modules="normal part_msdos part_gpt ext2 multiboot" \
    --boot-directory=tgrub/boot /dev/loop${lonum0}
#${SUDO} grub-install --target=i386-pc \
#    --modules="normal part_msdos part_gpt ext2 multiboot" \
#    --root-directory=tgrub --boot-directory=tgrub/boot \
#    --no-floppy /dev/loop${lonum0}
[ $? -ne 0 ] && echo "Failed to install GRUB module" && exit 1

# Remove problematic files
sudo rm tefi/EFI/BOOT/BOOTX64.CSV
sudo rm tefi/EFI/BOOT/fbx64.efi
sudo rm tefi/EFI/BOOT/grub.cfg


echo "Copying main disk contents"

# create the disk
./copy_files_to_disk.sh tdrive ${SYSROOT}

# fix the root dev in fstab
${SUDO} sed -i -e "s/hda./${ROOTDEV}/" -e "s/sda./${ROOTDEV}/" tdrive/etc/fstab

# comment out the cdrom mount line in fstab
${SUDO} sed -i -e "s~/dev/cdrom~#/dev/cdrom~" tdrive/etc/fstab

#sudo mkdir -p tdrive/boot/grub

echo "Copying kernel img"
${SUDO} cp "${SYSROOT}/boot/laylaos.kernel" tgrub/boot/laylaos.kernel

echo "Copying kernel symbol table System.map"
${SUDO} cp "${SYSROOT}/boot/System.map" tgrub/boot/System.map

echo "Copying boot modules"
${SUDO} cp "${SYSROOT}/boot/modules/"* tgrub/boot/

echo "Making initrd img"
./make_initrd.sh root ${ROOTDEV} sysroot ${SYSROOT}

echo "Copying initrd img"
${SUDO} mv initrd.img.gz tgrub/boot/

echo "Copying vdso img"
${SUDO} cp ${CWD}/../vdso/vdso.so tgrub/boot/


# Create sample configuration files
echo "search --set=root --file /grubhybrid.cfg
configfile /grubhybrid.cfg" | ${SUDO} tee tefi/boot/grub/grub.cfg > /dev/null

${SUDO} cp tefi/boot/grub/grub.cfg tgrub/boot/grub/grub.cfg

echo "set menu_color_normal=black/cyan
set menu_color_highlight=light-cyan/cyan

insmod part_msdos
insmod part_gpt
insmod fat
insmod ext2
#loadfont /boot/grub/fonts/unicode.pf2

insmod font
if loadfont unicode ; then
    if keystatus --shift ; then true ; else
        if [ \${grub_platform} == \"efi\" ]; then
            insmod efi_gop
            insmod efi_uga
        else
            insmod vbe
            insmod vga
        fi
        insmod gfxterm
        set gfxmode=1024x768
        set gfxpayload=auto
        terminal_output gfxterm
        if terminal_output gfxterm ; then true ; else
            terminal gfxterm
        fi
        insmod gfxmenu
    fi
fi

menuentry \"Layla OS\" {
  multiboot2 /boot/laylaos.kernel root=/dev/${ROOTDEV}
  module2 --nounzip /boot/initrd.img.gz \"INITRD\"
  module2 /boot/System.map \"SYSTEM.MAP\"
  module2 /boot/acpica.o \"ACPICA\"
  module2 /boot/vdso.so \"VDSO\"
}

menuentry \"Reboot\" { reboot }
menuentry \"Poweroff\" { halt }
if [ \${grub_platform} == \"efi\" ]; then
    menuentry \"UEFI setup\" { fwsetup  }
fi" | ${SUDO} tee tgrub/grubhybrid.cfg > /dev/null

#echo "default=0
#timeout=0
#menuentry \"Layla OS\" {
#  multiboot /boot/laylaos.kernel root=/dev/${ROOTDEV}
#  module --nounzip /boot/initrd.img.gz \"INITRD\"
#  module /boot/System.map \"SYSTEM.MAP\"
#  module /boot/acpica.o \"ACPICA\"
#  module /boot/vdso.so \"VDSO\"
#}" | ${SUDO} tee tgrub/boot/grub/grub.cfg > /dev/null


echo
echo "EFI partition contents:"
ls -l tefi

echo
echo "GRUB partition contents:"
ls -l tgrub

echo
echo "Main partition contents:"
ls -l tdrive

echo
df -h

sleep 3

# unmount and free the loop device
echo
echo "Finishing"
echo "---------"
${SUDO} umount /dev/loop${lonum1}
${SUDO} umount /dev/loop${lonum2}
${SUDO} umount /dev/loop${lonum3}
${SUDO} losetup -d /dev/loop${lonum0}
${SUDO} losetup -d /dev/loop${lonum1}
${SUDO} losetup -d /dev/loop${lonum2}
${SUDO} losetup -d /dev/loop${lonum3}

# remove tmp dirs
${SUDO} sync
${SUDO} rm -rf tefi
${SUDO} rm -rf tgrub
${SUDO} rm -rf tdrive

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
qemu-system-${QEMU_ARCH} -accel tcg,thread=single -cpu core2duo -m 2048 -M pc -no-reboot -no-shutdown -drive format=raw,file=${IMAGE},index=0,media=disk -boot d -serial stdio -smp 1 -usb -vga std -net nic,model=ne2k_pci -net tap,ifname=tap0,script=no,downscript=no,id=net0 -device intel-hda,debug=4 -device hda-duplex -audiodev id=pa,driver=pa,server=/run/user/1000/pulse/native
EOF

chmod +x "${OUTDIR}/qemu.sh"

echo "Done!"

