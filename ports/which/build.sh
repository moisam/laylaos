#!/bin/bash

#
# Script to download and build which
#

DOWNLOAD_NAME="which"
CWD=`pwd`

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/which

# install (no build step)
echo " ==> Installing ${DOWNLOAD_NAME}"

cp ${CWD}/which.debianutils ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/which \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

cp ${CWD}/which.debianutils.1 ${CROSSCOMPILE_SYSROOT_PATH}/usr/man/man1/which.1 \
    || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

