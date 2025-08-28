#!/bin/bash

#
# Script to download and build libtheora
#

DOWNLOAD_NAME="libtheora"
DOWNLOAD_VERSION="1.2.0alpha1"
DOWNLOAD_URL="http://downloads.xiph.org/releases/theora/"
DOWNLOAD_PREFIX="libtheora-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

export CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign"

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libtheora.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build libtheora
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

PNG_CFLAGS="`${PKG_CONFIG} --cflags libpng`" \
    PNG_LIBS="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libpng.so" \
    ../configure --host=${BUILD_TARGET} \
    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    --prefix=/usr --enable-shared \
    --with-vorbis-libraries=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib \
    --with-vorbis-includes=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    --with-ogg-libraries=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib \
    --with-ogg-includes=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix libtheora*.la for the future generations
sed -i "s/dependency_libs=.*/dependency_libs='-logg'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libtheora.la
sed -i "s/dependency_libs=.*/dependency_libs='-logg -ltheoradec'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libtheoraenc.la

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

