#!/bin/bash

#
# Script to download and build lite
#

DOWNLOAD_NAME="lite"
DOWNLOAD_VERSION="1.11"
DOWNLOAD_URL="https://github.com/rxi/lite/archive/refs/tags/"
DOWNLOAD_PREFIX="v"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/lite-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/lite

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

./build.sh \
    || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

cp lite ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/ \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

mkdir ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/lite \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cp -r data ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/lite/ \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

