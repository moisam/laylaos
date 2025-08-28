#!/bin/bash

#
# Script to download and build flac
#

DOWNLOAD_NAME="flac"
DOWNLOAD_VERSION="1.4.3"
DOWNLOAD_URL="https://github.com/xiph/flac/releases/download/${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="flac-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libFLAC.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
#echo " ==> Patching ${DOWNLOAD_NAME}"
#echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

#mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
#cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

#mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
#cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

#mv ${DOWNLOAD_SRCDIR}/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/m4/libtool.m4.OLD
#cp ../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/m4/libtool.m4

#cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

#../configure --host=${BUILD_TARGET} --enable-shared || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

cmake .. --toolchain ${CWD}/../prep_cross.cmake \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

#make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"
make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cp ${CWD}/*.pc ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/pkgconfig/
cp ${CWD}/*.la ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/

# Fix libFLAC++.la for the future generations
#sed -i "s/dependency_libs=.*/dependency_libs='-lFLAC -logg -lstdc++'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libFLAC++.la

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

