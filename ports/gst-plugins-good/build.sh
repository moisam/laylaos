#!/bin/bash

#
# Script to download and build gst-plugins-good
#

DOWNLOAD_NAME="gst-plugins-good"
DOWNLOAD_VERSION="1.20.7"
DOWNLOAD_URL="https://gstreamer.freedesktop.org/src/gst-plugins-good/"
DOWNLOAD_PREFIX="gst-plugins-good-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

export CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign"

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/gstreamer-1.0/libgstalpha.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# build
cd ${DOWNLOAD_SRCDIR}

CPPFLAGS="-D_GNU_SOURCE -D__laylaos__ -D__${BUILD_ARCH}__ -fPIC -DPIC" \
    CFLAGS="${CFLAGS} -mstackrealign" \
    meson setup build --cross-file ${CWD}/../crossfile.meson.laylaos \
    --buildtype=release \
    -D doc=disabled -D examples=disabled \
    -D tests=enabled -D nls=disabled -D orc=disabled \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

# Search & Replace any '-pthread' to nothing in build/build.ninja
sed  -i -e "s/-pthread//" build/build.ninja

ninja -C build || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

meson install -C build --destdir=${CROSSCOMPILE_SYSROOT_PATH} || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

