#!/bin/bash

#
# Script to download and build unifont
#

DOWNLOAD_NAME="unifont"
DOWNLOAD_VERSION="16.0.04"
DOWNLOAD_URL="https://unifoundry.com/pub/unifont/unifont-16.0.04/font-builds/"
DOWNLOAD_PREFIX="unifont-"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"
CWD=`pwd`

FONTS_INSTALLDIR="${CROSSCOMPILE_SYSROOT_PATH}/usr/share/fonts"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/share/fonts/${DOWNLOAD_FILE}.bdf.gz

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths

cd ${DOWNLOAD_PORTS_PATH}

for ext in "otf" "pcf.gz" "bdf.gz"; do
    echo "   ==> Downloading ${DOWNLOAD_URL}/${DOWNLOAD_FILE}.${ext}"
    wget -O "${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}.${ext}" "${DOWNLOAD_URL}/${DOWNLOAD_FILE}.${ext}" \
        || exit_failure "$0: failed to download ${DOWNLOAD_URL}/${DOWNLOAD_FILE}.${ext}"
done

# make sure the destination directory exists
if ! [ -e ${FONTS_INSTALLDIR} ]; then
    mkdir -p ${FONTS_INSTALLDIR} || exit_failure "$0: failed to create ${FONTS_INSTALLDIR}"
fi

# install
for ext in "otf" "pcf.gz" "bdf.gz"; do
    echo "   ==> Installing ${DOWNLOAD_URL}/${DOWNLOAD_FILE}.${ext}"
    mv "${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}.${ext}" "${FONTS_INSTALLDIR}/${DOWNLOAD_FILE}.${ext}" \
        || exit_failure "$0: failed to install ${DOWNLOAD_URL}/${DOWNLOAD_FILE}.${ext}"
done

# Clean up
cd ${CWD}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

