#!/bin/sh
set -e

cd ../../
. ./config.sh
cd kernel/acpi

export BUILDDIR=build

[ -e $BUILDDIR ] && rm -r $BUILDDIR
mkdir $BUILDDIR

FILES=`find components/ os_specific/ -name '*.c'`
cp $FILES $BUILDDIR
cp -r ../include/acpi/* $BUILDDIR
cp tools/acpidump/acpidump.h $BUILDDIR

DESTDIR="$SYSROOT" $MAKE

