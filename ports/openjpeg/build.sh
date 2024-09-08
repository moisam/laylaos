#!/bin/bash

#
# Script to download and build openjpeg
#

DOWNLOAD_NAME="openjpeg"
DOWNLOAD_VERSION="2.5.2"
DOWNLOAD_URL="https://github.com/uclouvain/openjpeg/archive/refs/tags/"
DOWNLOAD_PREFIX="v"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_NAME}-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libopenjp2.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

cmake .. \
    --toolchain ${CWD}/../prep_cross.cmake \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DZLIB_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DPNG_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DJPEG_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DTIFF_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DCMAKE_POLICY_DEFAULT_CMP0074=NEW \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

