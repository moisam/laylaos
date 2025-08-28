#!/bin/bash

#
# Script to download and build man-db
#

DOWNLOAD_NAME="man-db"
DOWNLOAD_VERSION="2.13.0"
DOWNLOAD_URL="https://gitlab.com/man-db/man-db/-/archive/${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="man-db-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/man

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

sed -i -e "s/chown/sudo chown/g" src/Makefile.am
sed -i -e "s/chmod/sudo chmod/g" src/Makefile.am

./bootstrap

rm ${DOWNLOAD_SRCDIR}/build-aux/config.sub
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.sub

rm ${DOWNLOAD_SRCDIR}/build-aux/config.guess
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.guess

rm ${DOWNLOAD_SRCDIR}/gnulib/build-aux/config.sub
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/gnulib/build-aux/config.sub

rm ${DOWNLOAD_SRCDIR}/gnulib/build-aux/config.guess
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/gnulib/build-aux/config.guess

autoreconf

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

CFLAGS="$CFLAGS -mstackrealign" ../configure \
    --host=${BUILD_TARGET} --prefix=/usr \
    --sysconfdir=/etc --with-systemdtmpfilesdir=no \
    --with-systemdsystemunitdir=no --enable-automatic-create --disable-cats \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

# we don't have sudo yet on LaylaOS
myname=`uname -s`
if [ "$myname" == "LaylaOS" ]; then
    sed -i 's/sudo//g' src/Makefile
fi

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

