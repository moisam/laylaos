#!/bin/sh

#
# Copyright 2021-2024 (c) Mohammed Isam
#
# This script is part of LaylaOS
#
# This is a supporting script that is not to be used directly.
# It is called by create_bootable_disk.sh with the
# appropriate options.
#

TMPBUILDDIR=$1
SYSROOT=$2
CWD=`pwd`

cd ${TMPBUILDDIR}

echo "Creating inital dirs"
echo "---------------------------"
echo "    Under ${TMPBUILDDIR} (`pwd`)"
echo

sudo mkdir dev etc initrd lib mnt proc root tmp usr var
sudo mkdir -p usr/share/gui/desktop
sudo mkdir var/log
sudo mkdir var/run
sudo mkdir var/tmp
sudo mkdir mnt/cdrom
sudo ln -s /usr/bin bin
sudo ln -s /usr/sbin sbin

# set up appropriate permissions for dirs
sudo chmod 0755 bin dev etc lib mnt sbin usr var
sudo chmod 0700 root
sudo chmod 0644 initrd
sudo chmod 0555 proc
sudo chmod 0777 tmp

sudo touch root/.profile
sudo cp ${CWD}/../others/home_files/.inputrc root/
sudo cp ${CWD}/../others/home_files/.bashrc root/

sudo cp ${CWD}/../others/share_files/pci.ids usr/share/

echo
echo "Copying files in /usr and /etc"
echo "------------------------------"

for d in etc usr; do
    sudo cp -R ${SYSROOT}/${d}/* ./${d}/
done

sudo ln -s /usr/share/doc usr/doc
sudo ln -s /usr/share/info usr/info
sudo ln -s /usr/share/man usr/man

echo
echo "Copying other files in /etc"
echo "---------------------------"
sudo cp -r ${CWD}/../others/etc_files/* etc/
sudo cp -r ${CWD}/../others/timidity etc/
sudo ln -s /etc/timidity/timidity.cfg etc/timidity.cfg

echo
echo "Symlinking poweroff and halt"
echo "----------------------------"
sudo ln usr/sbin/reboot usr/sbin/poweroff
sudo ln usr/sbin/reboot usr/sbin/halt

echo
echo "Copying desktop resources"
echo "-------------------------"
sudo mkdir -p usr/share/gui
sudo mkdir -p usr/share/fonts

sudo cp -r ${CWD}/../others/share_files/gui/ usr/share/
sudo cp -r ${CWD}/../others/share_files/fonts/ usr/share/

echo
echo "Creating /bin/sh"
echo "----------------"
cd bin
sudo ln -s /bin/bash ./sh
cd ..


sudo chown root:root usr/bin/*
sudo chown root:root usr/sbin/*
cd ${CWD}

echo
echo "Finished copying files to disk"
echo

