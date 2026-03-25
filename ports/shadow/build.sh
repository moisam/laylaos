#!/bin/bash

#
# Script to download and build shadow
#

DOWNLOAD_NAME="shadow"
DOWNLOAD_VERSION="4.19.2"
DOWNLOAD_URL="https://github.com/shadow-maint/shadow/releases/download/${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="shadow-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/sbin/useradd

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/build-aux/config.sub ${DOWNLOAD_SRCDIR}/build-aux/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.sub

mv ${DOWNLOAD_SRCDIR}/build-aux/config.guess ${DOWNLOAD_SRCDIR}/build-aux/config.guess.OLD
cp ../config.guess.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.guess

mv ${DOWNLOAD_SRCDIR}/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/m4/libtool.m4.OLD
cp ../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/m4/libtool.m4

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

../configure \
    --host=${BUILD_TARGET} --prefix=/usr --sysconfdir=/etc \
    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    --disable-nls --disable-subordinate-ids --disable-logind \
    --with-libpam=no --with-selinux=no --with-audit=no \
    --with-btrfs=no --with-libbsd=no --with-su=yes \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Comment the login timeout line
#sed -i 's/LOGIN_TIMEOUT/#LOGIN_TIMEOUT/' ${CROSSCOMPILE_SYSROOT_PATH}/etc/login.defs 

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

