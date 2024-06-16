#!/bin/bash

#
# Script to download and build Vulkan-Headers
#

DOWNLOAD_NAME="Vulkan-Headers"
DOWNLOAD_VERSION="1.3.278"
DOWNLOAD_URL="https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="v${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/Vulkan-Headers-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/vulkan/registry/vk.xml

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

# Don't install the headers from the include dir, as we already copied the fixed ones
# from SwiftShader

# Patch the registry files and copy to our sysroot

mkdir ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/vulkan
cp -r ${DOWNLOAD_SRCDIR}/registry/ ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/vulkan/registry/

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

