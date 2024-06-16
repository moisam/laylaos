#!/bin/bash

#
# Script to download and build Qt5
#

DOWNLOAD_NAME="Qt5"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-5.12/lib/libQt5Core.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths

cd ${DOWNLOAD_PORTS_PATH}

# if the directory exists, assume it is a partial build and carry on,
# otherwise try to download the source
if ! [ -d qt5 ]; then
    git clone git://code.qt.io/qt/qt5.git \
        || exit_failure "$0: failed to clone git repo"

    cd qt5
    git checkout 5.12 \
        || exit_failure "$0: failed to checkout from git repo"

    perl init-repository --module-subset=default,-qtwebengine \
        || exit_failure "$0: failed to init repo"
fi

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cp -r ${CWD}/extra/mkspecs/laylaos-g++ ${DOWNLOAD_PORTS_PATH}/qt5/qtbase/mkspecs/
cp -r ${CWD}/extra/plugins/laylaos ${DOWNLOAD_PORTS_PATH}/qt5/qtbase/src/plugins/platforms/
cp ${CWD}/extra/corelib/qstandardpaths_laylaos.cpp ${DOWNLOAD_PORTS_PATH}/qt5/qtbase/src/corelib/io/
cp ${CWD}/extra/qtlocation/laylaos.hpp ${DOWNLOAD_PORTS_PATH}/qt5/qtlocation/src/3rdparty/mapbox-gl-native/deps/boost/1.65.1/include/boost/config/platform/
cp ${CWD}/extra/qtlocation/laylaos.h ${DOWNLOAD_PORTS_PATH}/qt5/qtlocation/src/3rdparty/mapbox-gl-native/deps/boost/1.65.1/include/boost/predef/os/

# fix the paths in qmake.conf
${CWD}/fix_qmake_conf.sh ${DOWNLOAD_PORTS_PATH}/qt5/qtbase/mkspecs/laylaos-g++

# apply the rest of the patches
cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0

# copy our cross tools
[ -d "${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools" ] && \
    rm ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools/* || \
    mkdir ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools

for i in ar as c++ c++filt cpp elfedit g++ gcc gcc-11.3.0 gcc-ar gcc-nm gcc-ranlib gcov gcov-dump gcov-tool gprof ld ld.bfd lto-dump nm objcopy objdump ranlib readelf size strings strip ; do ln -s ${CROSSCOMPILE_TOOLS_PATH}/bin/${BUILD_ARCH}-laylaos-${i} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools/${BUILD_ARCH}-laylaos-${i} ; done

ln -s ${PKG_CONFIG} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools/pkg-config

# configure
mkdir ${DOWNLOAD_PORTS_PATH}/qt5-build
cd ${DOWNLOAD_PORTS_PATH}/qt5-build

export PKG_CONFIG_SYSROOT_DIR=${CROSSCOMPILE_SYSROOT_PATH}
export PKG_CONFIG_LIBDIR=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/pkgconfig
export PKG_CONFIG_PATH=${PKG_CONFIG_LIBDIR}
export CROSS_BUILD_SYSROOT=${CROSSCOMPILE_SYSROOT_PATH}
export CROSS_BUILD_ARCH=${BUILD_ARCH}

CPPFLAGS= CFLAGS= ../qt5/configure \
    -xplatform laylaos-g++ \
    -release -opensource -confirm-license \
    -no-iconv -no-inotify -no-gtk -no-eventfd -no-rpath -no-dbus -no-opengl -no-vulkan \
    -no-xcb-xlib -no-xcb-xinput -no-xkbcommon \
    -no-feature-alsa -no-feature-pulseaudio \
    -system-zlib -system-libjpeg -system-libpng \
    -system-freetype \
    -system-harfbuzz \
    -qt-pcre -qt-sqlite -strip \
    -gstreamer 1.0 \
    -qpa laylaos \
    -force-pkg-config \
    -fontconfig \
    -prefix /usr/Qt-5.12 \
    -extprefix ${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-5.12 \
    -sysroot ${CROSSCOMPILE_SYSROOT_PATH} \
    -device-option CROSS_COMPILE="${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools" \
    -I "=/usr/include/freetype2" \
    -I "=/usr/include/libpng16" \
    -I "=/usr/include/openssl" \
    -I "=/usr/include" \
    -I "${CXX_INCLUDE_PATH}" -I "${CXX_INCLUDE_PATH}/${BUILD_ARCH}-laylaos" \
    -D _GNU_SOURCE -D __laylaos__ -D __${BUILD_ARCH}__ \
        || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

# build & install
make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"
make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# clean up
cd ${CWD}
rm -rf ${DOWNLOAD_PORTS_PATH}/qt5
rm -rf ${DOWNLOAD_PORTS_PATH}/qt5-build
rm -rf ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

