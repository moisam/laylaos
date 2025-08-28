#!/bin/bash

#
# Script to download and build SDLBook
#

DOWNLOAD_NAME="SDLBook"
DOWNLOAD_VERSION="1.0.0"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/SDLBook"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/sdlbook

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths

cd ${DOWNLOAD_PORTS_PATH}
git clone https://github.com/rofl0r/SDLBook.git

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

# NOTE: The repo was updated, making our patch obsolete.
#       As the changes are small, it is easier to make them by hand and
#       avoid re-creating the patch and keeping track of the repo's updates.

#cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0
sed -i 's~SDL_WINDOW_OPENGL|~/* SDL_WINDOW_OPENGL| */ ~' ${DOWNLOAD_SRCDIR}/ezsdl.h
sed -i 's~SDL_RENDERER_ACCELERATED|~/* SDL_RENDERER_ACCELERATED| */ ~' ${DOWNLOAD_SRCDIR}/ezsdl.h

# build
cd ${DOWNLOAD_SRCDIR}

make CPPFLAGS="-D__laylaos__ -D__${ARCH}__" CFLAGS="-I${CXX_INCLUDE_PATH} -O2" SDL2=1 \
    || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make prefix=/usr DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

