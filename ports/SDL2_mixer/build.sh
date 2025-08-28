#!/bin/bash

#
# Script to download and build SDL2_mixer
#

DOWNLOAD_NAME="SDL2_mixer"
DOWNLOAD_VERSION="2.6.3"
DOWNLOAD_URL="https://www.libsdl.org/projects/SDL_mixer/release/"
DOWNLOAD_PREFIX="SDL2_mixer-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libSDL2_mixer.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

#mv ${DOWNLOAD_SRCDIR}/build-scripts/config.sub ${DOWNLOAD_SRCDIR}/build-scripts/config.sub.OLD
#cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/build-scripts/config.sub

#mv ${DOWNLOAD_SRCDIR}/build-scripts/config.guess ${DOWNLOAD_SRCDIR}/build-scripts/config.guess.OLD
#cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/build-scripts/config.guess

#mv ${DOWNLOAD_SRCDIR}/acinclude/libtool.m4 ${DOWNLOAD_SRCDIR}/acinclude/libtool.m4.OLD
#cp ../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/acinclude/libtool.m4

#cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build SDL2_mixer
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

# force SDL_mixer to dynamically load vorbis and not use its stub, as it cannot
# play some Ogg files

#../configure \
#    CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign" \
#    LDFLAGS="-L${CROSSCOMPILE_SYSROOT_PATH}/usr/lib" \
#    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} --host=${BUILD_TARGET} \
#    --prefix=/usr --enable-shared=yes --enable-static=yes \
#    --with-sdl-prefix=${CROSSCOMPILE_SYSROOT_PATH}/usr \
#    --with-sdl-exec-prefix=${CROSSCOMPILE_SYSROOT_PATH}/usr \
#    --enable-music-ogg-vorbis --disable-music-ogg-stb \
#    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

sed -i "s~sys/unistd.h~unistd.h~g" ../CMakeLists.txt

cmake .. --toolchain ${CWD}/../prep_cross.cmake -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DSDL2MIXER_VORBIS=VORBISFILE -DSDL2_DIR=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DSDL2MIXER_MOD=OFF -DSDL2MIXER_MOD_MODPLUG=OFF -DSDL2MIXER_MIDI_FLUIDSYNTH=OFF \
    -Dopusfile_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libopusfile.so \
    -Dopusfile_INCLUDE_PATH=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    -Dvorbisfile_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libvorbisfile.so \
    -Dvorbisfile_INCLUDE_PATH=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    -DFLAC_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libFLAC.so \
    -DFLAC_INCLUDE_PATH=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    -DSDL2_MAIN_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libSDL2.so \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

#make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"
make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix libSDL2_mixer.la for the future generations
#sed -i "s/dependency_libs=.*/dependency_libs='-lSDL2 -lfreetype -liconv -ldl -lgui -lpng16 -lz -lrt -lm'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libSDL2_mixer.la

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

