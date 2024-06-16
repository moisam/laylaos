#!/bin/bash

#
# Script to download and bootstrap build the cross-compiler toolchain
#

DOWNLOAD_NAME="binutils"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/binutils/"
DOWNLOAD_VERSION=${CROSS_BINUTILS_VERSION}  # defined in build-toolchain.sh
DOWNLOAD_PREFIX="binutils-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
if [ -d "${PREFIX}/bin" ]; then
    if [ -e "${PREFIX}/bin/${BUILD_ARCH}-laylabootstrap-ar" ]; then
        echo " ==> Binutils seems to be compiled already"
        echo " ==> We will skip this step"
        echo " ==> If you think this is wrong or you need to re-compile binutils,"
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
cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/binutils.diff -p0 && cd ${CWD}
cp extra/elf_i386_laylaos.sh extra/elf_x86_64_laylaos.sh ${DOWNLOAD_SRCDIR}/ld/emulparams

# bootstrap headers
echo " ==> Bootstrapping headers"
source ./bootstrap-header-dir.sh

# build binutils
echo " ==>"
echo " ==> Building binutils"
echo " ==>"
mkdir -p ${DOWNLOAD_SRCDIR}/build-bootstrap
cd ${DOWNLOAD_SRCDIR}/build-bootstrap

${DOWNLOAD_SRCDIR}/configure --target=${BUILD_ARCH}-laylabootstrap --prefix="${PREFIX}" --disable-nls --disable-werror --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} --enable-shared

make || exit_failure "$0: failed to build binutils"
make install || exit_failure "$0: failed to install binutils"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}/build-bootstrap

echo " ==>"
echo " ==> Finished building binutils"
echo " ==>"

