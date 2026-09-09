#!/bin/bash

#
# Script to download and build parted
#

DOWNLOAD_NAME="parted"
DOWNLOAD_VERSION="3.7"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/parted/"
DOWNLOAD_PREFIX="parted-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libparted.so

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

mv ${DOWNLOAD_SRCDIR}/build-aux/config.sub ${DOWNLOAD_SRCDIR}/build-aux/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.sub

mv ${DOWNLOAD_SRCDIR}/build-aux/config.guess ${DOWNLOAD_SRCDIR}/build-aux/config.guess.OLD
cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.guess

# build SDL2
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

ln -s linux.c ../libparted/arch/laylaos.c

../configure \
    --host=${BUILD_TARGET} --prefix=/usr \
    --with-readline --disable-nls --disable-device-mapper \
    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

