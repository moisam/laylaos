#!/bin/bash

#
# Script to download and build DjVuLibre
#

DOWNLOAD_NAME="DjVuLibre"
DOWNLOAD_VERSION="3.5.27"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

export CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -mstackrealign"
export CXXFLAGS="-mstackrealign -fPIC"

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/DjVuLibre"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libdjvulibre.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths

cd ${DOWNLOAD_PORTS_PATH}
git clone https://github.com/DjvuNet/DjVuLibre.git
cd DjVuLibre/
NOCONFIGURE=1 ./autogen.sh 

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

mv ${DOWNLOAD_SRCDIR}/config/config.sub ${DOWNLOAD_SRCDIR}/config/config.sub.OLD
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config/config.sub

mv ${DOWNLOAD_SRCDIR}/config/config.guess ${DOWNLOAD_SRCDIR}/config/config.guess.OLD
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/config/config.guess

mv ${DOWNLOAD_SRCDIR}/config/libtool.m4 ${DOWNLOAD_SRCDIR}/config/libtool.m4.OLD
cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/config/libtool.m4

cd ${DOWNLOAD_SRCDIR}
autoreconf

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

../configure --host=${BUILD_TARGET} --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    --disable-desktopfiles --prefix=/usr \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix usr/local/include/libdjvu/ddjvuapi.h
cd ${CROSSCOMPILE_SYSROOT_PATH}/usr && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# Fix libdjvulibre.la for the future generations
sed -i "s/dependency_libs=.*/dependency_libs='-ljpeg -lstdc++'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libdjvulibre.la

rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

