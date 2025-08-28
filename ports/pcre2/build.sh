#!/bin/bash

#
# Script to download and build pcre2
#

DOWNLOAD_NAME="pcre2"
DOWNLOAD_VERSION="10.44"
DOWNLOAD_URL="https://github.com/PCRE2Project/pcre2/releases/download/pcre2-${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="pcre2-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libpcre2-8.so

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
    -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release \
    -DPCRE2_BUILD_PCRE2_8=ON -DPCRE2_BUILD_PCRE2_16=ON -DPCRE2_BUILD_PCRE2_32=ON \
    -DPCRE2_STATIC_PIC=ON \
    -DZLIB_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DBZIP2_LIBRARIES=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libbz2.so \
    -DBZIP2_INCLUDE_DIR=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    -DCMAKE_POLICY_DEFAULT_CMP0074=NEW \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

