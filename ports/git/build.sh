#!/bin/bash

#
# Script to download and build git
#

DOWNLOAD_NAME="git"
DOWNLOAD_VERSION="2.46.2"
DOWNLOAD_URL="https://www.kernel.org/pub/software/scm/git/"
DOWNLOAD_PREFIX="git-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/git

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

# build
cd ${DOWNLOAD_SRCDIR}

./configure  \
    --host=${BUILD_TARGET} --prefix=/usr --with-editor=nano \
    NO_PYTHON='YesPlease' NO_TCLTK='YesPlease' \
    ac_cv_iconv_omits_bom='no' ac_cv_fread_reads_directories='no' \
    ac_cv_snprintf_returns_bogus='no' \
    CURL_CONFIG=${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/curl-config \
    CPPFLAGS="${CPPFLAGS} --sysroot=${CROSSCOMPILE_SYSROOT_PATH} -isystem=/usr/include -D_GNU_SOURCE -DNO_IPV6" \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make CURL_LDFLAGS='-lcurl -lssl -lcrypto -lz' || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

