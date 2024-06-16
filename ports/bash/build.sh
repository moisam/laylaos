#!/bin/bash

#
# Script to download and build bash
#

DOWNLOAD_NAME="bash"
DOWNLOAD_VERSION="4.4.18"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/bash/"
DOWNLOAD_PREFIX="bash-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/bash

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/support/config.sub ${DOWNLOAD_SRCDIR}/support/config.sub.OLD
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/support/config.sub

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

../configure cross_compiling=yes \
    --host=${BUILD_TARGET} --prefix=/usr \
    --enable-alias --enable-bang-history --enable-brace-expansion \
    --enable-command-timing --enable-coprocesses --enable-directory-stack \
    --enable-function-import --enable-help-builtin --enable-history \
    --enable-job-control --enable-multibyte --enable-prompt-string-decoding \
    --enable-readline --enable-select --with-bash-malloc=no \
    CPPFLAGS="${CPPFLAGS} -D_POSIX_VERSION -DNEED_EXTERN_PC" \
    LDFLAGS="-L${CROSSCOMPILE_SYSROOT_PATH}/usr/lib" \
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

