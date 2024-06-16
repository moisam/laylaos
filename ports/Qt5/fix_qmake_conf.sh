#!/bin/bash

if [ $# -ne 1 ]; then
    echo "$0: Missing arguments"
    echo
    echo "Usage:"
    echo "  $0 PATH"
    echo
    echo "Where:"
    echo "  PATH      where to save qmake.conf"
    echo
    exit 1
fi

cat << EOF > $1/qmake.conf
#
# qmake configuration for laylaos-g++
#

MAKEFILE_GENERATOR      = UNIX
QMAKE_PLATFORM          = laylaos

include(../common/unix.conf)
include(../common/gcc-base-unix.conf)
include(../common/g++-unix.conf)

CROSS_ROOT   = ${CROSSCOMPILE_SYSROOT_PATH}
CROSS_PREFIX = ${CROSSCOMPILE_SYSROOT_PATH}/usr/bin/build-crosstools/${BUILD_ARCH}-laylaos-

QMAKE_CC                = \$\${CROSS_PREFIX}gcc -mstackrealign
QMAKE_CXX               = \$\${CROSS_PREFIX}g++ -mstackrealign

QMAKE_LINK_C            = \$\${QMAKE_CC}
QMAKE_LINK_C_SHLIB      = \$\${QMAKE_CC}
QMAKE_LINK              = \$\${QMAKE_CXX}
QMAKE_LINK_SHLIB        = \$\${QMAKE_CXX}

QMAKE_LIBS              =
QMAKE_INCDIR            = \$\${CROSS_ROOT}/usr/include
QMAKE_LIBDIR            = \$\${CROSS_ROOT}/usr/lib

QMAKE_LIBS_NETWORK      =
QMAKE_LIBS_OPENGL       =
QMAKE_LIBS_OPENGL_QT    =
QMAKE_LIBS_THREAD       =

QMAKE_AR                = \$\${CROSS_PREFIX}ar cqs
QMAKE_OBJCOPY           = \$\${CROSS_PREFIX}objcopy
QMAKE_NM                = \$\${CROSS_PREFIX}nm -P
QMAKE_RANLIB            =

QMAKE_STRIP             = \$\${CROSS_PREFIX}strip
QMAKE_STRIPFLAGS_LIB   += --strip-unneeded

QMAKE_CFLAGS_THREAD     =
QMAKE_CXXFLAGS_THREAD   =

load(qt_config)
EOF
