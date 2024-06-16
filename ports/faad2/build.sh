#!/bin/bash

#
# Script to download and build faad2
#

DOWNLOAD_NAME="faad2"
DOWNLOAD_VERSION="2.11.1"
DOWNLOAD_URL="https://github.com/knik0/faad2/archive/refs/tags/"
DOWNLOAD_PREFIX=""
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

export CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign"

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/faad2-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfaad.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

cmake .. --toolchain ${CWD}/../prep_cross.cmake \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

