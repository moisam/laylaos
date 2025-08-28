#!/bin/bash

#
# Script to download and build hexedit
#

DOWNLOAD_NAME="hexedit"
DOWNLOAD_VERSION="1.6"
DOWNLOAD_URL="https://github.com/pixel/hexedit/archive/refs/tags/"
DOWNLOAD_PREFIX=""
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/hexedit-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/hexedit

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

# build
cd ${DOWNLOAD_SRCDIR}

./autogen.sh
./configure \
    --host=${BUILD_TARGET} --prefix=/usr \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make CPPFLAGS="${CPPFLAGS} -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/ncurses" \
    || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

