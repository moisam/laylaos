#!/bin/bash

#
# Script to download and build Qt5
#

DOWNLOAD_NAME="Qt5"
PATCH_FILE=${DOWNLOAD_NAME}.diff
PATCH_FILE_NATIVE_BUILD=${DOWNLOAD_NAME}.laylaos.diff
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
#if ! [ -d qt5 ]; then
#    #git clone git://code.qt.io/qt/qt5.git
#    git clone https://github.com/qtproject/qt5.git \
#        || exit_failure "$0: failed to clone git repo"
#
#    cd qt5
#    git checkout 5.12 \
#        || exit_failure "$0: failed to checkout from git repo"
#
#    perl init-repository --module-subset=default,-qtwebengine \
#        || exit_failure "$0: failed to init repo"
#fi

wget https://download.qt.io/archive/qt/5.12/5.12.12/single/qt-everywhere-src-5.12.12.tar.xz \
        || exit_failure "$0: failed to download ${DOWNLOAD_NAME}"
tar -xf qt-everywhere-src-5.12.12.tar.xz \
        || exit_failure "$0: failed to extract ${DOWNLOAD_NAME}"
mv qt-everywhere-src-5.12.12 qt5
rm qt-everywhere-src-5.12.12.tar.xz


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

OLD_EXTRA_CPPFLAGS=${EXTRA_CPPFLAGS}
# these are needed to build target qmake
export EXTRA_CPPFLAGS="-D_GNU_SOURCE -D__laylaos__ -D__${BUILD_ARCH}__ -fPIC"

myname=`uname -s`

if [ "$myname" == "LaylaOS" ]; then
    # apply this extra patch if we are building natively on LaylaOS
    cd ${DOWNLOAD_PORTS_PATH}
    patch -i ${CWD}/${PATCH_FILE_NATIVE_BUILD} -p0
    cd ${DOWNLOAD_PORTS_PATH}/qt5-build
    QT5_BUILD_PLATFORM=laylaos-g++
else
    QT5_BUILD_PLATFORM=linux-g++
fi

CPPFLAGS= CFLAGS= ../qt5/configure \
    -xplatform laylaos-g++ -platform ${QT5_BUILD_PLATFORM} \
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

# now to phase 2!
# annoyingly, Qt tools (qmake, rcc, uic, moc) are not cross-compiled, because
# they are host-tools meant to be used on the host machine.  however, this 
# means our target machine would have versions of these tools that are not
# going to run, because they are built for the host.  we need to go back and
# rebuild those tools, this time for the target machine

mkdir ${DOWNLOAD_PORTS_PATH}/qt5-build-hosttools
cd ${DOWNLOAD_PORTS_PATH}/qt5-build-hosttools

CPPFLAGS= CFLAGS= ../qt5/configure \
 -platform laylaos-g++ \
 -release -opensource -confirm-license \
 -no-iconv -no-inotify -no-gtk -no-eventfd -no-rpath -no-dbus -no-opengl -no-vulkan \
 -no-xcb-xlib -no-xcb-xinput -no-xkbcommon \
 -no-feature-alsa -no-feature-pulseaudio \
 -nomake examples -nomake tests -no-sql-{db2,ibase,mysql,oci,odbc,psql,sqlite,sqlite2,tds} \
 -external-hostbindir "${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-5.12/bin" \
 -qpa laylaos \
 -force-pkg-config \
 -prefix /usr/Qt-5.12-hosttools \
 -extprefix ${CROSSCOMPILE_SYSROOT_PATH}/usr/Qt-5.12-hosttools \
 -sysroot ${CROSSCOMPILE_SYSROOT_PATH} \
 -device-option CROSS_COMPILE="${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools" \
 -I "=/usr/local/include" \
 -I "${CXX_INCLUDE_PATH}" -I "${CXX_INCLUDE_PATH}/${BUILD_ARCH}-laylaos" \
 -D _GNU_SOURCE -D __laylaos__ -D __${BUILD_ARCH}__ \
        || exit_failure "$0: failed to configure ${DOWNLOAD_NAME} hosttools"

make module-qtbase-qmake_all || exit_failure "$0: failed to build ${DOWNLOAD_NAME} hosttools"
make -C qtbase sub-qmake-qmake-aux-pro-install_subtargets sub-src-qmake_all \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME} hosttools (1)"
make -C qtbase/src sub-moc-install_subtargets \
        sub-qlalr-install_subtargets \
        sub-rcc-install_subtargets \
        sub-uic-install_subtargets \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME} hosttools (2)"

# clean up
cd ${CWD}
rm -rf ${DOWNLOAD_PORTS_PATH}/qt5
rm -rf ${DOWNLOAD_PORTS_PATH}/qt5-build
rm -rf ${DOWNLOAD_PORTS_PATH}/qt5-build-hosttools
rm -rf ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools

export EXTRA_CPPFLAGS=${OLD_EXTRA_CPPFLAGS}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

