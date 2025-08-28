#!/bin/bash

#
# Script to download and build libuuid (part of util-linux)
#

DOWNLOAD_NAME="util-linux"
DOWNLOAD_VERSION="2.41"
DOWNLOAD_URL="https://www.kernel.org/pub/linux/utils/util-linux/v${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="util-linux-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libuuid.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/config/config.sub ${DOWNLOAD_SRCDIR}/config/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config/config.sub

mv ${DOWNLOAD_SRCDIR}/config/config.guess ${DOWNLOAD_SRCDIR}/config/config.guess.OLD
cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config/config.guess

mv ${DOWNLOAD_SRCDIR}/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/m4/libtool.m4.OLD
cp ../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/m4/libtool.m4

cd ${DOWNLOAD_SRCDIR} && autoreconf

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

../configure \
    --host=${BUILD_TARGET} \
    --disable-all-programs --disable-nls --disable-static \
    --enable-libuuid --enable-shared --prefix=/usr --libdir=/usr/lib \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

