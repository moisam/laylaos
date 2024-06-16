#!/bin/bash

#
# Script to download and build MPlayer
#

DOWNLOAD_NAME="mplayer"
DOWNLOAD_VERSION="1.5"
DOWNLOAD_URL="http://www.mplayerhq.hu/MPlayer/releases/"
DOWNLOAD_PREFIX="MPlayer-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/mplayer

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cp extra/ao_laylaos.c ${DOWNLOAD_SRCDIR}/libao2/
cp extra/vo_laylaos.c ${DOWNLOAD_SRCDIR}/libvo/

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
cd ${DOWNLOAD_SRCDIR}

CFLAGS= CPPFLAGS= ./configure \
    --enable-laylaos --enable-menu --enable-runtime-cpudetection \
    --enable-dvdread --disable-sunaudio \
    --target=${BUILD_TARGET} \
    --with-freetype-config="$PKGCONFIG freetype2" \
    --extra-cflags="-D__laylaos__ -D__${BUILD_ARCH}__" \
    --extra-libs="-ldvdread" \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

