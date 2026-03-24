#!/bin/bash

#
# Script to download and build bochs
#

DOWNLOAD_NAME="bochs"
DOWNLOAD_VERSION="3.0"
DOWNLOAD_URL="https://sourceforge.net/projects/bochs/files/bochs/${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="bochs-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/bochs

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

# build SDL2
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

CFLAGS="-mstackrealign" CXXFLAGS="-mstackrealign" \
    CPPFLAGS="-D_GNU_SOURCE -D__laylaos__ -D__${BUILD_ARCH}__ -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/ncurses" LDFLAGS="-ltinfo" NCURSES_PATH='-lncurses' \
    ../configure \
    --host=${BUILD_TARGET} --enable-smp --enable-cpu-level=6 \
    --enable-all-optimizations --enable-x86-64 \
    --enable-pci --enable-vmx --enable-logging --enable-fpu \
    --disable-sb16 --disable-es1370 \
    --enable-cdrom --enable-x86-debugger --enable-iodebug \
    --disable-plugins --disable-docbook \
    --with-term --with-sdl2 \
    --enable-usb --enable-usb-ohci --enable-usb-ehci --enable-usb-xhci \
    --enable-ne2000 --enable-e1000 --disable-readline \
    ac_cv_header_netpacket_packet_h=no \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

