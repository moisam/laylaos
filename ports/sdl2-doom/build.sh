#!/bin/bash

#
# Script to download and build sdl2-doom
#

DOWNLOAD_NAME="sdl2-doom"
DOWNLOAD_URL="https://github.com/moisam/laylaos-sdl2-doom/archive/refs/heads/"
DOWNLOAD_FILE="master.zip"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/laylaos-sdl2-doom-master"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/sdl2-doom

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_only

# extract
echo " ==> Extracting ${DOWNLOAD_NAME}"
cd ${DOWNLOAD_PORTS_PATH}
unzip ${DOWNLOAD_FILE} || exit_failure "$0: failed to unzip ${DOWNLOAD_FILE}"
rm ${DOWNLOAD_FILE}

# build
cd ${DOWNLOAD_SRCDIR}
chmod +x ./build_laylaos.sh
./build_laylaos.sh || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

