#!/bin/bash

#
# Script to download and build fribidi
#

DOWNLOAD_NAME="fribidi"
DOWNLOAD_VERSION="1.0.13"
DOWNLOAD_URL="https://github.com/fribidi/fribidi/releases/download/v${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="fribidi-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

export CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign"

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfribidi.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_SRCDIR}
NOCONFIGURE=1 ./autogen.sh

mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub
mv ${DOWNLOAD_SRCDIR}/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/m4/libtool.m4.OLD
cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/m4/libtool.m4
autoreconf

mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

../configure --host=${BUILD_TARGET} --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} --enable-shared || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

