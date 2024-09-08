#!/bin/bash

#
# Script to download and build libtiff
#

DOWNLOAD_NAME="libtiff"
DOWNLOAD_VERSION="4.6.0"
DOWNLOAD_URL="https://download.osgeo.org/libtiff/"
DOWNLOAD_PREFIX="tiff-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libtiff.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/config/config.sub ${DOWNLOAD_SRCDIR}/config/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config/config.sub
mv ${DOWNLOAD_SRCDIR}/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/m4/libtool.m4.OLD
cp ../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/m4/libtool.m4

cd ${DOWNLOAD_SRCDIR} && autoreconf

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

../configure CPPFLAGS="-D__laylaos__ -D__${ARCH}__" CFLAGS="-mstackrealign" --host=${BUILD_TARGET} --enable-shared=yes || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix libtiff.la for the future generations
sed -i "s/dependency_libs=.*/dependency_libs='-llzma -ljpeg -lz -lm'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libtiff.la

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

