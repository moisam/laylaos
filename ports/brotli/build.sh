#!/bin/bash

#
# Script to download and build brotli
#

DOWNLOAD_NAME="brotli"
DOWNLOAD_VERSION="1.2.0"
DOWNLOAD_URL="https://github.com/google/brotli/archive/refs/tags/"
DOWNLOAD_PREFIX="v"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/brotli-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/brotli

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

cmake .. \
    --toolchain ${CWD}/../prep_cross.cmake \
    -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_MANDIR=${CROSSCOMPILE_SYSROOT_PATH}/usr/share/man \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# set the SONAME so everybody else can link to it without using the full pathname
patchelf --set-soname libbrotlidec.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libbrotlidec.so
patchelf --set-soname libbrotlienc.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libbrotlienc.so
patchelf --set-soname libbrotlicommon.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libbrotlicommon.so

# cleanup
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

