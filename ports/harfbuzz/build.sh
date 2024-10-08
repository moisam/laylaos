#!/bin/bash

#
# Script to download and build harfbuzz
#

DOWNLOAD_NAME="harfbuzz"
DOWNLOAD_VERSION="8.3.0"
DOWNLOAD_URL="https://github.com/harfbuzz/harfbuzz/releases/download/${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="harfbuzz-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libharfbuzz.so

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
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub
mv ${DOWNLOAD_SRCDIR}/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/m4/libtool.m4.OLD
cp ../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/m4/libtool.m4

cd ${DOWNLOAD_SRCDIR} && autoreconf

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

CFLAGS="-mstackrealign" CXXFLAGS="-I${CXX_INCLUDE_PATH} -mstackrealign" \
    ../configure --host=${BUILD_TARGET} \
    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} --enable-shared \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix libharfbuzz.la et al for the future generations
sed -i "s/dependency_libs=.*/dependency_libs='-lm -lfreetype -lpng16 -lz'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libharfbuzz.la
sed -i "s/dependency_libs=.*/dependency_libs='-lharfbuzz -lm -lfreetype -lpng16 -lz'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libharfbuzz-cairo.la
sed -i "s/dependency_libs=.*/dependency_libs='-lharfbuzz -lm -lfreetype -lpng16 -lz'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libharfbuzz-subset.la

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

