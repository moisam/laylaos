#!/bin/bash

#
# Script to download and build patch
#

DOWNLOAD_NAME="patch"
DOWNLOAD_VERSION="2.7.6"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/patch/"
DOWNLOAD_PREFIX="patch-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/patch

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/build-aux/config.sub ${DOWNLOAD_SRCDIR}/build-aux/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.sub

mv ${DOWNLOAD_SRCDIR}/build-aux/config.guess ${DOWNLOAD_SRCDIR}/build-aux/config.guess.OLD
cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.guess

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

../configure --host=${BUILD_TARGET} --prefix=/usr \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# cleanup
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

