#!/bin/bash

#
# Script to download and build hunspell
#

DOWNLOAD_NAME="hunspell"
DOWNLOAD_VERSION="1.7.2"
DOWNLOAD_URL="https://github.com/hunspell/hunspell/releases/download/v${DOWNLOAD_VERSION}/"
DOWNLOAD_PREFIX="hunspell-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libhunspell-1.7.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_SRCDIR}
autoreconf -vfi

mv config.sub config.sub.OLD
cp ${CWD}/../config.sub.laylaos config.sub

mv config.guess config.guess.OLD
cp ${CWD}/../config.guess.laylaos config.guess

mv m4/libtool.m4 m4/libtool.m4.OLD
cp ${CWD}/../libtool.m4.laylaos m4/libtool.m4

autoreconf

# build
mkdir -p ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

CXXFLAGS="${CXXFLAGS} -fPIC -DPIC" ${DOWNLOAD_SRCDIR}/configure --host=${BUILD_TARGET} \
    --enable-shared --with-sysroot=${CROSSCOMPILE_SYSROOT_PATH} \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make DESTDIR=${CROSSCOMPILE_SYSROOT_PATH} install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Fix libhunspell-1.7.la for the future generations
sed -i "s/dependency_libs=.*/dependency_libs='-lstdc++'/g" ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libhunspell-1.7.la

# Download dictionaries
# https://wordlist.aspell.net/dicts/
# https://sourceforge.net/projects/wordlist/files/speller/2026.02.25/
echo " ==> Downloading dictionaries"

do_download()
{
    echo "   ==> Downloading ${1}${2} to ${DOWNLOAD_SRCDIR}/${3}"

    wget -O "${3}.zip" "${1}${2}/hunspell-${3}-${2}.zip"
    [ $? -ne 0 ] && exit_failure "$0: failed to download ${1}${2}/hunspell-${3}-${2}.zip"

    unzip "${3}.zip" || exit_failure "$0: failed to unzip ${3}.zip"
    rm "${3}.zip"

    cp ${3}* README_${3}* ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/hunspell/ \
        || exit_failure "$0: failed to copy contents of ${3}.zip"
}

cd ${DOWNLOAD_SRCDIR}
mkdir -p ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/hunspell

DICTVER="2026.02.25"

do_download "https://github.com/en-wl/wordlist/releases/download/rel-" ${DICTVER} "en_AU"
do_download "https://github.com/en-wl/wordlist/releases/download/rel-" ${DICTVER} "en_CA"
do_download "https://github.com/en-wl/wordlist/releases/download/rel-" ${DICTVER} "en_US"
do_download "https://github.com/en-wl/wordlist/releases/download/rel-" ${DICTVER} "en_GB-ise"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

