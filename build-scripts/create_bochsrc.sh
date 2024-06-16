#!/bin/bash

#
# Copyright 2021-2024 (c) Mohammed Isam
#
# This script is part of LaylaOS
#
# It creates a bochrc file to use to run LaylaOS under Bochs
#

if [ $# -ne 2 ]; then
    echo "$0: Missing arguments"
    echo
    echo "Usage:"
    echo "  $0 inpath outpath"
    echo
    echo "Where:"
    echo "  inpath      path to the bootable disk image to run under Bochs"
    echo "  outpath     where to save the newly created bochsrc file"
    echo
    exit 1
fi

cat << EOF > "$2/bochsrc"
romimage: file=/usr/local/share/bochs/BIOS-bochs-latest, address=0xfffe0000
vgaromimage: file=/usr/local/share/bochs/VGABIOS-lgpl-latest
megs: 1024

debug: action=ignore
error: action=report
info: action=ignore, keyboard=report

ata0-master:  type=disk, path="$1", mode=flat, translation=auto

magic_break: enabled=1
mouse: type=imps2, enabled=1, toggle=ctrl+f10

ne2k: type=pci, ioaddr=0x300, irq=10, mac=b0:c4:20:00:00:00, ethmod=linux, ethdev=wlp0s20f3

pci: enabled=1, chipset=i440fx, slot1=es1370, slot2=ne2k
sound: waveoutdrv=alsa, waveindrv=alsa, midioutdrv=dummy
es1370: enabled=1, wavemode=1
speaker: enabled=1, mode=sound

boot: disk

display_library: x, options="gui_debug"

clock: rtc_sync=1, sync=realtime, time0=local

EOF


echo
echo "  The bochsrc file was created in $2/"
echo
echo "  You can now run LaylaOS under Bochs using:"
echo "     bochs -qf $2/bochsrc"
echo
echo "  Or (if bochsrc is in the current working directory):"
echo "     bochs -q"
echo

