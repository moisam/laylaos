#!/bin/bash

#
# Script to download and bootstrap build the cross-compiler toolchain
#

DOWNLOAD_NAME="gcc"
DOWNLOAD_VERSION=${CROSS_GCC_VERSION}   # defined in build-toolchain.sh
DOWNLOAD_URL="https://ftp.gnu.org/gnu/gcc/gcc-${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="gcc-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

OLDCC=${CC}
myname=`uname -s`

if [ "$myname" == "LaylaOS" ]; then
    export CC=gcc
fi

# get common funcs
source ../common.sh

# check for an existing compile
if [ -d "${PREFIX}/bin" ]; then
    if [ -e "${PREFIX}/bin/${BUILD_ARCH}-laylabootstrap-gcc" ]; then
        echo " ==> GCC compiler seems to be compiled already"
        echo " ==> We will skip this step"
        echo " ==> If you think this is wrong or you need to re-compile GCC,"
        echo " ==> Remove ${PREFIX}/bin/${BUILD_ARCH}-laylabootstrap-* and re-run"
        echo " ==> this script again"
        exit 0
    fi
fi

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_PORTS_PATH}
patch -i ${CWD}/gcc.diff -p0
patch -i ${CWD}/gcc2.diff -p0
patch -i ${CWD}/gcc3.diff -p0
cd ${CWD}

cp extra/laylaos.h extra/laylaos.opt extra/laylaos-kernel.h extra/laylaos-user.h extra/laylaos-bootstrap.h ${DOWNLOAD_SRCDIR}/gcc/config

mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

mv ${DOWNLOAD_SRCDIR}/libtool.m4 ${DOWNLOAD_SRCDIR}/libtool.m4.OLD
mv ${DOWNLOAD_SRCDIR}/libgo/config/libtool.m4 ${DOWNLOAD_SRCDIR}/libgo/config/libtool.m4.OLD
mv ${DOWNLOAD_SRCDIR}/libphobos/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/libphobos/m4/libtool.m4.OLD

cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/libtool.m4
cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/libgo/config/libtool.m4
cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/libphobos/m4/libtool.m4

# build GCC
echo " ==>"
echo " ==> Building GCC"
echo " ==>"
mkdir -p ${DOWNLOAD_SRCDIR}/build-bootstrap
cd ${DOWNLOAD_SRCDIR}/build-bootstrap

${DOWNLOAD_SRCDIR}/configure --target=${BUILD_ARCH}-laylabootstrap --prefix="${PREFIX}" --disable-nls --enable-languages=c,c++ --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} --enable-shared --disable-threads

make all-gcc || exit_failure "$0: failed to build GCC"
make install-gcc || exit_failure "$0: failed to install GCC"

# Build and install libgcc
echo " ==>"
echo " ==> Building libgcc"
echo " ==>"
make all-target-libgcc || exit_failure "$0: failed to build libgcc"
make install-target-libgcc || exit_failure "$0: failed to install libgcc"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}/build-bootstrap

echo " ==>"
echo " ==> Finished building GCC"
echo " ==>"

# Remove the temporary headers
# We need to do this or otherwise MUSL will NOT overwrite our files with 
# the proper ones when we build libc
source ./bootstrap-remove-header-files.sh

export CC=${OLDCC}

