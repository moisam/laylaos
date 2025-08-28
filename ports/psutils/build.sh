#!/bin/bash

#
# Script to download and build psutils
#

DOWNLOAD_NAME="psutils"
DOWNLOAD_VERSION=""
DOWNLOAD_URL="http://knackered.org/angus/download/psutils/"
DOWNLOAD_PREFIX="psutils"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/psselect

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cp ${DOWNLOAD_SRCDIR}/Makefile.unix ${DOWNLOAD_SRCDIR}/Makefile

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
cd ${DOWNLOAD_SRCDIR}
make PREFIX=${CROSSCOMPILE_SYSROOT_PATH}/usr || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make PREFIX=${CROSSCOMPILE_SYSROOT_PATH}/usr install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

