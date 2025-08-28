#!/bin/bash

#
# Script to download and build ncurses
#

DOWNLOAD_NAME="ncurses"
DOWNLOAD_VERSION="6.4"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/ncurses/"
DOWNLOAD_PREFIX="ncurses-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libncurses.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

mv ${DOWNLOAD_SRCDIR}/config.guess ${DOWNLOAD_SRCDIR}/config.guess.OLD
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config.guess

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

# build the lib without progs (tic, ...)

LD=${CROSSCOMPILE_TOOLS_PATH}/bin/${BUILD_ARCH}-laylaos-ld \
    CPPFLAGS="${CPPFLAGS} -I${CROSSCOMPILE_SYSROOT_PATH}/usr/include" \
    LDFLAGS="-L${CROSSCOMPILE_SYSROOT_PATH}/usr/lib" \
    ../configure  \
    --host=${BUILD_TARGET} --prefix=/usr --includedir=/usr/include/ncursesw \
    --with-termlib --with-ticlib --without-tests \
    --enable-xmc-glitch --without-ada --without-cxx-binding \
    --with-shared --without-pkg-config --enable-widec \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# make symlinks for those who need ncurses
for lib in form menu ncurses panel tinfo tic curses; do
    ln -s "lib${lib}w.so" "${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/lib${lib}.so"
done

# make symlink for those who need the old curses
ln -s "libncursesw.so" "${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libcurses.so"

if [ ! -e ${CROSSCOMPILE_SYSROOT_PATH}/usr/include/ncurses ]; then
    ln -s ncursesw ${CROSSCOMPILE_SYSROOT_PATH}/usr/include/ncurses
fi

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

