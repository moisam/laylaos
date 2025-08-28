#!/bin/bash

#
# Script to download and build perl-cross
#

DOWNLOAD_NAME="perl-cross"
DOWNLOAD_VERSION="1.5.3"
DOWNLOAD_URL="https://github.com/arsv/perl-cross/raw/releases/"
DOWNLOAD_PREFIX="perl-cross-"
DOWNLOAD_SUFFIX=".tar.gz"
DOWNLOAD_FILE="${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}${DOWNLOAD_SUFFIX}"
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_PREFIX}${DOWNLOAD_VERSION}"

# get common funcs
source ../common.sh

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths
download_and_extract

exit 0

