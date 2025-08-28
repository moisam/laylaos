#!/bin/bash

#
# Script to download and build mupdf
#

DOWNLOAD_NAME="mupdf"
DOWNLOAD_VERSION="1.26.2"
DOWNLOAD_URL="https://mupdf.com/downloads/archive/"
DOWNLOAD_PREFIX="mupdf-"
DOWNLOAD_SUFFIX="-source.tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
#DOWNLOAD_VERSION=""     # we will set this below
CWD=`pwd`

# where the downloaded and extracted source will end up
#DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/mupdf"
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_NAME}-${DOWNLOAD_VERSION}-source"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libmupdf.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

#cd ${DOWNLOAD_PORTS_PATH}
#git clone --recursive git://git.ghostscript.com/mupdf.git
#cd mupdf/
cd ${DOWNLOAD_SRCDIR}
rm -rf thirdparty/{curl,freetype,harfbuzz,libjpeg,openjpeg,jbig2dec,zlib}

# build
#cd ${DOWNLOAD_SRCDIR}

make HAVE_X11=no HAVE_GLUT=no prefix=/usr brotli=no \
    XCFLAGS="${CFLAGS} -mstackrealign -fPIC -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/openjpeg-2.5" \
    shared=yes build=release USE_SYSTEM_LIBS=yes USE_SYSTEM_GUMBO=no \
    || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make HAVE_X11=no HAVE_GLUT=no prefix=/usr brotli=no \
    XCFLAGS="$CFLAGS -mstackrealign -fPIC -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/openjpeg-2.5 -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/freetype2 -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/harfbuzz" \
    DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install \
    shared=yes build=release USE_SYSTEM_LIBS=yes USE_SYSTEM_GUMBO=no \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# get the version (this code is taken straight from mupdf's Makefile)
#VERSION_MINOR=($(grep "define FZ_VERSION_MINOR" ${DOWNLOAD_SRCDIR}/include/mupdf/fitz/version.h | cut -d ' ' -f 3))
#VERSION_PATCH=($(grep "define FZ_VERSION_PATCH" ${DOWNLOAD_SRCDIR}/include/mupdf/fitz/version.h | cut -d ' ' -f 3))
#DOWNLOAD_VERSION="${VERSION_MINOR}.${VERSION_PATCH}"

# Create a symlink
#ln -s libmupdf.so.${DOWNLOAD_VERSION} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libmupdf.so

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

