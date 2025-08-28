#!/bin/bash

#
# Script to download and build Midnight Commander
#

DOWNLOAD_NAME="mc"
DOWNLOAD_VERSION="4.8.32"
DOWNLOAD_URL="http://ftp.midnight-commander.org/"
DOWNLOAD_PREFIX="mc-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/mc

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
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config/config.sub

mv ${DOWNLOAD_SRCDIR}/config/config.guess ${DOWNLOAD_SRCDIR}/config/config.guess.OLD
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config/config.guess

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

# build the lib without progs (tic, ...)

../configure  \
    --host=${BUILD_TARGET} --prefix=/usr --sysconfdir=/etc \
    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    --disable-vfs --disable-largefile --without-x \
    --with-screen=ncurses --disable-nls --disable-charset --without-subshell \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

