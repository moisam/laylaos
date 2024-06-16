#!/bin/bash

#
# Script to download and build libwebp
#

DOWNLOAD_NAME="libwebp"
DOWNLOAD_VERSION="1.3.2"
CWD=`pwd`

export CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign"

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/libwebp"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libwebp.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths

# this is one of them Google packages you just have to clone from the repo
cd ${DOWNLOAD_PORTS_PATH}
git clone https://chromium.googlesource.com/webm/libwebp

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

cmake -DWEBP_BUILD_CWEBP=ON -DWEBP_BUILD_DWEBP=ON .. \
    --toolchain ${CWD}/../prep_cross.cmake \
    -DBUILD_SHARED_LIBS=ON \
    -DZLIB_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DPNG_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DJPEG_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DTIFF_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DCMAKE_POLICY_DEFAULT_CMP0074=NEW \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

# Search & Replace any '-pthread' to nothing in all the files (could not
# find a better way to do it!!):
grep -rl '\-pthread' . | xargs sed  -i -e "s/-pthread//"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

