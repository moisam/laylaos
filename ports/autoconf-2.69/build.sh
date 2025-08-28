#!/bin/bash

#
# Script to download and build autoconf-2.69
#

DOWNLOAD_NAME="autoconf"
DOWNLOAD_VERSION="2.69"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/autoconf/"
DOWNLOAD_PREFIX="autoconf-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/ifnames-2.69

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

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

PERL=${HOST_PERL_FOR_CROSSCOMPILE} \
    ../configure  \
    --host=${BUILD_TARGET} --prefix=/usr --program-suffix=-2.69 \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# patch these suckers
sed -i "s~$HOST_PERL_FOR_CROSSCOMPILE~/usr/bin/perl~g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/ifnames-2.69
sed -i "s~$HOST_PERL_FOR_CROSSCOMPILE~/usr/bin/perl~g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/autoheader-2.69
sed -i "s~$HOST_PERL_FOR_CROSSCOMPILE~/usr/bin/perl~g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/autoreconf-2.69
sed -i "s~$HOST_PERL_FOR_CROSSCOMPILE~/usr/bin/perl~g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/autom4te-2.69
sed -i "s~$HOST_PERL_FOR_CROSSCOMPILE~/usr/bin/perl~g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/autoscan-2.69
sed -i "s~$HOST_PERL_FOR_CROSSCOMPILE~/usr/bin/perl~g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/autoupdate-2.69

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

