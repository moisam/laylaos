#!/bin/bash

##
# Copyright 2021-2025 (c) Mohammed Isam
#
# This script is part of LaylaOS

# Script to create an ISO9660 CD-ROM image from LaylaOS build tree.
#
# Use as:
#   create_cdrom.sh [x86_64|i686] [sysroot PATH] [outdir PATH] [rootdev ROOTDEV]
#
# If you don't specify an architecture, x86_64 is selected by default.
# If you don't specify sysroot path, the directory ./build/sysroot/ is
#   used by default.
# If you don't specify outdir path, the current working directory is
#   used by default.
#
# Please DO NOT set ROOTDEV unless you know what you are doing. The main
# reason this option exists is to allow us to test AHCI driver code (in 
# which case a value like 'sda1' would be expected).
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

ROOTDEV=hda1
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
            echo "$0: create an ISO9660 CD-ROM image from LaylaOS build tree"
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
            echo "which case a value like 'sda1' would be expected)."
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

mkdir -p ${OUTDIR}/isodir
mkdir -p ${OUTDIR}/isodir/boot
mkdir -p ${OUTDIR}/isodir/boot/grub

echo "Copying kernel img"
cp "${SYSROOT}/boot/laylaos.kernel" ${OUTDIR}/isodir/boot/laylaos.kernel

echo "Copying kernel symbol table System.map"
cp "${SYSROOT}/boot/System.map" ${OUTDIR}/isodir/boot/System.map

echo "Copying boot modules"
cp "${SYSROOT}/boot/modules/"* ${OUTDIR}/isodir/boot/

echo "Making initrd img"
${CWD}/make_initrd.sh root ${ROOTDEV} sysroot ${SYSROOT}

echo "Copying initrd img"
mv initrd.img.gz ${OUTDIR}/isodir/boot/

echo "Copying vdso img"
cp ${CWD}/../vdso/vdso.so ${OUTDIR}/isodir/boot/

cat > ${OUTDIR}/isodir/boot/grub/grub.cfg << EOF
default=0
timeout=0
insmod all_video
menuentry "Layla OS" {
  multiboot2 /boot/laylaos.kernel root=/dev/${ROOTDEV}
  module2 --nounzip /boot/initrd.img.gz "INITRD"
  module2 /boot/System.map "SYSTEM.MAP"
  module2 /boot/acpica.o "ACPICA"
  module2 /boot/vdso.so "VDSO"
}
EOF

echo "Creating ISO image"
[ -e ${OUTDIR}/laylaos.iso ] && rm ${OUTDIR}/laylaos.iso
grub-mkrescue -o ${OUTDIR}/laylaos.iso ${OUTDIR}/isodir

echo "Removing temporary directory"
rm -rf ${OUTDIR}/isodir

