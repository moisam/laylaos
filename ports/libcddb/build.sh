#!/bin/bash

#
# Script to download and build libcddb
#

DOWNLOAD_NAME="libcddb"
DOWNLOAD_VERSION="1.3.2"
DOWNLOAD_URL="http://prdownloads.sourceforge.net/libcddb/"
DOWNLOAD_PREFIX="libcddb-"
DOWNLOAD_SUFFIX=".tar.bz2"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libcddb.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

CFLAGS="$CFLAGS -mstackrealign" \
    ../configure \
    --host=${BUILD_TARGET} --prefix=/usr --with-pic=yes \
    ac_cv_func_malloc_0_nonnull=yes ac_cv_func_realloc_0_nonnull=yes \
    --with-libiconv-prefix=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix libcddb.la for the future generations
sed -i "s/dependency_libs=.*/dependency_libs=' -liconv'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libcddb.la

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

