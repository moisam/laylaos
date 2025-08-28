#!/bin/bash

#
# Script to download and build nano
#

DOWNLOAD_NAME="nano"
DOWNLOAD_VERSION="7.2"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/nano"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/nano

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths

cd ${DOWNLOAD_PORTS_PATH}
git clone https://https.git.savannah.gnu.org/git/nano.git

cd nano || exit_failure "$0: failed to download ${DOWNLOAD_NAME}"

# git protocol is not supported in LaylaOS yet, so fix the URL used in autogen.sh
sed -i "s~gnulib_url=.*~gnulib_url=\"https://git.savannah.gnu.org/git/gnulib.git\"~" ./autogen.sh
./autogen.sh

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv config.sub config.sub.OLD
cp ${CWD}/../config.sub.laylaos config.sub

mv config.guess config.guess.OLD
cp ${CWD}/../config.guess.laylaos config.guess

mv gnulib/build-aux/config.sub gnulib/build-aux/config.sub.OLD
cp ${CWD}/../config.sub.laylaos gnulib/build-aux/config.sub

mv gnulib/build-aux/config.guess gnulib/build-aux/config.guess.OLD
cp ${CWD}/../config.guess.laylaos gnulib/build-aux/config.guess

# build
enable_utf8=no CURSES_LIB_NAME=ncurses \
    CPPFLAGS="${CPPFLAGS} -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include/ncurses" \
    LDFLAGS="-L${CROSSCOMPILE_SYSROOT_PATH}/usr/lib" \
    LIBS="-lncurses -ltinfo" \
    ./configure \
    --host=${BUILD_TARGET} --prefix=/usr \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

