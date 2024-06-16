#!/bin/bash

#
# Script to download and build aomedia
#

DOWNLOAD_NAME="aomedia"
DOWNLOAD_VERSION="3.8.1"
DOWNLOAD_URL="https://aomedia.googlesource.com/aom/+archive"
DOWNLOAD_FILE="bb6430482199eaefbeaaa396600935082bc43f66.tar.gz"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/aomedia-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libaom.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths

# Download first
download_only

# Then extract
echo "   ==> Extracting ${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}"
mkdir ${DOWNLOAD_SRCDIR}
tar -C ${DOWNLOAD_SRCDIR} -xf ${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}

# And remove the downloaded file
echo "   ==> Removing ${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}"
rm ${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

cmake .. --toolchain ${CWD}/../prep_cross.cmake -DBUILD_SHARED_LIBS=ON || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

