#!/bin/bash

#
# Script to download and build libpng16
#

DOWNLOAD_NAME="libpng16"
DOWNLOAD_VERSION="1.6.34"
DOWNLOAD_URL="http://ftp-osl.osuosl.org/pub/libpng/src/libpng16/"
DOWNLOAD_PREFIX="libpng-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

export CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign"

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libpng16.so

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
#cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

#mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
#cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

#mv ${DOWNLOAD_SRCDIR}/scripts/libtool.m4 ${DOWNLOAD_SRCDIR}/scripts/libtool.m4.OLD
#cp ../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/scripts/libtool.m4

#cd ${DOWNLOAD_SRCDIR} && autoreconf

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

#../configure --host=${BUILD_TARGET} --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

cmake .. --toolchain ${CWD}/../prep_cross.cmake \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
    -DZLIB_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DCMAKE_POLICY_DEFAULT_CMP0074=NEW \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

#make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"
make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cp ${CWD}/*.pc ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/pkgconfig/
cp ${CWD}/*.la ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/
ln -s libpng16.pc ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/pkgconfig/libpng.pc

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

