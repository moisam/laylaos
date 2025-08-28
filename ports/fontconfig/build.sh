#!/bin/bash

#
# Script to download and build fontconfig
#

DOWNLOAD_NAME="fontconfig"
DOWNLOAD_VERSION="2.15.0"
DOWNLOAD_URL="https://www.freedesktop.org/software/fontconfig/release/"
DOWNLOAD_PREFIX="fontconfig-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

export CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign"

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfontconfig.so

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

#mv ${DOWNLOAD_SRCDIR}/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/m4/libtool.m4.OLD
#cp ../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/m4/libtool.m4

#cd ${DOWNLOAD_SRCDIR} && autoreconf

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
#mkdir ${DOWNLOAD_SRCDIR}/build2
#cd ${DOWNLOAD_SRCDIR}/build2

#../configure --sysconfdir=/etc --prefix=/usr --mandir=/usr/share/man \
#    --host=${BUILD_TARGET} --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
#    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

#make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

#make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${DOWNLOAD_SRCDIR}
meson setup build --cross-file ${CWD}/../crossfile.meson.laylaos \
    -Dsysconfdir=/etc -Dprefix=/usr -Dmandir=/usr/share/man -Dc_args="${CFLAGS}" \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

# Search & Replace any '-pthread' to nothing in build/build.ninja
sed  -i -e "s/-pthread//" build/build.ninja

meson compile -C build || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

meson install -C build --destdir=${CROSSCOMPILE_SYSROOT_PATH} \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix libfontconfig.la for the future generations
sed -i "s/dependency_libs=.*/dependency_libs='-lfreetype -lpng16 -lz -lexpat -lm'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfontconfig.la

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

