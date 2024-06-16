#!/bin/bash

#
# Script to download and build musl libc
#

DOWNLOAD_NAME="musl"
DOWNLOAD_VERSION=${CROSS_MUSL_VERSION}
DOWNLOAD_URL="https://musl.libc.org/releases/"
DOWNLOAD_PREFIX="musl-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=musl.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
if [ -d "${CROSSCOMPILE_SYSROOT_PATH}/usr" ]; then
    if [ -e "${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libc.so" ]; then
        echo " ==> musl libc seems to be compiled already"
        echo " ==> We will skip this step"
        echo " ==> If you think this is wrong or you need to re-compile musl,"
        echo " ==> Remove ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/* and re-run"
        echo " ==> this script again"
        exit 0
    fi
fi

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"
cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# we will replace some of musl's original files with our own
rm ${DOWNLOAD_SRCDIR}/src/process/x86_64/vfork.s
rm ${DOWNLOAD_SRCDIR}/src/signal/x86_64/restore.s
rm ${DOWNLOAD_SRCDIR}/src/thread/x86_64/clone.s
rm ${DOWNLOAD_SRCDIR}/src/thread/x86_64/__set_thread_area.s
rm ${DOWNLOAD_SRCDIR}/src/thread/x86_64/__unmapself.s

copy_or_die extra-musl/x84_64/vfork.S ${DOWNLOAD_SRCDIR}/src/process/x86_64/
copy_or_die extra-musl/x84_64/restore.S ${DOWNLOAD_SRCDIR}/src/signal/x86_64
copy_or_die extra-musl/x84_64/clone.S ${DOWNLOAD_SRCDIR}/src/thread/x86_64
copy_or_die extra-musl/x84_64/__set_thread_area.S ${DOWNLOAD_SRCDIR}/src/thread/x86_64
copy_or_die extra-musl/x84_64/__unmapself.S ${DOWNLOAD_SRCDIR}/src/thread/x86_64

rm ${DOWNLOAD_SRCDIR}/src/signal/i386/restore.s

copy_or_die extra-musl/i386/restore.S ${DOWNLOAD_SRCDIR}/src/signal/i386

# and we have some new files as well
copy_or_die extra-musl/syscall-laylaos.h.in ${DOWNLOAD_SRCDIR}/arch/x86_64/bits
copy_or_die extra-musl/gnu_posix_fallocate.c ${DOWNLOAD_SRCDIR}/src/fcntl
copy_or_die extra-musl/sysctl.c ${DOWNLOAD_SRCDIR}/src/misc

# sys/socket.h needs this from our kernel source
copy_or_die extra-musl/ucred.h ${CROSSCOMPILE_SYSROOT_PATH}/usr/include/sys

# build musl
echo " ==>"
echo " ==> Building ${DOWNLOAD_NAME}"
echo " ==>"

mkdir -p ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

export CROSSTOOLS_PREFIX="${CROSSCOMPILE_TOOLS_PATH}/bin/${BUILD_ARCH}-laylabootstrap"

${DOWNLOAD_SRCDIR}/configure --prefix=${CROSSCOMPILE_SYSROOT_PATH}/usr \
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

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"
make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

ln -s Scrt1.o ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/crt0.o
ln -s libc.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/ld.so

#echo " ==> Removing build directory"
cd ${CWD}
#rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

