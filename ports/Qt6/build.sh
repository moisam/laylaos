#!/bin/bash

#
# Script to download and build Qt6
#

DOWNLOAD_NAME="Qt6"
PATCH_FILE=${DOWNLOAD_NAME}.diff
PATCH_FILE_NATIVE_BUILD=${DOWNLOAD_NAME}.laylaos.diff
CWD=`pwd`

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-6.11/lib/libQt6Core.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths

cd ${DOWNLOAD_PORTS_PATH}

wget https://download.qt.io/archive/qt/6.11/6.11.1/single/qt-everywhere-src-6.11.1.tar.xz \
        || exit_failure "$0: failed to download ${DOWNLOAD_NAME}"
tar -xf qt-everywhere-src-6.11.1.tar.xz \
        || exit_failure "$0: failed to extract ${DOWNLOAD_NAME}"
mv qt-everywhere-src-6.11.1 qt6
rm qt-everywhere-src-6.11.1.tar.xz

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cp -r ${CWD}/extra/qtbase/mkspecs/laylaos-g++ ${DOWNLOAD_PORTS_PATH}/qt6/qtbase/mkspecs/
cp ${CWD}/extra/qtbase/corelib/qstandardpaths_laylaos.cpp ${DOWNLOAD_PORTS_PATH}/qt6/qtbase/src/corelib/io/
cp -r ${CWD}/extra/qtbase/plugins/laylaos ${DOWNLOAD_PORTS_PATH}/qt6/qtbase/src/plugins/platforms/
cp -r ${CWD}/extra/qtbase/tests/qfileselector/platforms/+laylaos ${DOWNLOAD_PORTS_PATH}/qt6/qtbase/tests/auto/corelib/io/qfileselector/platforms/
cp -r ${CWD}/extra/qtbase/tests/qfileselector/platforms/+unix/+laylaos ${DOWNLOAD_PORTS_PATH}/qt6/qtbase/tests/auto/corelib/io/qfileselector/platforms/+unix/
cp -r ${CWD}/extra/qtmultimedia/multimedia/laylaos ${DOWNLOAD_PORTS_PATH}/qt6/qtmultimedia/src/multimedia/
cp -r ${CWD}/extra/qtmultimedia/plugins/laylaos ${DOWNLOAD_PORTS_PATH}/qt6/qtmultimedia/src/plugins/multimedia/

# fix the paths in qmake.conf
${CWD}/fix_qmake_conf.sh ${DOWNLOAD_PORTS_PATH}/qt6/qtbase/mkspecs/laylaos-g++

# apply the rest of the patches
cd ${DOWNLOAD_PORTS_PATH}
patch -i ${CWD}/${PATCH_FILE} -p0

# copy our cross tools
[ -d "${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools" ] && \
    rm ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools/* || \
    mkdir ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools

for i in ar as c++ c++filt cpp elfedit g++ gcc gcc-11.3.0 gcc-ar gcc-nm gcc-ranlib gcov gcov-dump gcov-tool gprof ld ld.bfd lto-dump nm objcopy objdump ranlib readelf size strings strip ; do ln -s ${CROSSCOMPILE_TOOLS_PATH}/bin/${BUILD_ARCH}-laylaos-${i} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools/${BUILD_ARCH}-laylaos-${i} ; done

ln -s ${PKG_CONFIG} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools/pkg-config

# configure
mkdir ${DOWNLOAD_PORTS_PATH}/qt6-build
cd ${DOWNLOAD_PORTS_PATH}/qt6-build

export PKG_CONFIG_SYSROOT_DIR=${CROSSCOMPILE_SYSROOT_PATH}
export PKG_CONFIG_LIBDIR=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/pkgconfig
export PKG_CONFIG_PATH=${PKG_CONFIG_LIBDIR}
export CROSS_BUILD_SYSROOT=${CROSSCOMPILE_SYSROOT_PATH}
export CROSS_BUILD_ARCH=${BUILD_ARCH}

OLD_EXTRA_CPPFLAGS=${EXTRA_CPPFLAGS}
# these are needed to build target qmake
export EXTRA_CPPFLAGS="-D_GNU_SOURCE -D__laylaos__ -D__${BUILD_ARCH}__ -fPIC"

CPPFLAGS="" CFLAGS="" ../qt6/configure \
 -release \
 -xplatform laylaos-g++ \
 -skip qtopcua -skip qtgrpc \
 -make examples \
 -nomake tests \
 -no-rpath \
 -no-dbus \
 -no-vulkan \
 -no-xcb \
 -no-accessibility \
 -system-zlib \
 -system-freetype \
 -qt-harfbuzz \
 -system-libpng \
 -system-libjpeg \
 -sql-sqlite -qt-sqlite \
 -ssl -openssl-runtime \
 -opengl -egl \
 -cups \
 -no-feature-xkbcommon \
 -no-feature-alsa \
 -no-feature-pulseaudio \
 -no-feature-qdbus \
 -no-feature-bluetooth \
 -no-feature-nfc \
 -no-feature-emojisegmenter \
 -feature-fontconfig \
 -qt-pcre \
 -qt-sqlite \
 -strip \
 -gstreamer \
 -qpa laylaos \
 -force-pkg-config \
 -prefix /usr/Qt-6.11 \
 -extprefix ${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-6.11 \
 -device-option CROSS_COMPILE="${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools" \
 -- \
 -DQT_HOST_PATH="${HOST_QT6_FOR_CROSSCOMPILE}" \
 -DQT_QMAKE_TARGET_MKSPEC=laylaos-g++ \
 -DQT_BUILD_EXAMPLES=ON \
 -DQT_BUILD_EXAMPLES_BY_DEFAULT=ON \
 -DQT_BUILD_TOOLS_WHEN_CROSSCOMPILING=ON \
 -DQT_FORCE_BUILD_TOOLS=ON \
 -DFEATURE_iconv=ON \
 -DFEATURE_inotify=OFF \
 -DFEATURE_gtk3=OFF \
 -DFEATURE_eventfd=OFF \
 -DFEATURE_ffmpeg=ON \
 -DQT_FEATURE_ffmpeg=ON \
 -DCMAKE_TOOLCHAIN_FILE="${CWD}/../prep_cross_gui.cmake" \
 -DHAVE_ICONV=1 -DHAVE_ICONV_WITH_LIB=1 \
 -DOPENGL_INCLUDE_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include" \
 -DOPENGL_EGL_INCLUDE_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include" \
 -DGLESv2_INCLUDE_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include" \
 -DZLIB_INCLUDE_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include" \
 -DOPENSSL_INCLUDE_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include" \
 -DPCRE2_INCLUDE_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include" \
 -DFREETYPE_INCLUDE_DIR_ft2build="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include/freetype2" \
 -DFREETYPE_INCLUDE_DIR_freetype2="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include/freetype2" \
 -DJPEG_INCLUDE_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include" \
 -DCUPS_INCLUDE_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include" \
 -DFFMPEG_INCLUDE_DIRS="${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include" \
 -DOPENGL_gl_LIBRARY="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libGL.so" \
 -DOPENGL_egl_LIBRARY="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libEGL.so" \
 -DOPENGL_glu_LIBRARY="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libGLU.so" \
 -DGLESv2_LIBRARY="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libGLESv2.so" \
 -DZLIB_LIBRARY_RELEASE="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libz.so" \
 -DOPENSSL_SSL_LIBRARY="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libssl.so" \
 -DOPENSSL_CRYPTO_LIBRARY="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libcrypto.so" \
 -DPCRE2_8BIT_LIBRARY="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libpcre2-8.so" \
 -DPCRE2_16BIT_LIBRARY="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libpcre2-16.so" \
 -DPCRE2_32BIT_LIBRARY="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libpcre2-32.so" \
 -DPCRE2_POSIX_LIBRARY="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libpcre2-posix.so" \
 -DFREETYPE_LIBRARY_RELEASE="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfreetype.so" \
 -DJPEG_LIBRARY_RELEASE="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libjpeg.so" \
 -DCUPS_LIBRARIES="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libcups.so" \
 -DFFMPEG_LIBRARY_DIRS="${CROSSCOMPILE_SYSROOT_PATH}/usr/lib" \
 -DFFMPEG_DIR="${CROSSCOMPILE_SYSROOT_PATH}/usr" \
 -DTIFF_ROOT="${CROSSCOMPILE_SYSROOT_PATH}/usr" \
 -DPNG_ROOT="${CROSSCOMPILE_SYSROOT_PATH}/usr" \
 -DUNIX=True \
 -DCMAKE_HOST_UNIX=True \
 -DQT_GENERATE_SBOM=OFF \
 -DFEATURE_sbom=OFF \
 -DCMAKE_SHARED_LIBRARY_SONAME_C_FLAG="-Wl,-soname," \
 -DCMAKE_SHARED_LIBRARY_SONAME_CXX_FLAG="-Wl,-soname," \
 -DCMAKE_SHARED_LIBRARY_CREATE_C_FLAGS="-shared" \
 -DCMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS="-shared" \
 -DCMAKE_EXE_LINKER_FLAGS="-Wl,-rpath-link,${PWD}/qtbase/lib" \
        || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

sed  -i -e "s/-pthread//" ./build.ninja

# build & install
cmake --build . --target qhelpgenerator \
    || exit_failure "$0: failed to build ${DOWNLOAD_NAME} qhelpgenerator"

ninja -j3 || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

cmake --install . -v --prefix ${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-6.11 \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# clean up
cd ${CWD}
rm -rf ${DOWNLOAD_PORTS_PATH}/qt6
rm -rf ${DOWNLOAD_PORTS_PATH}/qt6-build
rm -rf ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools

export EXTRA_CPPFLAGS=${OLD_EXTRA_CPPFLAGS}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

