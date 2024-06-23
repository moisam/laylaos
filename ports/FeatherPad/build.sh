#!/bin/bash

#
# Script to download and build FeatherPad
#

DOWNLOAD_NAME="FeatherPad"
DOWNLOAD_VERSION="1.1.1"    # unfortunately this is the last version supporting Qt5.12
DOWNLOAD_URL="https://github.com/tsujan/FeatherPad/releases/download/V${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="FeatherPad-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/featherpad

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
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

Qt5Core_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-5.12/lib/cmake/Qt5Core" \
    Qt5Gui_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-5.12/lib/cmake/Qt5Gui" \
    Qt5Widgets_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-5.12/lib/cmake/Qt5Widgets" \
    Qt5Svg_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-5.12/lib/cmake/Qt5Svg" \
    Qt5Network_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-5.12/lib/cmake/Qt5Network" \
    Qt5PrintSupport_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-5.12/lib/cmake/Qt5PrintSupport" \
    cmake .. --toolchain ${CWD}/../prep_cross.cmake \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

