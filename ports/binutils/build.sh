#!/bin/bash

#
# Script to download and build binutils
#

DOWNLOAD_NAME="binutils"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/binutils/"
DOWNLOAD_VERSION="2.38"
DOWNLOAD_PREFIX="binutils-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/ar

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
patch -i ${CWD}/../toolchain/binutils.diff -p0
patch -i ${CWD}/../toolchain/binutils2.diff -p0
patch -i ${CWD}/../toolchain/binutils3.diff -p0
cd ${CWD}

cp ${CWD}/../toolchain/extra/elf_i386_laylaos.sh ${CWD}/../toolchain/extra/elf_x86_64_laylaos.sh ${DOWNLOAD_SRCDIR}/ld/emulparams

mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

mv ${DOWNLOAD_SRCDIR}/libtool.m4 ${DOWNLOAD_SRCDIR}/libtool.m4.OLD
cp ../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/libtool.m4

# build binutils
echo " ==>"
echo " ==> Building ported binutils"
echo " ==>"
mkdir -p ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

${DOWNLOAD_SRCDIR}/configure --host=${BUILD_TARGET} \
    --with-build-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    --enable-host-shared=yes --enable-shared=yes \
    || exit_failure "$0: failed to configure ported ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ported ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ported ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ported ${DOWNLOAD_NAME}"
echo " ==>"

