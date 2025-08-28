#!/bin/bash

#
# Script to download and build twolame
#

DOWNLOAD_NAME="twolame"
DOWNLOAD_VERSION="0.4.0"
DOWNLOAD_URL="https://sourceforge.net/projects/twolame/files/twolame/${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="twolame-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

export CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign"

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libtwolame.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/build-scripts/config.sub ${DOWNLOAD_SRCDIR}/build-scripts/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/build-scripts/config.sub

mv ${DOWNLOAD_SRCDIR}/build-scripts/config.guess ${DOWNLOAD_SRCDIR}/build-scripts/config.guess.OLD
cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/build-scripts/config.guess

mv ${DOWNLOAD_SRCDIR}/build-scripts/libtool.m4 ${DOWNLOAD_SRCDIR}/build-scripts/libtool.m4.OLD
cp ../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/build-scripts/libtool.m4

cd ${DOWNLOAD_SRCDIR} && autoreconf

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

../configure --host=${BUILD_TARGET} --enable-shared || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

