#!/bin/bash

#
# Script to download and build gptfdisk
#

DOWNLOAD_NAME="gptfdisk"
DOWNLOAD_VERSION="1.0.10"
DOWNLOAD_URL="https://www.rodsbooks.com/gdisk/"
DOWNLOAD_PREFIX="gptfdisk-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/sbin/gdisk

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

CPPFLAGS= CXXFLAGS="-D__laylaos__ -D__${BUILD_ARCH}__ -D_GNU_SOURCE -fPIC -DPIC" \
    LDFLAGS=-ltinfow TARGET=laylaos \
    make \
    || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

cp cgdisk fixparts gdisk sgdisk ${CROSSCOMPILE_SYSROOT_PATH}/usr/sbin/ \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cp *.8 ${CROSSCOMPILE_SYSROOT_PATH}/usr/man/man8/ \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME} manpages"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

