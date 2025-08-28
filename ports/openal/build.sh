#!/bin/bash

#
# Script to download and build OpenAL
#

DOWNLOAD_NAME="openal"
DOWNLOAD_VERSION="1.23.1"
DOWNLOAD_URL="https://github.com/kcat/openal-soft/archive/refs/tags/"
DOWNLOAD_PREFIX=""
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/openal-soft-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libopenal.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# build
cd ${DOWNLOAD_SRCDIR}

mkdir build
cd build

cmake .. --toolchain ${CWD}/../prep_cross.cmake -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON -DALSOFT_BACKEND_SDL2=ON -DALSOFT_REQUIRE_SDL2=ON \
    -DALSOFT_BACKEND_SOLARIS=OFF \
    -DAVFORMAT_LIBRARIES=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libavformat.so \
    -DAVFORMAT_INCLUDE_DIRS=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    -DAVCODEC_LIBRARIES=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libavcodec.so \
    -DAVCODEC_INCLUDE_DIRS=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    -DAVUTIL_LIBRARIES=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libavutil.so \
    -DAVUTIL_INCLUDE_DIRS=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    -DSWSCALE_LIBRARIES=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libswscale.so \
    -DSWSCALE_INCLUDE_DIRS=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    -DSWRESAMPLE_LIBRARIES=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libswresample.so \
    -DSWRESAMPLE_INCLUDE_DIRS=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# set the SONAME so everybody else can link to it without using the full pathname
patchelf --set-soname libopenal.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libopenal.so

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

