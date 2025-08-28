#!/bin/bash

#
# Script to download and build grub
#

DOWNLOAD_NAME="grub"
DOWNLOAD_VERSION="2.12"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/grub/"
DOWNLOAD_PREFIX="grub-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/grub-mkimage

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

rm ${DOWNLOAD_SRCDIR}/build-aux/config.sub
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.sub

rm ${DOWNLOAD_SRCDIR}/build-aux/config.guess
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.guess

echo "depends bli part_gpt" > ${DOWNLOAD_SRCDIR}/grub-core/extra_deps.lst

mkdir -p ${DOWNLOAD_SRCDIR}/grub-core/osdep/laylaos
cp ${CWD}/extra/getroot.c ${CWD}/extra/hostdisk.c ${DOWNLOAD_SRCDIR}/grub-core/osdep/laylaos/

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0

myname=`uname -s`

if [ "$myname" == "LaylaOS" ]; then
    BUILD_OS_FOR_GRUB="${BUILD_ARCH}-pc-laylaos"
else
    BUILD_OS_FOR_GRUB="${BUILD_ARCH}-pc-linux-gnu"
fi

# build

#
# first, create all the tools, in addition to the EFI module
#
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

../configure \
    --host=${BUILD_ARCH}-pc-laylaos --target=${BUILD_ARCH}-pc-laylaos \
    --build=${BUILD_OS_FOR_GRUB} \
    --with-platform=efi \
    --prefix=/usr --disable-nls --disable-threads \
    --disable-device-mapper \
    --enable-grub-mkfont=yes enable_build_grub_mkfont=yes \
    --with-unifont=${CROSSCOMPILE_SYSROOT_PATH}/usr/share/fonts/unifont-16.0.04.pcf.gz \
    BUILD_FREETYPE_CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/freetype2" \
    --enable-grub-emu-sdl2=yes enable_grub_emu_sdl2=yes \
    BUILD_FREETYPE_LIBS="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfreetype.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libglib-2.0.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libiconv.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libintl.so" \
    LDFLAGS="-L${CROSSCOMPILE_SYSROOT_PATH}/usr/lib" \
    CFLAGS="-D__laylaos__ -mstackrealign" CPPFLAGS= \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

#
# second, rebuild but only install the i386 module
#
mkdir ${DOWNLOAD_SRCDIR}/build3
cd ${DOWNLOAD_SRCDIR}/build3

../configure \
    --host=${BUILD_ARCH}-pc-laylaos --target=${BUILD_ARCH}-pc-laylaos \
    --build=${BUILD_OS_FOR_GRUB} \
    --prefix=/usr --disable-nls --disable-threads \
    --disable-device-mapper \
    --enable-grub-mkfont=yes enable_build_grub_mkfont=yes \
    --with-unifont=${CROSSCOMPILE_SYSROOT_PATH}/usr/share/fonts/unifont-16.0.04.pcf.gz \
    BUILD_FREETYPE_CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/freetype2" \
    --enable-grub-emu-sdl2=yes enable_grub_emu_sdl2=yes \
    BUILD_FREETYPE_LIBS="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfreetype.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libglib-2.0.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libiconv.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libintl.so" \
    LDFLAGS="-L${CROSSCOMPILE_SYSROOT_PATH}/usr/lib" \
    CFLAGS="-D__laylaos__ -mstackrealign" CPPFLAGS= \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME} i386 module"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME} i386 module"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} -C grub-core install \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME} i386 module"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

