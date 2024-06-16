#!/bin/bash

#
# Script to download and build links
#

DOWNLOAD_NAME="links"
DOWNLOAD_VERSION="2.29"
DOWNLOAD_URL="http://links.twibright.com/download/"
DOWNLOAD_PREFIX="links-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/links

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

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

../configure \
    --host=${BUILD_TARGET} --prefix=/usr \
    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    --without-ipv6 --without-libevent --without-gpm \
    --without-brotli --without-x \
    --without-directfb --without-pmshell --without-window \
    --without-atheos --without-haiku --without-grx \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

