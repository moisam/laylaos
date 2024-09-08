#!/bin/bash

#
# Script to download and build opusfile
#

DOWNLOAD_NAME="opusfile"
DOWNLOAD_VERSION="0.12"
DOWNLOAD_URL="https://github.com/xiph/opusfile/releases/download/v0.12/"
DOWNLOAD_PREFIX="opusfile-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libopusfile.so

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

autoreconf -if

mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub
mv ${DOWNLOAD_SRCDIR}/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/m4/libtool.m4.OLD
cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/m4/libtool.m4

autoreconf

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

../configure CFLAGS="-mstackrealign" \
    --host=${BUILD_TARGET} \
    --disable-static \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix opusfile.pc for the future generations
sed -i "s/Version.*/Version: ${DOWNLOAD_VERSION}/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/pkgconfig/opusfile.pc

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

