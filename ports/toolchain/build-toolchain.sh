#!/bin/bash

#
# Script to download and build the cross-compiler toolchain
#

CWD=`pwd`

# get common funcs
source ../common.sh

check_target
check_paths

# The versions we will download and compile
export CROSS_BINUTILS_VERSION="2.38"
export CROSS_GCC_VERSION="11.3.0"
export CROSS_MUSL_VERSION="1.2.4"

# Save these for later
OLDPREFIX=$PREFIX
OLDPATH=$PATH
OLDTARGET=$TARGET

export PREFIX="${CROSSCOMPILE_TOOLS_PATH}"
export PATH="${PREFIX}/bin:${PATH}"
export TARGET=${BUILD_TARGET}

myname=`uname -s`
if [ "$myname" == "LaylaOS" ]; then
    export CPPFLAGS="-D__laylaos__ -D_GNU_SOURCE -D__${BUILD_ARCH}__"
fi

####################################
# Build the userland cross compiler
####################################

./build-binutils.sh || exit_failure "$0: failed to build binutils"
./build-gcc.sh || exit_failure "$0: failed to build gcc"

# Build lib C
./build-musl.sh || exit_failure "$0: failed to build musl"

# This is where our build-gcc.sh script downloaded the source
GCC_SRCDIR="${DOWNLOAD_PORTS_PATH}/gcc-${CROSS_GCC_VERSION}"

# This is where our build-gcc.sh script downloaded the source
BINUTILS_SRCDIR="${DOWNLOAD_PORTS_PATH}/binutils-${CROSS_BINUTILS_VERSION}"

# This is where our build-musl.sh script downloaded the source
MUSL_SRCDIR="${DOWNLOAD_PORTS_PATH}/musl-${CROSS_MUSL_VERSION}"

# Rebuild with libc support
echo " ==>"
echo " ==> Rebuilding binutils for ${TARGET}"
echo " ==>"

mkdir -p ${BINUTILS_SRCDIR}/build
cd ${BINUTILS_SRCDIR}/build || exit_failure "$0: failed to chdir to build directory"

${BINUTILS_SRCDIR}/configure --target=${TARGET} --prefix="${PREFIX}" --disable-nls --disable-werror --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} --enable-shared

make || "$0: failed to build binutils (second pass)"
make install || "$0: failed to install binutils (second pass)"

# Rebuild with threads support
echo " ==>"
echo " ==> Rebuilding gcc for ${TARGET} with thread support"
echo " ==>"

mkdir -p ${GCC_SRCDIR}/build
cd ${GCC_SRCDIR}/build || exit_failure "$0: failed to chdir to build directory"

${GCC_SRCDIR}/configure --target=${TARGET} --prefix="${PREFIX}" --disable-nls --enable-languages=c,c++ --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} --enable-shared --enable-threads=yes --enable-libssp --disable-tls

make all-gcc || "$0: failed to build gcc (second pass)"
make install-gcc || exit_failure "$0: failed to install gcc (second pass)"

make all-target-libgcc || "$0: failed to build libgcc (second pass)"
make install-target-libgcc || exit_failure "$0: failed to install libgcc (second pass)"


# Rebuild musl with the ${ARCH}-laylaos target
# XXX: is this really necessary?

echo " ==>"
echo " ==> Rebuilding musl for ${TARGET}"
echo " ==>"

mkdir -p ${MUSL_SRCDIR}/build2
cd ${MUSL_SRCDIR}/build2

export CROSSTOOLS_PREFIX="${CROSSCOMPILE_TOOLS_PATH}/bin/${TARGET}"

${MUSL_SRCDIR}/configure --prefix=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    --syslibdir=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib \
    --host=${BUILD_TARGET} --target=${BUILD_TARGET} \
    --enable-shared --enable-static \
    CFLAGS="-I${CROSSCOMPILE_SYSROOT_PATH}/usr/include -D__laylaos__ -D__${BUILD_ARCH}__ -D_POSIX_SOURCE" \
    CPPFLAGS="--sysroot=${CROSSCOMPILE_SYSROOT_PATH} -isystem=/usr/include" \
    CC=${CROSSTOOLS_PREFIX}-gcc CXX=${CROSSTOOLS_PREFIX}-g++ \
    AS=${CROSSTOOLS_PREFIX}-as AR=${CROSSTOOLS_PREFIX}-ar \
    RANLIB=${CROSSTOOLS_PREFIX}-ranlib LD=${CROSSTOOLS_PREFIX}-ld \
    STRIP=${CROSSTOOLS_PREFIX}-strip RANLIB=${CROSSTOOLS_PREFIX}-ranlib \
    NM=${CROSSTOOLS_PREFIX}-nm OBJDUMP=${CROSSTOOLS_PREFIX}-objdump

make || exit_failure "$0: failed to build musl (second pass)"
make install || exit_failure "$0: failed to install musl (second pass)"

# remove unneeded linux header files
rm ${CROSSCOMPILE_SYSROOT_PATH}/usr/include/sys/soundcard.h
rm ${CROSSCOMPILE_SYSROOT_PATH}/usr/include/bits/soundcard.h


# Build and install libgcc and libstdc++-v3
cd ${GCC_SRCDIR}/build || exit_failure "$0: failed to chdir to build directory"

make all-target-libgcc CFLAGS_FOR_TARGET='-D_POSIX_THREADS -g -O2 -mcmodel=large -mstackrealign' || "$0: failed to build libgcc (third pass)"
make install-target-libgcc || exit_failure "$0: failed to install libgcc (third pass)"

make all-target-libstdc++-v3 CFLAGS_FOR_TARGET='-D_POSIX_THREADS -g -O2 -mcmodel=large -mstackrealign' CXXFLAGS_FOR_TARGET='-D_POSIX_THREADS -g -O2 -mcmodel=large -mstackrealign' || exit_failure "$0: failed to build libstdc++-v3 (third pass)"
make install-target-libstdc++-v3 || exit_failure "$0: failed to install libstdc++-v3 (third pass)"

make all-target-libssp CFLAGS_FOR_TARGET='-D_POSIX_THREADS -g -O2 -mcmodel=large -mstackrealign' || exit_failure "$0: failed to build libssp (third pass)"
make install-target-libssp || exit_failure "$0: failed to install libssp (third pass)"


# copy libgcc and libstdc++ as some libs will need to link to them
# the installation directory differs between native and cross compiles
if [ "$myname" == "LaylaOS" ]; then
    PATH_TO_GCC_LIBS=${CROSSCOMPILE_TOOLS_PATH}/lib
else
    PATH_TO_GCC_LIBS=${CROSSCOMPILE_TOOLS_PATH}/${BUILD_ARCH}-laylaos/lib
fi

cp ${PATH_TO_GCC_LIBS}/libgcc_s.so.1 ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/
cp ${PATH_TO_GCC_LIBS}/libstdc++.so.6 ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/


####################################
# Build the kernel cross compiler
####################################
export TARGET=${BUILD_ARCH}-laylakernel

# build binutils
echo " ==>"
echo " ==> Building binutils for the kernel"
echo " ==>"
mkdir -p ${BINUTILS_SRCDIR}/build-kernel
cd ${BINUTILS_SRCDIR}/build-kernel

${BINUTILS_SRCDIR}/configure --target=${TARGET} --prefix="${PREFIX}" --disable-nls --disable-werror --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} --enable-shared

make || "$0: failed to build kernel binutils"
make install || "$0: failed to install kernel binutils"

# build GCC
echo " ==>"
echo " ==> Building GCC for the kernel"
echo " ==>"
mkdir -p ${GCC_SRCDIR}/build-kernel
cd ${GCC_SRCDIR}/build-kernel

${GCC_SRCDIR}/configure --target=${TARGET} --prefix="${PREFIX}" --disable-nls --enable-languages=c,c++ --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} --enable-shared

make all-gcc || "$0: failed to build kernel gcc"
make all-target-libgcc CFLAGS_FOR_TARGET='-g -O2 -mcmodel=large -mno-red-zone' || "$0: failed to build kernel libgcc"
make install-gcc || "$0: failed to install kernel gcc"
make install-target-libgcc || "$0: failed to install kernel libgcc"


# Clean up
echo " ==> Removing build directories"
cd ${CWD}
rm -rf ${GCC_SRCDIR}
rm -rf ${BINUTILS_SRCDIR}
rm -rf ${MUSL_SRCDIR}

# Restore these
export PREFIX=${OLDPREFIX}
export PATH=${OLDPATH}
export TARGET=${OLDTARGET}

