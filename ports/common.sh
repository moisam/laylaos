#!/bin/bash

#
# Common functions used by porting scripts
#

check_target()
{
    if [ "x${BUILD_ARCH}" == "x" ]; then
        echo "$0: \$BUILD_ARCH is not defined"
        echo "$0: This should be x86_64 OR i686"
        echo
        exit 1
    fi

    if [ "x${BUILD_TARGET}" == "x" ]; then
        echo "$0: \$BUILD_TARGET is not defined"
        echo "$0: This should be x86_64-laylaos OR i686-laylaos"
        echo
        exit 1
    fi
}

check_paths()
{
    if [ "x${DOWNLOAD_PORTS_PATH}" == "x" ]; then
        echo "$0: \$DOWNLOAD_PORTS_PATH is not defined"
        echo "$0: This should point to the path where ports are to be downloaded for build"
        echo
        exit 1
    fi

    if [ "x${CROSSCOMPILE_TOOLS_PATH}" == "x" ]; then
        echo "$0: \$CROSSCOMPILE_TOOLS_PATH is not defined"
        echo "$0: This should point to the cross-compiler tool path"
        echo
        exit 1
    fi

    if [ "x${CROSSCOMPILE_SYSROOT_PATH}" == "x" ]; then
        echo "$0: \$CROSSCOMPILE_SYSROOT_PATH is not defined"
        echo "$0: This should point to the system root for cross-compilation"
        echo
        exit 1
    fi
}

exit_failure()
{
    echo $1
    exit 1
}

copy_or_die()
{
    cp $1 $2
    
    [ $? -eq 0 ] || exit_failure "$0: failed to copy $1 to $2"
}

download_only()
{
    # Create download dir if it does not exist
    if ! [ -e ${DOWNLOAD_PORTS_PATH} ]; then
        mkdir -p ${DOWNLOAD_PORTS_PATH} || exit_failure "$0: failed to create ${DOWNLOAD_PORTS_PATH}"
    fi

    # Download
    # Some downloads, like musl, pass an extra flag to wget
    echo "   ==> Downloading ${DOWNLOAD_URL}/${DOWNLOAD_FILE} to ${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}"
    wget -O "${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}" "${DOWNLOAD_URL}/${DOWNLOAD_FILE}" $1

    [ $? -ne 0 ] && exit_failure "$0: failed to download ${DOWNLOAD_URL}/${DOWNLOAD_FILE}"
}

download_and_extract()
{
    # Download first
    # Some downloads, like musl, pass an extra flag to wget
    myname=`uname -s`
    if [ "$myname" == "LaylaOS" ]; then
        download_only --no-check-certificate
    else
        download_only
    fi

    # Then extract
    echo "   ==> Extracting ${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}"
    tar -C "${DOWNLOAD_PORTS_PATH}" -xf "${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}" \
        || exit_failure "$0: failed to extract ${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}"

    # And remove the downloaded file
    echo "   ==> Removing ${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}"
    rm ${DOWNLOAD_PORTS_PATH}/${DOWNLOAD_FILE}
}

check_existing()
{
    if [ -d `dirname $2` ]; then
        if [ -e "$2" ]; then
            echo
            echo " ==> $1 seems to be compiled already"
            echo " ==> We will skip this step"
            echo " ==> If you think this is wrong or you need to re-compile $1,"
            echo " ==> Remove $2 and re-run"
            echo " ==> this script again"
            echo
            exit 0
        fi
    fi
}

