#!/bin/bash

#
# Script to download and build help2man
#

DOWNLOAD_NAME="help2man"
DOWNLOAD_VERSION="1.49.3"
DOWNLOAD_URL="https://mirror.cyberbits.eu/gnu/help2man/"
DOWNLOAD_PREFIX="help2man-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/help2man

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

PERL=${HOST_PERL_FOR_CROSSCOMPILE} ../configure  \
    --host=${BUILD_TARGET} --prefix=/usr \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# patch these suckers
sed -i '1 s/^.*$/#! \/usr\/bin\/perl -w/' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/help2man

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

