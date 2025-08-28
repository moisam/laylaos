#!/bin/bash

#
# Copyright 2021-2025 (c) Mohammed Isam
#
# This script is part of LaylaOS
#
# This is a supporting script that is not to be used directly.
# It is called by create_bootable_disk.sh with the
# appropriate options.
#

# sudo is not available on LaylaOS yet
myname=`uname -s`
if [ "$myname" == "LaylaOS" ]; then
    SUDO=
else
    SUDO=sudo
fi

TMPBUILDDIR=$1
SYSROOT=$2
CWD=`pwd`

cd ${TMPBUILDDIR}

echo "Creating inital dirs"
echo "---------------------------"
echo "    Under ${TMPBUILDDIR} (`pwd`)"
echo

${SUDO} mkdir dev etc initrd lib mnt proc root tmp usr var
${SUDO} mkdir -p usr/share/gui/desktop
${SUDO} mkdir var/log
${SUDO} mkdir var/run
${SUDO} mkdir var/tmp
${SUDO} mkdir mnt/cdrom
${SUDO} ln -s /usr/bin bin
${SUDO} ln -s /usr/sbin sbin

# set up appropriate permissions for dirs
${SUDO} chmod 0755 bin dev etc lib mnt sbin usr var
${SUDO} chmod 0700 root
${SUDO} chmod 0644 initrd
${SUDO} chmod 0555 proc
${SUDO} chmod 0777 tmp

${SUDO} touch root/.profile
${SUDO} cp ${CWD}/../others/home_files/inputrc root/.inputrc
${SUDO} cp ${CWD}/../others/home_files/bashrc root/.bashrc

${SUDO} cp ${CWD}/../others/share_files/pci.ids usr/share/

echo
echo "Copying files in /usr and /etc"
echo "------------------------------"

for d in etc usr; do
    ${SUDO} cp -R ${SYSROOT}/${d}/* ./${d}/
done

${SUDO} ln -s /usr/share/doc usr/doc
${SUDO} ln -s /usr/share/info usr/info
${SUDO} ln -s /usr/share/man usr/man

${SUDO} cp usr/Qt-5.12-hosttools/bin/* usr/Qt-5.12/bin/
${SUDO} rm -rf usr/Qt-5.12-hosttools

echo
echo "Copying other files in /etc"
echo "---------------------------"
${SUDO} cp -r ${CWD}/../others/etc_files/* etc/
${SUDO} cp -r ${CWD}/../others/timidity etc/
${SUDO} ln -s /etc/timidity/timidity.cfg etc/timidity.cfg

echo
echo "Symlinking poweroff and halt"
echo "----------------------------"
${SUDO} ln usr/sbin/reboot usr/sbin/poweroff
${SUDO} ln usr/sbin/reboot usr/sbin/halt

echo
echo "Copying desktop resources"
echo "-------------------------"
${SUDO} mkdir -p usr/share/gui
${SUDO} mkdir -p usr/share/fonts

${SUDO} cp -r ${CWD}/../others/share_files/gui/ usr/share/
${SUDO} cp -r ${CWD}/../others/share_files/fonts/ usr/share/

echo
echo "Creating /bin/sh"
echo "----------------"
cd bin
${SUDO} ln -s /bin/bash ./sh
cd ..


${SUDO} chown root:root usr/bin/*
${SUDO} chown root:root usr/sbin/*
cd ${CWD}

echo
echo "Finished copying files to disk"
echo

