#!/bin/bash

#
# Copyright 2021-2026 (c) Mohammed Isam
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
${SUDO} mkdir -p var/spool/mail
${SUDO} mkdir mnt/cdrom
${SUDO} ln -s /usr/bin bin
${SUDO} ln -s /usr/sbin sbin

# set up appropriate permissions for dirs
${SUDO} chmod 0755 bin dev etc lib mnt sbin usr var
${SUDO} chmod 0700 root
${SUDO} chmod 0644 initrd
${SUDO} chmod 0555 proc
${SUDO} chmod 0777 tmp

#${SUDO} touch root/.profile
#${SUDO} cp ${CWD}/../others/home_files/inputrc root/.inputrc
${SUDO} cp ${CWD}/../others/home_files/bashrc root/.bashrc
${SUDO} cp ${CWD}/../others/home_files/profile root/.profile

# create our regular user home dir
${SUDO} mkdir -p home/user
${SUDO} cp ${CWD}/../others/home_files/bashrc home/user/.bashrc
${SUDO} cp ${CWD}/../others/home_files/profile home/user/.profile
${SUDO} chown 1000:1000 home/user

# create skeleton dir structure for new users
${SUDO} mkdir -p etc/skel
${SUDO} cp ${CWD}/../others/home_files/bashrc etc/skel/.bashrc
${SUDO} cp ${CWD}/../others/home_files/profile etc/skel/.profile

${SUDO} cp ${CWD}/../others/share_files/pci.ids usr/share/
${SUDO} cp ${CWD}/../others/share_files/usb.ids usr/share/

echo
echo "Copying files in /usr, /etc, /bin and /sbin"
echo "-------------------------------------------"

for d in etc usr; do
    ${SUDO} cp -R ${SYSROOT}/${d}/* ./${d}/
done

# manually copy sysroot/bin and sysroot/sbin because we symlinked them to
# /usr/bin and /usr/sbin, and they will overwrite the host's utilities and
# not get copied to disk unless we do this manually
${SUDO} cp -R ${SYSROOT}/bin/* ./usr/bin/
${SUDO} cp -R ${SYSROOT}/sbin/* ./usr/sbin/

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

# ensure these have the right user and group
${SUDO} chown root:root usr/bin/*
${SUDO} chown root:root usr/sbin/*

# except man + mandb, which need to be owned by user man (userid 9)
${SUDO} chown 9:9 usr/bin/man usr/bin/mandb

# ensure regular users can run the GUI environment
${SUDO} chmod 0775 usr/bin/desktop/*
${SUDO} chmod 0775 usr/bin/widgets/*

# ensure these have the setuid bit set
${SUDO} chmod u+s usr/bin/su
${SUDO} chmod u+s usr/bin/passwd usr/bin/gpasswd usr/bin/expiry usr/bin/newgrp
${SUDO} chmod u+s usr/bin/chsh usr/bin/chfn usr/bin/chage

cd ${CWD}

echo
echo "Finished copying files to disk"
echo

