#!/bin/bash

#
# Script to download and build texinfo
#

DOWNLOAD_NAME="texinfo"
DOWNLOAD_VERSION="7.1.1"
DOWNLOAD_URL="https://ftp.gnu.org/gnu/texinfo/"
DOWNLOAD_PREFIX="texinfo-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/texi2any

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
mv ${DOWNLOAD_SRCDIR}/tp/Texinfo/XS/config.sub ${DOWNLOAD_SRCDIR}/tp/Texinfo/XS/config.sub.OLD

cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.sub
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/tp/Texinfo/XS/config.sub

mv ${DOWNLOAD_SRCDIR}/build-aux/config.guess ${DOWNLOAD_SRCDIR}/build-aux/config.guess.OLD
mv ${DOWNLOAD_SRCDIR}/tp/Texinfo/XS/config.guess ${DOWNLOAD_SRCDIR}/tp/Texinfo/XS/config.guess.OLD

cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.guess
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/tp/Texinfo/XS/config.guess

mv ${DOWNLOAD_SRCDIR}/tp/Texinfo/XS/gnulib/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/tp/Texinfo/XS/gnulib/m4/libtool.m4.OLD

cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/tp/Texinfo/XS/gnulib/m4/libtool.m4

cd ${DOWNLOAD_SRCDIR} && autoreconf

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

PERL=${HOST_PERL_FOR_CROSSCOMPILE} PERL_EXT_CC=$CC \
    PERL_EXT_CFLAGS="$CFLAGS" PERL_EXT_CPPFLAGS="$CPPFLAGS" PERL_EXT_LDFLAGS="$LDFLAGS" \
    BUILD_CC=gcc BUILD_AR=ar BUILD_RANLIB=ranlib \
    CFLAGS="$CFLAGS -mstackrealign" \
    ../configure  \
    --host=${BUILD_TARGET} --build=x86_64-pc-linux-gnu \
    --prefix=/usr \
    --with-included-regex --enable-cross-guesses=risky texinfo_cv_sys_iconv_converts_euc_cn=yes \
    --enable-perl-xs --enable-perl-api-texi-build --disable-tp-tests --disable-pod-simple-texinfo-tests \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"
make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} TEXMF=/usr/share/texmf install-tex || exit_failure "$0: failed to install tex files from ${DOWNLOAD_NAME}"

# patch these suckers
sed -i '1 s/^.*$/#! \/usr\/bin\/perl/' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/pod2texi
sed -i '1 s/^.*$/#! \/usr\/bin\/perl/' ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/texi2any

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

