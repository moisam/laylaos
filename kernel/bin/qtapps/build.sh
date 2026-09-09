#!/bin/bash

#
# Script to build qt desktop apps
#

CWD=`pwd`

echo " ==>"
echo " ==> Building qt desktop apps"
echo " ==>"

exit_failure()
{
    echo $1
    exit 1
}

for f in `ls -d desktop widgets app*`; do
    cd "${CWD}/${f}"
    mkdir b
    cd b

    cmake .. --toolchain "${CWD}/toolchain_file.cmake" || exit_failure "$0: failed to configure ${f}"
    make || exit_failure "$0: failed to build ${f}"

    cd "${CWD}"
done

