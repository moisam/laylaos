#!/bin/bash

#
# Script to download and build pkg-config
#

DOWNLOAD_NAME="pkg-config"
DOWNLOAD_URL="https://pkgconfig.freedesktop.org/releases/"
DOWNLOAD_VERSION="0.29.2"
DOWNLOAD_PREFIX="pkg-config-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/pkg-config

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
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

mv ${DOWNLOAD_SRCDIR}/glib/config.sub ${DOWNLOAD_SRCDIR}/glib/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/glib/config.sub

mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

mv ${DOWNLOAD_SRCDIR}/glib/config.guess ${DOWNLOAD_SRCDIR}/glib/config.guess.OLD
cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/glib/config.guess

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

../configure \
    --disable-host-tool --host=${BUILD_TARGET} --prefix=/usr \
    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# cleanup
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

