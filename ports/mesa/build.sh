#!/bin/bash

#
# Script to download and build mesa 3d
#

DOWNLOAD_NAME="mesa"
DOWNLOAD_VERSION="25.2.5"
DOWNLOAD_URL="https://archive.mesa3d.org/"
DOWNLOAD_PREFIX="mesa-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/mesa-25.2"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libEGL.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

# move from 25.2.5 to 25.2 so our patch works
mv "${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}" "${DOWNLOAD_SRCDIR}"

cp ${CWD}/extra/egl/platform_laylaos.c ${DOWNLOAD_SRCDIR}/src/egl/drivers/dri2/
cp -R ${CWD}/extra/gallium/laylaos ${DOWNLOAD_SRCDIR}/src/gallium/targets/

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
cd ${DOWNLOAD_SRCDIR}

CPPFLAGS="`${PKG_CONFIG} --cflags freetype2`" \
    CFLAGS="-D_GNU_SOURCE -D__laylaos__ -D__${BUILD_ARCH}__ -mstackrealign" \
    CXXFLAGS="-D_GNU_SOURCE -D__laylaos__ -D__${BUILD_ARCH}__ -mstackrealign" \
    meson setup build2 \
    --cross-file ${CWD}/../crossfile.meson.laylaos \
    -Degl=enabled -Degl-native-platform=laylaos \
    -Dgles1=enabled -Dgles2=enabled -Dglx=disabled -Dopengl=true \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

# Search & Replace any '-pthread' to nothing in build/build.ninja
sed  -i -e "s/-pthread//" build2/build.ninja

meson compile -C build2 || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

meson install -C build2 --destdir=${CROSSCOMPILE_SYSROOT_PATH} \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

