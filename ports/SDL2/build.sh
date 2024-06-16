#!/bin/bash

#
# Script to download and build SDL2
#

DOWNLOAD_NAME="SDL2"
DOWNLOAD_VERSION="2.26.5"
DOWNLOAD_URL="https://github.com/libsdl-org/SDL/releases/download/release-${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="SDL2-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libSDL2.so

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

cp ./extra/SDL_config_laylaos.h ${DOWNLOAD_SRCDIR}/include/
cp ./extra/vulkan_laylaos.h ${DOWNLOAD_SRCDIR}/src/video/khronos/vulkan/
cp -r ./extra/audio/laylaos ${DOWNLOAD_SRCDIR}/src/audio/
cp -r ./extra/video/laylaos ${DOWNLOAD_SRCDIR}/src/video/

mv ${DOWNLOAD_SRCDIR}/build-scripts/config.sub ${DOWNLOAD_SRCDIR}/build-scripts/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/build-scripts/config.sub
mv ${DOWNLOAD_SRCDIR}/acinclude/libtool.m4 ${DOWNLOAD_SRCDIR}/acinclude/libtool.m4.OLD
cp ../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/acinclude/libtool.m4

cd ${DOWNLOAD_SRCDIR} && autoreconf

# build SDL2
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

../configure \
    CPPFLAGS="${CPPFLAGS} `${PKG_CONFIG} --cflags freetype2` -D_POSIX_PRIORITY_SCHEDULING -D_POSIX_TIMERS" \
    CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign" \
    LDFLAGS="-L${CROSSCOMPILE_SYSROOT_PATH}/usr/lib -ldl -lgui -lfreetype" \
    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} --host=${BUILD_TARGET} \
    --prefix=/usr --enable-shared=yes --enable-static=yes \
    --disable-pulseaudio --disable-pulseaudio-shared \
    --disable-arts-shared --disable-nas-shared --disable-sndio-shared \
    --disable-fusionsound-shared --disable-libsamplerate-shared \
    --disable-video-wayland --disable-video-wayland-qt-touch \
    --disable-wayland-shared --disable-video-x11 --disable-x11-shared \
    --disable-video-x11-xcursor --disable-video-x11-xdbe \
    --disable-video-x11-xinput --disable-video-x11-xrandr \
    --disable-video-x11-scrnsaver --disable-video-x11-xshape \
    --disable-video-vivante --disable-video-cocoa \
    --disable-directfb-shared --disable-video-opengl --disable-video-opengles \
    --disable-video-opengles1 --disable-video-opengles2 --disable-libudev \
    --disable-dbus --disable-ime --disable-fcitx --disable-directx \
    --disable-render-d3d --disable-3dnow \
    --disable-haptic --disable-joystick \
    --enable-audio --enable-video --enable-video-laylaos --disable-sse3 \
    --disable-sndio --disable-sndio-shared \
    --enable-video-vulkan --disable-video-offscreen \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# build tests
mkdir tests && cd tests

../../test/configure \
    CFLAGS="${CFLAGS} `${PKG_CONFIG} --cflags freetype2`" \
    CPPFLAGS="${CPPFLAGS} -D_POSIX_PRIORITY_SCHEDULING -D_POSIX_TIMERS -I." \
    LDFLAGS="-L${HOME}/laylaos-x86_64/usr/local/lib -ldl -lgui -lfreetype" \
    --host=${BUILD_TARGET} --prefix=/usr \
    --with-sdl-prefix=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    --with-sdl-exec-prefix=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME} tests"

make
export SDL2_TEST_DEST=${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/SDL2
mkdir -p ${SDL2_TEST_DEST}
cp * ${SDL2_TEST_DEST}/
rm ${SDL2_TEST_DEST}/config.log ${SDL2_TEST_DEST}/config.status ${SDL2_TEST_DEST}/Makefile

cp *.{bmp,wav,dat,test,hex,txt} ${SDL2_TEST_DEST}/

# Fix libSDL2.la for the future generations
sed -i "s/dependency_libs=.*/dependency_libs='-ldl -lgui -lfreetype -lpng16 -lz -lm -liconv -lrt'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libSDL2.la

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

