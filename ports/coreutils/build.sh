#!/bin/bash

#
# Script to download and build coreutils
#

DOWNLOAD_NAME="coreutils"
DOWNLOAD_VERSION="8.32"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/coreutils/"
DOWNLOAD_PREFIX="coreutils-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/info/coreutils.info

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

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

../configure \
    --host=${BUILD_TARGET} --prefix=/usr \
    CPPFLAGS="${CPPFLAGS} -D_POSIX_TIMERS -DHAVE_LSTAT -DHAVE_MKNOD -DHAVE_OPENDIR -DHAVE_READDIR -DHAVE_REWINDDIR -DHAVE_CLOSEDIR -DHAVE_SCANDIR -DHAVE_OPENAT_SUPPORT -D__USE_MISC -D_POSIX_PRIORITY_SCHEDULING" \
    CFLAGS="--sysroot=${CROSSCOMPILE_SYSROOT_PATH}/ -isystem=/usr/include" \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

