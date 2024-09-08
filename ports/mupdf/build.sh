#!/bin/bash

#
# Script to download and build mupdf
#

DOWNLOAD_NAME="mupdf"
DOWNLOAD_VERSION="25.0"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/mupdf"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libmupdf.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths

cd ${DOWNLOAD_PORTS_PATH}
git clone --recursive git://git.ghostscript.com/mupdf.git
cd mupdf/
rm -rf thirdparty/{curl,freetype,harfbuzz,libjpeg,openjpeg,jbig2dec,zlib}

# build
cd ${DOWNLOAD_SRCDIR}

make HAVE_X11=no HAVE_GLUT=no prefix=/usr \
    XCFLAGS="${CFLAGS} -mstackrealign -fPIC -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/openjpeg-2.5" \
    shared=yes build=release USE_SYSTEM_LIBS=yes USE_SYSTEM_GUMBO=no \
    || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make HAVE_X11=no HAVE_GLUT=no prefix=/usr \
    XCFLAGS="$CFLAGS -mstackrealign -fPIC -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/openjpeg-2.5 -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/freetype2 -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/harfbuzz" \
    DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install \
    shared=yes build=release USE_SYSTEM_LIBS=yes USE_SYSTEM_GUMBO=no \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Create a symlink
ln -s libmupdf.so.${DOWNLOAD_VERSION} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libmupdf.so

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

