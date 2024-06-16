#!/bin/bash

#
# Script to download and build ffmpeg
#

DOWNLOAD_NAME="ffmpeg"
DOWNLOAD_VERSION="6.1.1"
DOWNLOAD_URL="https://ffmpeg.org/releases/"
DOWNLOAD_PREFIX="ffmpeg-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libavcodec.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

../configure --arch=${BUILD_ARCH} --target-os=${BUILD_TARGET} \
    --target-path=/usr/include --enable-cross-compile \
    --enable-shared --sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    --sysinclude=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    --cc=${CC} --cxx=${CXX} --nm=${NM} --ar=${AR} --as=${AS} \
    --strip=${STRIP} --ranlib=${RANLIB} --ld=${CC} \
    --pkg-config=${PKG_CONFIG} --enable-pic --disable-avx \
    --extra-cflags="-D__laylaos__ -D__${ARCH}__ -mstackrealign" \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

