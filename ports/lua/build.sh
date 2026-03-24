#!/bin/bash

#
# Script to download and build lua
#

DOWNLOAD_NAME="lua"
DOWNLOAD_VERSION="5.2.4"
DOWNLOAD_URL="https://www.lua.org/ftp/"
DOWNLOAD_PREFIX="lua-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/lua

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0

# build
cd ${DOWNLOAD_SRCDIR}/

CC="$CC -std=gnu99" AR="$AR rcu" \
    make laylaos \
    || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make install INSTALL_TOP=${CROSSCOMPILE_SYSROOT_PATH}/usr/ \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cp ${CWD}/*.pc ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/pkgconfig/

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

