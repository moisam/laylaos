#!/bin/bash

#
# Script to download and build freetype
#

DOWNLOAD_NAME="freetype"
DOWNLOAD_VERSION="2.13.1"
DOWNLOAD_URL="https://download.savannah.gnu.org/releases/freetype/"
DOWNLOAD_PREFIX="freetype-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

export CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign"

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfreetype.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/builds/unix/config.sub ${DOWNLOAD_SRCDIR}/builds/unix/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/builds/unix/config.sub

mv ${DOWNLOAD_SRCDIR}/builds/unix/config.guess ${DOWNLOAD_SRCDIR}/builds/unix/config.guess.OLD
cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/builds/unix/config.guess

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

../configure --host=${BUILD_TARGET} --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Remove the hardcoded .so dependency
patchelf --replace-needed \
    ${CROSSCOMPILE_SYSROOT_PATH}/usr/local/lib/libpng16.so \
    libpng16.so \
    ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfreetype.so

# And fix the rpath
patchelf --set-rpath '/usr/lib:/usr/local/lib' \
    ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfreetype.so

# Fix libfreetype.la for the future generations
sed -i "s/dependency_libs=.*/dependency_libs='-lpng16 -lz'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfreetype.la

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

