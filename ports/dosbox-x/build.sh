#!/bin/bash

#
# Script to download and build dosbox-x
#

DOWNLOAD_NAME="dosbox-x"
DOWNLOAD_VERSION="2025.10.07"
DOWNLOAD_URL="https://github.com/joncampbell123/dosbox-x/archive/refs/tags/"
DOWNLOAD_PREFIX="dosbox-x-v"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/dosbox-x

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# rename the extracted dir so our patch works
mv "${DOWNLOAD_PORTS_PATH}/dosbox-x-dosbox-x-v${DOWNLOAD_VERSION}" ${DOWNLOAD_SRCDIR}

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_SRCDIR}/
./autogen.sh

mv ./config.sub ./config.sub.OLD
cp ${CWD}/../config.sub.laylaos ./config.sub

mv ./config.guess ./config.guess.OLD
cp ${CWD}/../config.guess.laylaos ./config.guess

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0

# build
cd ${DOWNLOAD_SRCDIR}/

LDFLAGS="-lgui" \
    CPPFLAGS="-D__laylaos__ -D__${BUILD_ARCH}__ -I${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include/ncurses -I${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include/SDL2" \
    ./configure \
    --host=${BUILD_TARGET} --prefix=/usr \
    --enable-sdl2=yes --enable-debug --disable-x11 --disable-alsa-midi \
    --with-sdl2-prefix="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/" \
    --disable-opengl \
    ac_cv_path_SDL2_CONFIG="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/bin/sdl2-config" \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

