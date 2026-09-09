#!/bin/bash

#
# Script to download and build glew
#

DOWNLOAD_NAME="glew"
DOWNLOAD_VERSION="2.2.0"
DOWNLOAD_URL="https://github.com/nigels-com/glew/releases/download/glew-${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="glew-"
DOWNLOAD_SUFFIX=".tgz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libGLEW.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

rm ${DOWNLOAD_SRCDIR}/config/config.guess
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config/config.guess

cp ${CWD}/extra/Makefile.laylaos ${DOWNLOAD_SRCDIR}/config/

# build
cd ${DOWNLOAD_SRCDIR}/

GLEW_NO_GLU="-DGLEW_NO_GLU" LD="$CC" \
    make \
    SYSTEM=laylaos \
    DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

