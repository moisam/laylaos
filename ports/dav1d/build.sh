#!/bin/bash

#
# Script to download and build dav1d
#

DOWNLOAD_NAME="dav1d"
DOWNLOAD_VERSION="1.4.0"
DOWNLOAD_URL="https://code.videolan.org/videolan/dav1d/-/archive/${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="dav1d-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

export CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign"

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libdav1d.so

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
cd ${DOWNLOAD_SRCDIR}

meson setup build --cross-file ${CWD}/../crossfile.meson.laylaos || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

# Search & Replace any '-pthread' to nothing in build/build.ninja
sed  -i -e "s/-pthread//" build/build.ninja

meson compile -C build || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

meson install -C build --destdir=${CROSSCOMPILE_SYSROOT_PATH} || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

