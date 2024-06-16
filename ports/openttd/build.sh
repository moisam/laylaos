#!/bin/bash

#
# Script to download and build OpenTTD
#

DOWNLOAD_NAME="openttd"
DOWNLOAD_VERSION="13.4"
DOWNLOAD_URL="https://github.com/OpenTTD/OpenTTD/archive/refs/tags/"
DOWNLOAD_PREFIX=""
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
PATCH_FILE=${DOWNLOAD_NAME}.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/OpenTTD-${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/games/openttd

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

# build
mkdir ${DOWNLOAD_SRCDIR}/build2
cd ${DOWNLOAD_SRCDIR}/build2

cmake .. --toolchain ${CWD}/../prep_cross.cmake \
    -DPNG_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libpng.so \
    -DLIBLZMA_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/liblzma.so \
    -DPNG_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DZLIB_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DLIBLZMA_ROOT=${CROSSCOMPILE_SYSROOT_PATH}/usr \
    -DLIBLZMA_INCLUDE_DIR=${CROSSCOMPILE_SYSROOT_PATH}/usr/include \
    -DFREETYPE_LIBRARY=${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libfreetype.so \
    -DFREETYPE_INCLUDE_DIR_ft2build=${CROSSCOMPILE_SYSROOT_PATH}/usr/include/freetype2 \
    -DFREETYPE_INCLUDE_DIR_freetype2=${CROSSCOMPILE_SYSROOT_PATH}/usr/include/freetype2 \
    -DCMAKE_POLICY_DEFAULT_CMP0074=NEW \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# Download graphics & sound files
echo " ==> Downloading graphics & sound files"

do_download()
{
    echo "   ==> Downloading ${1}/${2} to ${DOWNLOAD_SRCDIR}/${2}"

    wget -O "${2}" "${1}/${2}"
    [ $? -ne 0 ] && exit_failure "$0: failed to download ${DOWNLOAD_URL}/${2}"

    unzip "${2}" || exit_failure "$0: failed to unzip ${2}"

    cp "${3}" ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/games/openttd/baseset/
}

cd ${DOWNLOAD_SRCDIR}

do_download "https://cdn.openttd.org/opengfx-releases/7.1/" \
            "opengfx-7.1-all.zip" \
            "opengfx-7.1.tar"

do_download "https://cdn.openttd.org/opensfx-releases/1.0.3/" \
            "opensfx-1.0.3-all.zip" \
            "opensfx-1.0.3.tar"

do_download "https://cdn.openttd.org/openmsx-releases/0.4.2/" \
            "openmsx-0.4.2-all.zip" \
            "openmsx-0.4.2.tar"

# Create desktop entry
echo " ==> Creating desktop entry"

mkdir -p ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/gui/icons/
cp ${DOWNLOAD_SRCDIR}/os/windows/openttd.ico ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/gui/icons/

mkdir -p ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/gui/desktop/
cat > ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/gui/desktop/openttd.entry << EOF
#
# Desktop entry file
#

[Desktop Entry]
Name=OpenTTD
Command=/usr/games/openttd
IconPath=Default
Icon=openttd
ShowOnDesktop=no
Category=Games

EOF

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

