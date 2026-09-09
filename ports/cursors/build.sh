#!/bin/bash

#
# Script to download mouse cursors
#

ICONS_TARGET_DIR=${CROSSCOMPILE_SYSROOT_PATH}/usr/share/icons
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/cursors"

echo " ==>"
echo " ==> Downloading cursors"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
echo " ==>"

exit_failure()
{
    echo $1
    exit 1
}

do_download()
{
    echo "   ==> Downloading ${1} to ${DOWNLOAD_SRCDIR}/${2}"

    wget -O "${2}" "${1}"
    [ $? -ne 0 ] && exit_failure "$0: failed to download ${1}"

    if [ "${2}" == *".zip" ]; then
        unzip "${2}" || exit_failure "$0: failed to unzip ${2}"
    else
        tar -xf "${2}" || exit_failure "$0: failed to untar ${2}"
    fi

    rm "${2}"
}

mkdir -p ${DOWNLOAD_SRCDIR}
cd ${DOWNLOAD_SRCDIR}
mkdir -p ${ICONS_TARGET_DIR}

# mocu-xcursor
do_download "https://github.com/sevmeyer/mocu-xcursor/releases/download/1.1/mocu-xcursor-1.1.zip" "mocu-xcursor-1.1.zip"

cp -r ./Mocu-Black-Left/ ${ICONS_TARGET_DIR}/ || exit_failure "$0: failed to copy mocu-xcursor-1.1"
cp -r ./Mocu-Black-Right/ ${ICONS_TARGET_DIR}/ || exit_failure "$0: failed to copy mocu-xcursor-1.1"
cp -r ./Mocu-White-Left/ ${ICONS_TARGET_DIR}/ || exit_failure "$0: failed to copy mocu-xcursor-1.1"
cp -r ./Mocu-White-Right/ ${ICONS_TARGET_DIR}/ || exit_failure "$0: failed to copy mocu-xcursor-1.1"

cp ./README.txt ${ICONS_TARGET_DIR}/Mocu-Black-Left/ || exit_failure "$0: failed to copy mocu-xcursor-1.1"
cp ./README.txt ${ICONS_TARGET_DIR}/Mocu-Black-Right/ || exit_failure "$0: failed to copy mocu-xcursor-1.1"
cp ./README.txt ${ICONS_TARGET_DIR}/Mocu-White-Left/ || exit_failure "$0: failed to copy mocu-xcursor-1.1"
cp ./README.txt ${ICONS_TARGET_DIR}/Mocu-White-Right/ || exit_failure "$0: failed to copy mocu-xcursor-1.1"

chmod -R a+r ${ICONS_TARGET_DIR}/Mocu-Black-Left/
chmod -R a+r ${ICONS_TARGET_DIR}/Mocu-Black-Right/
chmod -R a+r ${ICONS_TARGET_DIR}/Mocu-White-Left/
chmod -R a+r ${ICONS_TARGET_DIR}/Mocu-White-Right/
chmod a+x ${ICONS_TARGET_DIR}/Mocu-Black-Left ${ICONS_TARGET_DIR}/Mocu-Black-Left/cursors
chmod a+x ${ICONS_TARGET_DIR}/Mocu-Black-Right ${ICONS_TARGET_DIR}/Mocu-Black-Right/cursors
chmod a+x ${ICONS_TARGET_DIR}/Mocu-White-Left ${ICONS_TARGET_DIR}/Mocu-White-Left/cursors
chmod a+x ${ICONS_TARGET_DIR}/Mocu-White-Right ${ICONS_TARGET_DIR}/Mocu-White-Right/cursors

# hackneyed light
do_download "https://gitlab.com/-/project/6703061/uploads/2610dde7a179b3c1630b84f5a376928f/Hackneyed-24px-0.9.3-right-handed.tar.bz2" "Hackneyed-24px.tar.bz2"

cp -r Hackneyed-24px ${ICONS_TARGET_DIR}/ || exit_failure "$0: failed to copy contents of Hackneyed-24px"

do_download "https://gitlab.com/-/project/6703061/uploads/7ccc43053e3303fde713acec0b47c262/Hackneyed-36px-0.9.3-right-handed.tar.bz2" "Hackneyed-36px.tar.bz2"

cp -r Hackneyed-36px ${ICONS_TARGET_DIR}/ || exit_failure "$0: failed to copy contents of Hackneyed-36px"

do_download "https://gitlab.com/-/project/6703061/uploads/9825422d85a64eabeeefcbe27a8acd22/Hackneyed-48px-0.9.3-right-handed.tar.bz2" "Hackneyed-48px.tar.bz2"

cp -r Hackneyed-48px ${ICONS_TARGET_DIR}/ || exit_failure "$0: failed to copy contents of Hackneyed-48px"

# hackneyed dark
do_download "https://gitlab.com/-/project/6703061/uploads/4b8234a1bdf604803f6e4dfe24a340a2/Hackneyed-Dark-24px-0.9.3-right-handed.tar.bz2" "Hackneyed-Dark-24px.tar.bz2"

cp -r Hackneyed-Dark-24px ${ICONS_TARGET_DIR}/ || exit_failure "$0: failed to copy contents of Hackneyed-Dark-24px"

do_download "https://gitlab.com/-/project/6703061/uploads/275462cbf0b9738ef7c7d8fec44e5918/Hackneyed-Dark-36px-0.9.3-right-handed.tar.bz2" "Hackneyed-Dark-36px.tar.bz2"

cp -r Hackneyed-Dark-36px ${ICONS_TARGET_DIR}/ || exit_failure "$0: failed to copy contents of Hackneyed-Dark-36px"

do_download "https://gitlab.com/-/project/6703061/uploads/0d0a1d86e76e7ef0e7771d50d5758051/Hackneyed-Dark-48px-0.9.3-right-handed.tar.bz2" "Hackneyed-Dark-48px.tar.bz2"

cp -r Hackneyed-Dark-48px ${ICONS_TARGET_DIR}/ || exit_failure "$0: failed to copy contents of Hackneyed-Dark-48px"

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building cursors"
echo " ==>"

