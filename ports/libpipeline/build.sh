#!/bin/bash

#
# Script to download and build libpipeline
#

DOWNLOAD_NAME="libpipeline"
DOWNLOAD_VERSION="1.5.8"
DOWNLOAD_URL="https://gitlab.com/libpipeline/libpipeline/-/archive/${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="libpipeline-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libpipeline.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_SRCDIR}
./bootstrap

rm build-aux/config.sub 
cp ${CWD}/../config.sub.laylaos build-aux/config.sub

rm build-aux/config.guess
cp ${CWD}/../config.guess.laylaos build-aux/config.guess

rm gnulib/build-aux/config.sub 
cp ${CWD}/../config.sub.laylaos gnulib/build-aux/config.sub

rm gnulib/build-aux/config.guess
cp ${CWD}/../config.guess.laylaos gnulib/build-aux/config.guess

myname=`uname -s`
if [ "$myname" != "LaylaOS" ]; then
    cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}
fi

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

CFLAGS="$CFLAGS -mstackrealign" ../configure \
    --host=${BUILD_TARGET} --prefix=/usr --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

