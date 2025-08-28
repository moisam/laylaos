#!/bin/bash

#
# Script to download and build vorbis
#

DOWNLOAD_NAME="vorbis"
DOWNLOAD_VERSION="1.3.7"
DOWNLOAD_URL="https://ftp.osuosl.org/pub/xiph/releases/vorbis/"
DOWNLOAD_PREFIX="libvorbis-"
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
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libvorbis.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

#mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
#cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

#mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
#cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

#cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

#../configure --host=${BUILD_TARGET} --enable-shared \
#    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} --prefix=/usr \
#    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

cmake .. --toolchain ${CWD}/../prep_cross.cmake -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_INSTALL_PREFIX=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

#make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"
make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix libvorbisenc.la for the future generations
#sed -i "s/dependency_libs=.*/dependency_libs='-lvorbis -lm -logg'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libvorbisenc.la

# Fix libvorbisfile.la for the future generations
#sed -i "s/dependency_libs=.*/dependency_libs='-lvorbis -lm -logg'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libvorbisfile.la

# Fix libvorbis.la for the future generations
#sed -i "s/dependency_libs=.*/dependency_libs='-lm -logg'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libvorbis.la

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

