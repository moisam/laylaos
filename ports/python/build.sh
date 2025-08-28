#!/bin/bash

#
# Script to download and build python
#

DOWNLOAD_NAME="python"
DOWNLOAD_VERSION="3.10.16"
DOWNLOAD_URL="https://www.python.org/ftp/python/${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="Python-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/python

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

myname=`uname -s`
if [ "$myname" == "LaylaOS" ]; then
    PYTHON_BUILD_SYSTEM=${BUILD_ARCH}-laylaos
else
    PYTHON_BUILD_SYSTEM=${BUILD_ARCH}-pc-linux-gnu
fi

CPPFLAGS="${CPPFLAGS} -mstackrealign -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/ncurses" \
    READELF=${CROSSCOMPILE_TOOLS_PATH}/bin/${ARCH}-laylaos-readelf \
    ../configure \
    --host=${BUILD_TARGET} --target=${BUILD_TARGET} \
    --build=${PYTHON_BUILD_SYSTEM} \
    --disable-ipv6 --enable-shared \
    --with-system-expat --with-system-ffi \
    --prefix=/usr --with-pymalloc=no \
    ac_cv_file__dev_ptmx=yes ac_cv_file__dev_ptc=no \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

sed -i "s~#define HAVE_UTIMENSAT 1~//#define HAVE_UTIMENSAT 1~" ./pyconfig.h

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

ln -s python3 ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/python

# cleanup
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

