#!/bin/bash

#
# Script to download and build zstd
#

DOWNLOAD_NAME="zstd"
DOWNLOAD_VERSION="1.5.7"
DOWNLOAD_URL="https://github.com/facebook/zstd/releases/download/v${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="zstd-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libzstd.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# build
cd ${DOWNLOAD_SRCDIR}

cp Makefile Makefile.save
sed -i 's/-pthread//g' lib/Makefile 

cmake \
    -S build/cmake -B build-cmake \
    --toolchain ${CWD}/../prep_cross.cmake \
    -DBUILD_SHARED_LIBS=ON \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

export LIBDIR=/usr/lib
export BINDIR=/usr/bin
export INCLUDEDIR=/usr/include
export MAN1DIR=/usr/man/man1
export MANDIR=/usr/man

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# cleanup
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

