#!/bin/bash

#
# Script to download and build hunspell
#

DOWNLOAD_NAME="hunspell"
DOWNLOAD_VERSION="1.7.2"
DOWNLOAD_URL="https://github.com/hunspell/hunspell/releases/download/v${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="hunspell-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libhunspell-1.7.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_SRCDIR}
autoreconf -vfi

mv config.sub config.sub.OLD
cp ${CWD}/../config.sub.laylaos config.sub

mv m4/libtool.m4 m4/libtool.m4.OLD
cp ${CWD}/../libtool.m4.laylaos m4/libtool.m4

autoreconf

# build
mkdir -p ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

${DOWNLOAD_SRCDIR}/configure --host=${BUILD_TARGET} \
    --enable-shared --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

