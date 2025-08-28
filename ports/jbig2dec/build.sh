#!/bin/bash

#
# Script to download and build jbig2dec
#

DOWNLOAD_NAME="jbig2dec"
DOWNLOAD_VERSION="0.20"
DOWNLOAD_URL="https://github.com/ArtifexSoftware/jbig2dec/archive/refs/tags/"
DOWNLOAD_PREFIX=""
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_NAME}-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libjbig2dec.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

cd ${DOWNLOAD_SRCDIR}
./autogen.sh 

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

mv ${DOWNLOAD_SRCDIR}/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/m4/libtool.m4.OLD
cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/m4/libtool.m4

if [ -e ./config.status ]; then
    # The stupid ./autogen script automatically runs ./configure, but that's no
    # good as we need it to generate config files so that we can patch them.
    # We can either patch ./autogen and risk it being changed upstream at some
    # point and break our patch, or just delete the config files (what a waste)
    # and re-run ./configure ourselves below.
    make distclean
else
    autoreconf
fi

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

CFLAGS="-mstackrealign" ../configure \
    --host=${BUILD_TARGET} --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    --prefix=/usr \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

