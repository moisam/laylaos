#!/bin/bash

#
# Script to download and build mpfr
#

DOWNLOAD_NAME="mpfr"
DOWNLOAD_URL="https://gcc.gnu.org/pub/gcc/infrastructure/"
DOWNLOAD_VERSION="4.1.0"
DOWNLOAD_PREFIX="mpfr-"
DOWNLOAD_SUFFIX=".tar.bz2"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libmpfr.so

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

mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

mv ${DOWNLOAD_SRCDIR}/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/m4/libtool.m4.OLD
cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/m4/libtool.m4

# build
mkdir -p ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

${DOWNLOAD_SRCDIR}/configure --host=${BUILD_TARGET} \
    --enable-shared \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make  # yes -- do it twice! the first one is fucked up by autoconf
make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix libfreetype.la for the future generations
sed -i "s/dependency_libs=.*/dependency_libs='-lgmp'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libmpfr.la

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

