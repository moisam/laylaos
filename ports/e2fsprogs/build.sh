#!/bin/bash

#
# Script to download and build e2fsprogs
#

DOWNLOAD_NAME="e2fsprogs"
DOWNLOAD_VERSION="1.47.2"
DOWNLOAD_URL="https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="e2fsprogs-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/sbin/mke2fs

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

rm ${DOWNLOAD_SRCDIR}/config/config.sub
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config/config.sub

rm ${DOWNLOAD_SRCDIR}/config/config.guess
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config/config.guess

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

CPPFLAGS="${CPPFLAGS} -D_GNU_SOURCE -mstackrealign" \
    ../configure \
    --host=${BUILD_TARGET} --prefix=/usr --disable-tls \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

find . -type f -exec sed -i 's/-rdynamic//g' {} \;

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

