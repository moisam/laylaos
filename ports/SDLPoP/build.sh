#!/bin/bash

#
# Script to download and build SDL Prince of Persia
#

DOWNLOAD_NAME="SDLPoP"
DOWNLOAD_VERSION="1.24-RC"
DOWNLOAD_URL="https://github.com/NagyD/SDLPoP/archive/refs/tags/"
DOWNLOAD_PREFIX="v"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/SDLPoP-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/prince

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/src/build
cd ${DOWNLOAD_SRCDIR}/src/build

cmake .. --toolchain ${CWD}/../prep_cross_gui.cmake \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make all || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

# copy executable
cp ../../prince ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/ || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# copy app resources
mkdir -p ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/games/SDLPoP
cp -R ../../data/ ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/games/SDLPoP/ \
    || exit_failure "$0: failed to install resource files for ${DOWNLOAD_NAME}"
cp -R ../../mods/ ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/games/SDLPoP/ \
    || exit_failure "$0: failed to install resource files for ${DOWNLOAD_NAME}"
cp -R ../../replays/ ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/games/SDLPoP/ \
    || exit_failure "$0: failed to install resource files for ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

