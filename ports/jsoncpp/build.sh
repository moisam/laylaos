#!/bin/bash

#
# Script to download and build jsoncpp
#

DOWNLOAD_NAME="jsoncpp"
DOWNLOAD_URL="https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/"
DOWNLOAD_VERSION="1.9.5"
DOWNLOAD_PREFIX=""
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

export CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include"

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/jsoncpp-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libjsoncpp.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# build
cd ${DOWNLOAD_SRCDIR}

CPPFLAGS="-D_GNU_SOURCE -D__laylaos__ -D__${BUILD_ARCH}__" CFLAGS="${CFLAGS} -mstackrealign" \
    meson setup build --cross-file ${CWD}/../crossfile.meson.laylaos \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

meson compile -C build || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

meson install -C build --destdir=${CROSSCOMPILE_SYSROOT_PATH} || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

