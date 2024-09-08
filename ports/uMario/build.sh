#!/bin/bash

#
# Script to download and build uMario
#

DOWNLOAD_NAME="uMario"
DOWNLOAD_URL="https://github.com/jakowskidev/uMario_Jakowski.git"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/uMario_Jakowski"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/uMario

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths

cd ${DOWNLOAD_PORTS_PATH}

git clone ${DOWNLOAD_URL}

[ $? -ne 0 ] && exit_failure "$0: failed to clone ${DOWNLOAD_URL}"

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

cmake .. --toolchain ${CWD}/../prep_cross.cmake -DCMAKE_BUILD_TYPE=Release \
    -DSDL2_MIXER_INCLUDE_DIR=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    -DSDL2_IMAGE_INCLUDE_DIR=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    -DSDL2_IMAGE_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libSDL2_image.so \
    -DSDL2_MIXER_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libSDL2_mixer.so \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

# copy executable
cp uMario ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/ || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# copy app resources
mkdir -p ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/games/uMario
cp -R ./files/ ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/games/uMario/ || exit_failure "$0: failed to install resource files for ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

