#!/bin/bash

#
# Copyright 2021-2024 (c) Mohammed Isam
#
# This script is part of LaylaOS
#
# Script to download and build LaylaOS from source.
# It also downloads and builds all the necessary ports, including the 
# crosscompiler toolchain.
#

# libs with no dependencies -- a.k.a. easy wins
NODEPS_LIBS="zlib xz libiconv libexpat libffi libucontext fribidi gzip"

# image, multimedia and font libs
IMAGE_LIBS="libjpeg libpng16 libtiff libwebp"
MULTIMEDIA_LIBS="aomedia dav1d libavif libdvdread faad2 twolame liba52 libogg"
MULTIMEDIA_LIBS="${MULTIMEDIA_LIBS} flac vorbis libcaca"

FONT_LIBS="freetype fontconfig harfbuzz"
SDL_LIBS="SDL2 SDL2_mixer SDL2_image"

MULTIMEDIA_LIBS2="libtheora libass mplayer ffmpeg"
TERMINAL_APPS_AND_LIBS="ncurses coreutils bash inetutils nano openssl links curl cups"
GAMES_APPS="openttd sdl2-doom"

#save these for later
OLDPREFIX=$PREFIX
OLDEXEC_PREFIX=$EXEC_PREFIX
OLDBOOTDIR=$BOOTDIR
OLDLIBDIR=$LIBDIR
OLDINCLUDEDIR=$INCLUDEDIR
OLDSYSROOT=$SYSROOT

# build 64bit OS by default
ARCH=x86_64

CWD=`pwd`

# download 3rd party packages to ./build/ports by default
PORTSDIR=${CWD}/build/ports

# build the cross-compiler in ./build/cross by default
CROSS=${CWD}/build/cross

# use ./build/sysroot as the cross sysroot by default
SYSROOT=${CWD}/build/sysroot

# now process arguments to see if the user wants to change anything
i=1
j=$#
BUILD_TOOLCHAIN=yes

while [ $i -le $j ]
do
    case $1 in
        x86-64 | x86_64)
            ARCH=x86_64
            ;;
        i686)
            ARCH=i686
            ;;
        skip-toolchain)
            BUILD_TOOLCHAIN=no
            ;;
    esac
    
    i=$((i + 1))
    shift 1
done

# make sure the directories exist
mkdir -p ${PORTSDIR}
mkdir -p ${CROSS}
mkdir -p ${SYSROOT}

# export vars
export BUILD_ARCH=${ARCH}
export BUILD_TARGET=${ARCH}-laylaos
export DOWNLOAD_PORTS_PATH=${PORTSDIR}
export CROSSCOMPILE_TOOLS_PATH=${CROSS}
export CROSSCOMPILE_SYSROOT_PATH=${SYSROOT}

# some scripts expect this var
export SYSROOT=${SYSROOT}

##############################################
# helper functions
##############################################

build_list()
{
    for lib in ${1}; do
        cd ../ports/$lib
        chmod +x ./build.sh     # ensure it is executable
        ./build.sh || exit 1
        cd ${CWD}
    done
}

##############################################
# build the cross-compiler toolchain
##############################################

if [ "x${BUILD_TOOLCHAIN}" == "xyes" ]; then
    cd ../ports/toolchain
    ./build-toolchain.sh
    cd ${CWD}
fi

##############################################
# build the kernel
##############################################

export KERNEL_AR=${CROSS}/bin/${ARCH}-laylakernel-ar
export KERNEL_AS=${CROSS}/bin/${ARCH}-laylakernel-as
export KERNEL_CC="${CROSS}/bin/${ARCH}-laylakernel-gcc --sysroot=\"${SYSROOT}\" -isystem=${INCLUDEDIR}"
export USERLAND_AR=${CROSS}/bin/${ARCH}-laylaos-ar
export USERLAND_AS=${CROSS}/bin/${ARCH}-laylaos-as
export USERLAND_CC="${CROSS}/bin/${ARCH}-laylaos-gcc --sysroot=\"${SYSROOT}\" -isystem=${INCLUDEDIR}"

export PREFIX=/usr
export EXEC_PREFIX=${PREFIX}
export BOOTDIR=/boot
export LIBDIR=${EXEC_PREFIX}/lib
export INCLUDEDIR=${PREFIX}/include

case "${ARCH}" in
    x86_64)
        export KERNEL_CFLAGS="-O2 -Wall -Wextra -pedantic -g -D__laylaos__ -D__POSIX_VISIBLE -D__x86_64__ -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2"
        export USERLAND_CFLAGS="-O2 -Wall -Wextra -pedantic -g -D__laylaos__ -D__POSIX_VISIBLE -D__x86_64__ -mcmodel=large"
        ;;
    i686)
        export KERNEL_CFLAGS="-O2 -Wall -Wextra -pedantic -g -D__laylaos__ -D__POSIX_VISIBLE"
        export USERLAND_CFLAGS="-O2 -Wall -Wextra -pedantic -g -D__laylaos__ -D__POSIX_VISIBLE"
        ;;
    *)
        echo "$0: Invalid architecture: ${ARCH}"
        exit 1
        ;;
esac

KERNEL_HEADER_PROJECTS="libk kernel"
KERNEL_PROJECTS="libk kernel"

for PROJECT in ${KERNEL_HEADER_PROJECTS}; do
    echo " ==>"
    echo " ==> installing kernel headers for: ${PROJECT}"
    echo " ==>"
	(cd ${CWD}/../${PROJECT} && HOST="${ARCH}" DESTDIR="${SYSROOT}" make install-headers)
done

for PROJECT in ${KERNEL_PROJECTS}; do
    echo " ==>"
    echo " ==> building ${PROJECT}"
    echo " ==>"
	(cd ${CWD}/../${PROJECT} && HOST="${ARCH}" DESTDIR="${SYSROOT}" make install)
done

##############################################
# set important env variables for the
# upcoming compilations
##############################################

export PREFIX=${SYSROOT}/usr
export CC=${CROSS}/bin/${ARCH}-laylaos-gcc
export CXX=${CROSS}/bin/${ARCH}-laylaos-g++
export AS=${CROSS}/bin/${ARCH}-laylaos-as
export AR=${CROSS}/bin/${ARCH}-laylaos-ar
export STRIP=${CROSS}/bin/${ARCH}-laylaos-strip
export RANLIB=${CROSS}/bin/${ARCH}-laylaos-ranlib
export NM=${CROSS}/bin/${ARCH}-laylaos-nm
export OBJDUMP=${CROSS}/bin/${ARCH}-laylaos-objdump

export PKG_CONFIG=${CWD}/laylaos-pkg-config
export PKG_CONFIG_FOR_BUILD=pkg-config
export PKGCONFIG=${PKG_CONFIG}

export CPPFLAGS="-D__laylaos__ -D__${ARCH}__"
export CXX_INCLUDE_PATH=${CROSS}/${ARCH}-laylaos/include/c++/11.3.0

# create a cross-compile file for packages that need meson
source ./make-meson-crossfile.sh

##############################################
# build libs with no dependencies.
# these are often required by other libs
# down the road.
##############################################

build_list "${NODEPS_LIBS}"

##############################################
# build image/graphics, multimedia, font libs
##############################################

build_list "${IMAGE_LIBS}"
build_list "${MULTIMEDIA_LIBS}"
build_list "${FONT_LIBS}"

##############################################
# build our core bins and gui apps
# we need to build libgui as SDL2 and other
# video libs depend on it
##############################################

echo " ==>"
echo " ==> building bins and gui apps"
echo " ==>"
(cd ${CWD}/../kernel/bin && HOST="${ARCH}" SYSROOT="${SYSROOT}" CC=${USERLAND_CC} AR=${USERLAND_AR} AS=${USERLAND_AS} make)

if [ $? -ne 0 ]; then
    echo "$0: failed to build bins"
    exit 1
fi

#echo " ==> removing object files"
#KERNEL_BIN_OBJS=`find ${CWD}/../kernel/bin/ -name '*.o'`
#rm ${KERNEL_BIN_OBJS}

echo " ==> creating desktop dirs"
mkdir -p ${SYSROOT}/usr/bin/desktop
mkdir -p ${SYSROOT}/usr/bin/widgets
mkdir -p ${SYSROOT}/usr/sbin

cd ${CWD}/../kernel/bin/

echo " ==> installing bins and gui apps"
KERNEL_BINS=`find . -type f -executable ! '(' -name '*.a' -o -name '*.so' -o -name '*.o' ')'`

#cp ${KERNEL_BINS} ${SYSROOT}/usr/bin/

for f in ${KERNEL_BINS}; do
    # desktop bins go under usr/bin/desktop/
    if [[ "${f}" == *"desktop"* ]]; then
        echo "     Copying ${f} to usr/bin/desktop/"
        cp ${f} ${SYSROOT}/usr/bin/desktop/
    else
        echo "     Copying ${f} to usr/bin/"
        cp ${f} ${SYSROOT}/usr/bin/
    fi
done

# reboot needs to be under sbin/
mv ${SYSROOT}/usr/bin/reboot ${SYSROOT}/usr/sbin/reboot

echo " ==> installing desktop widgets"
cd desktop/widgets/

for f in `ls *.so`; do
    cp ${f} ${SYSROOT}/usr/bin/widgets/
done

echo " ==>"
echo " ==> creating vdso"
echo " ==>"
(cd ${CWD}/../vdso && HOST="${ARCH}" SYSROOT="${SYSROOT}" CC=${USERLAND_CC} AR=${USERLAND_AR} AS=${USERLAND_AS} make)

if [ $? -ne 0 ]; then
    echo "$0: failed to create vdso"
    exit 1
fi

cd ${CWD}

##############################################
# NOW we can build SDL2
##############################################

build_list "${SDL_LIBS}"

##############################################
# now multimedia libs that depend on other
# stuff, e.g. SDL2
##############################################

build_list "${MULTIMEDIA_LIBS2}"

##############################################
# other apps and games
##############################################

build_list "${TERMINAL_APPS_AND_LIBS}"
build_list "${GAMES_APPS}"

##############################################
# SwiftShader
##############################################

build_list "SwiftShader"
build_list "Vulkan-Headers"

##############################################
# Qt5
##############################################

build_list "glib"
build_list "opus"
build_list "gstreamer gst-plugins-base"
build_list "Qt5"

##############################################
# Done!
##############################################

echo
echo "  LaylaOS has been built under ${SYSROOT}/"
echo
echo "  To create a bootable disk image, run './create_bootable_disk.sh help'"
echo

##############################################
# Restore these
##############################################
export PREFIX=$OLDPREFIX
export EXEC_PREFIX=$OLDEXEC_PREFIX
export BOOTDIR=$OLDBOOTDIR
export LIBDIR=$OLDLIBDIR
export INCLUDEDIR=$OLDINCLUDEDIR
export SYSROOT=$OLDSYSROOT


