#!/bin/bash

#
# Script to download and build gettext
#

DOWNLOAD_NAME="gettext"
DOWNLOAD_VERSION="0.22.5"
DOWNLOAD_URL="https://ftp.gnu.org/pub/gnu/gettext/"
DOWNLOAD_PREFIX="gettext-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/gettext

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
mv ${DOWNLOAD_SRCDIR}/gettext-tools/examples/hello-c++-kde/admin/config.sub ${DOWNLOAD_SRCDIR}/gettext-tools/examples/hello-c++-kde/admin/config.sub.OLD
mv ${DOWNLOAD_SRCDIR}/libtextstyle/build-aux/config.sub ${DOWNLOAD_SRCDIR}/libtextstyle/build-aux/config.sub.OLD

cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.sub
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/gettext-tools/examples/hello-c++-kde/admin/config.sub
cp ${CWD}/../config.sub.laylaos ${DOWNLOAD_SRCDIR}/libtextstyle/build-aux/config.sub

mv ${DOWNLOAD_SRCDIR}/build-aux/config.guess ${DOWNLOAD_SRCDIR}/build-aux/config.guess.OLD
mv ${DOWNLOAD_SRCDIR}/gettext-tools/examples/hello-c++-kde/admin/config.guess ${DOWNLOAD_SRCDIR}/gettext-tools/examples/hello-c++-kde/admin/config.guess.OLD
mv ${DOWNLOAD_SRCDIR}/libtextstyle/build-aux/config.guess ${DOWNLOAD_SRCDIR}/libtextstyle/build-aux/config.guess.OLD

cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/build-aux/config.guess
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/gettext-tools/examples/hello-c++-kde/admin/config.guess
cp ${CWD}/../config.guess.laylaos ${DOWNLOAD_SRCDIR}/libtextstyle/build-aux/config.guess

mv ${DOWNLOAD_SRCDIR}/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/m4/libtool.m4.OLD
mv ${DOWNLOAD_SRCDIR}/libtextstyle/m4/libtool.m4 ${DOWNLOAD_SRCDIR}/libtextstyle/m4/libtool.m4.OLD

cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/m4/libtool.m4
cp ${CWD}/../libtool.m4.laylaos ${DOWNLOAD_SRCDIR}/libtextstyle/m4/libtool.m4

cd ${DOWNLOAD_SRCDIR} && autoreconf
cd ${DOWNLOAD_SRCDIR}/gettext-tools/examples/ && autoreconf

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

LDFLAGS="-ltinfo" CFLAGS="$CFLAGS -mstackrealign" \
    ../configure  \
    --host=${BUILD_TARGET} --prefix=/usr \
    --disable-java --without-git --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    --with-included-gettext --enable-nls \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix libgettext*.la for the future generations
sed -i "s/dependency_libs=.*/dependency_libs='-lintl -lunistring -liconv -ltinfo'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libgettextlib.la
sed -i "s/dependency_libs=.*/dependency_libs='-lc -lintl -lunistring -liconv -ltinfo'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libgettextpo.la
sed -i "s/dependency_libs=.*/dependency_libs='-lgettextlib -lunistring -liconv -ltextstyle -lintl -lc -ltinfo'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libgettextsrc.la

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

