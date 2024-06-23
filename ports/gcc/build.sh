#!/bin/bash

#
# Script to download and build gcc
#

DOWNLOAD_NAME="gcc"
DOWNLOAD_VERSION="11.3.0"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/gcc/gcc-${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="gcc-"
DOWNLOAD_SUFFIX=".tar.xz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/gcc

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/../toolchain/gcc.diff -p0 && cd ${CWD}

cp ${CWD}/../toolchain/extra/laylaos.h \
   ${CWD}/../toolchain/extra/laylaos.opt \
   ${CWD}/../toolchain/extra/laylaos-kernel.h \
   ${CWD}/../toolchain/extra/laylaos-user.h \
   ${CWD}/../toolchain/extra/laylaos-bootstrap.h ${DOWNLOAD_SRCDIR}/gcc/config

mv ${DOWNLOAD_SRCDIR}/config.sub ${DOWNLOAD_SRCDIR}/config.sub.OLD
cp ../config.sub.laylaos ${DOWNLOAD_SRCDIR}/config.sub

mv ${DOWNLOAD_SRCDIR}/libtool.m4 ${DOWNLOAD_SRCDIR}/libtool.m4.OLD
cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/libtool.m4

mv ${DOWNLOAD_SRCDIR}/libgo/config/libtool.m4 ${DOWNLOAD_SRCDIR}/libgo/config/libtool.m4.OLD
cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/libgo/config/libtool.m4

mv ${DOWNLOAD_SRCDIR}/libphobos/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/libphobos/m4/libtool.m4.OLD
cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/libphobos/m4/libtool.m4


# build GCC
echo " ==>"
echo " ==> Building ported GCC"
echo " ==>"
mkdir -p ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

OLD_CXXFLAGS=$CXXFLAGS
OLD_CC=$CC_FOR_TARGET
OLD_CXX=$CXX_FOR_TARGET
OLD_AR=$AR_FOR_TARGET
OLD_AS=$AS_FOR_TARGET
OLD_NM=$NM_FOR_TARGET
OLD_RANLIB=$RANLIB_FOR_TARGET
OLD_STRIP=$STRIP_FOR_TARGET
OLD_OBJDUMP=$OBJDUMP_FOR_TARGET

CXXFLAGS="-I${CXX_INCLUDE_PATH} -I${CXX_INCLUDE_PATH}/${BUILD_ARCH}-laylaos"

export CXXFLAGS="-I${CXX_INCLUDE_PATH} -I${CXX_INCLUDE_PATH}/${BUILD_ARCH}-laylaos"
export CC_FOR_TARGET=$CC
export CXX_FOR_TARGET=$CXX
export AR_FOR_TARGET=$AR
export AS_FOR_TARGET=$AS
export NM_FOR_TARGET=$NM
export RANLIB_FOR_TARGET=$RANLIB
export STRIP_FOR_TARGET=$STRIP
export OBJDUMP_FOR_TARGET=$OBJDUMP

${DOWNLOAD_SRCDIR}/configure --host=${BUILD_TARGET} \
    --enable-languages=c,c++ \
    --with-build-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    --enable-shared --enable-host-shared \
    --enable-threads=yes --enable-libssp --disable-multilib \
    CPPFLAGS="-D__laylaos__ -D__${BUILD_ARCH}__" \
    CXXFLAGS="-I${CXX_INCLUDE_PATH} -I${CXX_INCLUDE_PATH}/${BUILD_ARCH}-laylaos" \
    || exit_failure "$0: failed to configure ported ${DOWNLOAD_NAME}"

make all-gcc || exit_failure "$0: failed to build ported GCC"
make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install-gcc || exit_failure "$0: failed to install ported GCC"


# move the ligbcc we created earlier when we cross-compiled gcc
mv ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libgcc_s.so.1 ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libgcc_s.so.1.CROSS


# Build and install libgcc
echo " ==>"
echo " ==> Building ported libgcc"
echo " ==>"

make all-target-libgcc CFLAGS_FOR_TARGET='-D_POSIX_THREADS -g -O2 -mcmodel=large -mstackrealign' || exit_failure "$0: failed to build ported libgcc"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install-target-libgcc || exit_failure "$0: failed to install ported libgcc"

# Build and install libssp
echo " ==>"
echo " ==> Building ported libssp"
echo " ==>"

make all-target-libssp CFLAGS_FOR_TARGET='-D_POSIX_THREADS -g -O2 -mcmodel=large -mstackrealign' || exit_failure "$0: failed to build ported libssp"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install-target-libssp || exit_failure "$0: failed to install ported libssp"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

export CXXFLAGS=$OLD_CXXFLAGS
export CC_FOR_TARGET=$OLD_CC
export CXX_FOR_TARGET=$OLD_CXX
export AR_FOR_TARGET=$OLD_AR
export AS_FOR_TARGET=$OLD_AS
export NM_FOR_TARGET=$OLD_NM
export RANLIB_FOR_TARGET=$OLD_RANLIB
export STRIP_FOR_TARGET=$OLD_STRIP
export OBJDUMP_FOR_TARGET=$OLD_OBJDUMP

echo " ==>"
echo " ==> Finished building ported GCC"
echo " ==>"

