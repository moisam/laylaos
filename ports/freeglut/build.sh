#!/bin/bash

#
# Script to download and build freeglut
#

DOWNLOAD_NAME="freeglut"
DOWNLOAD_VERSION="3.6.0"
DOWNLOAD_URL="https://github.com/freeglut/freeglut/releases/download/v${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="freeglut-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libglut.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cp -R ${CWD}/extra/laylaos ${DOWNLOAD_SRCDIR}/src/

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

cmake .. \
    --toolchain ${CWD}/../prep_cross_gui.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DOPENGL_gl_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libGL.so \
    -DOPENGL_INCLUDE_DIR=${CROSSCOMPILE_SYSROOT_PATH}/usr/include/ \
    -DOPENGL_GLU_FOUND=True \
    -DOPENGL_glu_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libGLU.so \
    -DEGL_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libEGL.so \
    -DFREETYPE_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfreetype.so \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# set the SONAME so everybody else can link to it without using the full pathname
patchelf --set-soname libglut.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libglut.so

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

