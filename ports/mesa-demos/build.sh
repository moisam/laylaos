#!/bin/bash

#
# Script to download and build mesa-demos
#

DOWNLOAD_NAME="mesa-demos"
DOWNLOAD_VERSION="9.0.0"
DOWNLOAD_URL="https://gitlab.freedesktop.org/mesa/demos/-/archive/mesa-demos-${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="demos-mesa-demos-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/mesa-demos/gears

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

# build
cd ${DOWNLOAD_SRCDIR}

CPPFLAGS="-D_GNU_SOURCE -D__laylaos__ -D__${BUILD_ARCH}__" \
    CFLAGS="${CFLAGS} -mstackrealign" \
    meson setup build \
    --cross-file ${CWD}/../crossfile.meson.laylaos \
    -Dx11=disabled -Dlibdrm=disabled -Dwayland=disabled -Dvulkan=disabled \
    -Dprefix=${CROSSCOMPILE_SYSROOT_PATH}/usr/local/ \
    -Dbindir=bin/mesa-demos/ \
    -Ddatadir=share/ \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

# Fix this issue with the linker commands in build/build.ninja
sed -i "s~libglut.so -Wl,--end-group~libglut.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libgui.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfreetype.so -Wl,--end-group~g" build/build.ninja 

# Search & Replace any '-pthread' to nothing in build/build.ninja
sed  -i -e "s/-pthread//" build/build.ninja

meson compile -C build || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

meson install -C build || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Copy resource files needed by some demos
cp src/glsl/*.frag ${CROSSCOMPILE_SYSROOT_PATH}/usr/local/bin/mesa-demos/
cp src/glsl/*.vert ${CROSSCOMPILE_SYSROOT_PATH}/usr/local/bin/mesa-demos/
cp src/glsl/simplex-noise.glsl ${CROSSCOMPILE_SYSROOT_PATH}/usr/local/bin/mesa-demos/

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

