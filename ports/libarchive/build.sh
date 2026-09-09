#!/bin/bash

#
# Script to download and build libarchive
#

DOWNLOAD_NAME="libarchive"
DOWNLOAD_VERSION="3.8.9"
DOWNLOAD_URL="https://github.com/libarchive/libarchive/releases/download/v${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="libarchive-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libarchive.so

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

rm ${DOWNLOAD_SRCDIR}/config.sub
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/build/autoconf/config.sub

rm ${DOWNLOAD_SRCDIR}/config.guess
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/build/autoconf/config.guess

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

CFLAGS="${CFLAGS} -mstackrealign" CXXFLAGS="${CXXFLAGS} -mstackrealign" \
    ../configure \
    --host=${BUILD_TARGET} --prefix=/usr \
    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} --disable-rpath \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

# for some reason, --disable-rpath does not disable rpath!
patchelf --set-rpath "" .libs/libarchive.so

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

