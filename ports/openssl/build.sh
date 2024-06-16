#!/bin/bash

#
# Script to download and build openssl
#

DOWNLOAD_NAME="openssl"
DOWNLOAD_VERSION="3.1.1"
DOWNLOAD_URL="https://www.openssl.org/source/old/3.1/"
DOWNLOAD_PREFIX="openssl-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libssl.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cp ${CWD}/extra/50-laylaos.conf ${DOWNLOAD_SRCDIR}/Configurations/

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

CC= CXX= AR= AS= RANLIB= \
    ../Configure \
    --prefix=/usr \
    --openssldir=/usr/ssl \
    --cross-compile-prefix=${CROSSCOMPILE_TOOLS_PATH}/bin/${BUILD_ARCH}-laylaos- \
    -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    --sysroot=${CROSSCOMPILE_SYSROOT_PATH} -isystem=/usr/include \
    laylaos-${BUILD_ARCH} \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

