#!/bin/bash

#
# Script to download and build perl
#

DOWNLOAD_NAME="perl"
DOWNLOAD_VERSION="5.40.0"
DOWNLOAD_URL="https://www.cpan.org/src/5.0/"
DOWNLOAD_PREFIX="perl-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/perl

# build arch
if [ "${BUILD_ARCH}" == "x86_64" -o "${BUILD_ARCH}" == "i686" ]; then
    PERL_BYTEORDER=1234
else
    exit_failure "$0: build arch not recognised: ${BUILD_ARCH}"
fi

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# download the perl-cross tool
chmod +x ${CWD}/download_perl_cross.sh
./download_perl_cross.sh || exit_failure "$0: failed to get the perl-cross tool"

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cp -r ${DOWNLOAD_PORTS_PATH}/perl-cross-1.5.3/* ${DOWNLOAD_SRCDIR}/ \
    || exit_failure "$0: failed to patch source tree"

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

# build
cd ${DOWNLOAD_SRCDIR}

myname=`uname -s`

TOOL_PATHS="--target-tools-prefix=${BUILD_ARCH}-laylaos- \
           --with-cc=${CROSSCOMPILE_TOOLS_PATH}/bin/${BUILD_ARCH}-laylaos-gcc \
           --with-cpp=${CROSSCOMPILE_TOOLS_PATH}/bin/${BUILD_ARCH}-laylaos-cpp \
           --with-ranlib=${CROSSCOMPILE_TOOLS_PATH}/bin/${BUILD_ARCH}-laylaos-ranlib \
           --with-objdump=${CROSSCOMPILE_TOOLS_PATH}/bin/${BUILD_ARCH}-laylaos-objdump \
           --with-readelf=${CROSSCOMPILE_TOOLS_PATH}/bin/${BUILD_ARCH}-laylaos-readelf"

EXTRA_DEFINES="-D__laylaos__=1 -D_GNU_SOURCE=1 \
              -Dd_nanosleep=define -Ud_setproctitle -Ud_strlcpy -Ud_strlcat -Ud_memrchr \
              -Ud_prctl -Ud_prctl_set_name"

if [ "$myname" == "LaylaOS" ]; then
    # if building natively on LaylaOS, ensure we use our patched config.guess and
    # config.sub scripts
    cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/cnf/config.sub
    cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/cnf/config.guess

    # ensure --build is defined and equal to --target, otherwise the configure
    # script will think we are cross-compiling.
    ./configure \
        -Dosname=laylaos --target=${BUILD_ARCH}-pc-laylaos --targetarch=${BUILD_ARCH} \
        --build=${BUILD_ARCH}-pc-laylaos --buildarch=${BUILD_ARCH} \
        --prefix=/usr --sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
        `echo ${TOOL_PATHS} ${EXTRA_DEFINES}` \
        --host-libs="c,m,dl" -Dbyteorder=${PERL_BYTEORDER} \
        || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"
else
    ./configure \
        -Dosname=laylaos --target=${BUILD_TARGET} --targetarch=${BUILD_ARCH} \
        --prefix=/usr --sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
        `echo ${TOOL_PATHS} ${EXTRA_DEFINES}` \
        --host-libs="c,m,dl,bsd" \
        || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"
fi

sed -i '/#define HAS_STRLCAT/d' xconfig.h
sed -i '/#define HAS_STRLCPY/d' xconfig.h
sed -i '/#define HAS_SETPROCTITLE/d' xconfig.h
sed -i '/#define HAS_MEMRCHR/d' xconfig.h
sed -i 's/shm_nattch/shm_nattach/g' cpan/IPC-SysV/SysV.xs

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

