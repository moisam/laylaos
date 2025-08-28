#!/bin/bash

#
# Script to download and build automake-1.16.5
# This particular version is needed by some ported packages, e.g. caca and grep
#

DOWNLOAD_NAME="automake"
DOWNLOAD_VERSION="1.16.5"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/automake/"
DOWNLOAD_PREFIX="automake-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/automake-1.16

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/lib/config.sub ${DOWNLOAD_SRCDIR}/lib/config.sub.OLD
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/lib/config.sub

mv ${DOWNLOAD_SRCDIR}/lib/config.guess ${DOWNLOAD_SRCDIR}/lib/config.guess.OLD
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/lib/config.guess

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

PERL=${HOST_PERL_FOR_CROSSCOMPILE} \
    ../configure  \
    --host=${BUILD_TARGET} --prefix=/usr \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

#    --host=${BUILD_TARGET} --prefix=/usr --program-suffix=-1.16

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# patch these suckers
sed -i '1 s/^.*$/#! \/usr\/bin\/perl/' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/aclocal-1.16
sed -i '1 s/^.*$/#! \/usr\/bin\/perl/' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/aclocal
sed -i '1 s/^.*$/#! \/usr\/bin\/perl/' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/automake-1.16
sed -i '1 s/^.*$/#! \/usr\/bin\/perl/' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/automake

# those are not needed
#rm ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/aclocal-1.16-1.16
#rm ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/automake-1.16-1.16 
#rm ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/man/man1/aclocal-1.16-1.16
#rm ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/man/man1/automake-1.16-1.16

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

