#!/bin/bash

#
# Script to download and build inetutils
#

DOWNLOAD_NAME="inetutils"
DOWNLOAD_VERSION="2.2"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/inetutils/"
DOWNLOAD_PREFIX="inetutils-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/ping

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
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.sub

mv ${DOWNLOAD_SRCDIR}/build-aux/config.guess ${DOWNLOAD_SRCDIR}/build-aux/config.guess.OLD
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.guess

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

../configure \
    CPPFLAGS="${CPPFLAGS} -D__USE_XOPEN -D__USE_MISC -DHAVE_STRUCT_SOCKADDR_STORAGE -DHAVE_STRUCT_ADDRINFO -DPATH_PROCNET_DEV='\"/proc/net/dev\"'" \
    LDFLAGS="-L${CROSSCOMPILE_SYSROOT_PATH}/usr/lib" \
    --host=${BUILD_TARGET} --prefix=/usr \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix exec bit for these bins
chmod +x ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/ping
chmod +x ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/ping6
chmod +x ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/traceroute

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

