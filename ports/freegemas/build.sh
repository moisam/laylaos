#!/bin/bash

#
# Script to download and build freegemas
#

DOWNLOAD_NAME="freegemas"
DOWNLOAD_URL="https://github.com/JoseTomasTocino/freegemas.git"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/freegemas"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/freegemas

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
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

# copy executable
cp freegemas ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/ || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# copy app resources
mkdir -p ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/games/freegemas
cp -R ../media ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/games/freegemas/ || exit_failure "$0: failed to install resource files for ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

