#!/bin/bash

#
# Copyright 2021-2024 (c) Mohammed Isam
#
# This script is part of LaylaOS
#

cat << EOF > ../ports/crossfile.meson.laylaos

[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$AR'
strip = '$STRIP'

[host_machine]
system = 'laylaos'
cpu_family = '$ARCH'
cpu = '$ARCH'
endian = 'little'

[properties]
sys_root = '$SYSROOT'
pkg_config_libdir = '${SYSROOT}/usr/lib/pkgconfig'

EOF
