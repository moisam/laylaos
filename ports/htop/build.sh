#!/bin/bash

#
# Script to download and build htop
#

DOWNLOAD_NAME="htop"
DOWNLOAD_VERSION="3.3.0"
DOWNLOAD_URL="https://github.com/htop-dev/htop/archive/refs/tags/"
DOWNLOAD_PREFIX=""
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/htop-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/htop

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

cd ${DOWNLOAD_SRCDIR}
./autogen.sh

mv build-aux/config.sub build-aux/config.sub.OLD
cp ${CWD}/../config.sub.laylaos build-aux/config.sub

mv build-aux/config.guess build-aux/config.guess.OLD
cp ${CWD}/../config.guess.laylaos build-aux/config.guess

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

../configure \
    --host=${BUILD_ARCH}-linux --prefix=/usr --disable-largefile --disable-unicode \
    CFLAGS="${CFLAGS} -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/ncurses" \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

# disable -rdynamic flag as it does not work on LaylaOS
myname=`uname -s`
if [ "$myname" == "LaylaOS" ]; then
    sed -i 's/-rdynamic//g' Makefile
fi

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

