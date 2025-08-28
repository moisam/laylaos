#!/bin/bash

#
# Script to download and build libcdio
#

DOWNLOAD_NAME="libcdio"
DOWNLOAD_VERSION="2.1.0"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/libcdio/"
DOWNLOAD_PREFIX="libcdio-"
DOWNLOAD_SUFFIX=".tar.bz2"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libcdio.so

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

cp ./extra/laylaos.c ${DOWNLOAD_SRCDIR}/lib/driver/

mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

mv ${DOWNLOAD_SRCDIR}/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/m4/libtool.m4.OLD
cp ../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/m4/libtool.m4

cd ${DOWNLOAD_SRCDIR} && autoreconf

# build SDL2
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

LDFLAGS="-L${CROSSCOMPILE_SYSROOT_PATH}/usr/lib -ltinfo" \
    CPPFLAGS="-D__laylaos__ -D__x86_64__" \
    CFLAGS="$CFLAGS -mstackrealign" \
    CPPFLAGS="$CPPFLAGS -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/ncurses" \
    CXXFLAGS="$CXXFLAGS -I${CXX_INCLUDE_PATH}" \
    ../configure \
    --host=${BUILD_TARGET} --prefix=/usr --enable-maintainer-mode --with-pic=yes \
    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix libcdio.la for the future generations
sed -i "s/dependency_libs=.*/dependency_libs=' -ltinfo -liconv -lm'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libcdio.la

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

