#!/bin/bash

#
# Script to download and build libtool
#

DOWNLOAD_NAME="libtool"
DOWNLOAD_VERSION="2.5.4"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/libtool/"
DOWNLOAD_PREFIX="libtool-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/libtool

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

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

../configure  \
    --host=${BUILD_TARGET} --prefix=/usr \
    --enable-shared --enable-ltdl-install \
    --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# remove absolute paths from the installed libtool
sed -i "s~${CROSSCOMPILE_TOOLS_PATH}/bin/x86_64-laylaos-~/usr/bin/~g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/libtool
sed -i "s~${CROSSCOMPILE_TOOLS_PATH}/x86_64-laylaos~/usr~g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/libtool

sed -i 's~LTCFLAGS=".*"~LTCFLAGS=""~' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/libtool
sed -i 's~sys_lib_search_path_spec=".*"~sys_lib_search_path_spec=""~' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/libtool

sed -i 's~compiler_lib_search_dirs=".*"~compiler_lib_search_dirs=""~' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/libtool
sed -i 's~compiler_lib_search_path=".*"~compiler_lib_search_path=""~' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/libtool
sed -i 's~predep_objects=".*"~predep_objects=""~' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/libtool
sed -i 's~postdep_objects=".*"~postdep_objects=""~' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/libtool
sed -i 's~postdeps=".*"~postdeps=""~' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/libtool

sed -i 's~lt_sysroot=.*~lt_sysroot=/~' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/libtool

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

