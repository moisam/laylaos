set(crosscompile_arch $ENV{BUILD_ARCH})
set(crosscompile_sysroot $ENV{CROSSCOMPILE_SYSROOT_PATH})

# Setting CMAKE_SYSTEM_NAME causes cmake to automatically set CMAKE_CROSSCOMPILING.
# When cross-compiling, this is fine, but when we build natively on LaylaOS,
# this won't work. Therefore, only set CMAKE_SYSTEM_NAME if we know we are
# cross-compiling.
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "LaylaOS")
    set(CMAKE_SYSTEM_NAME LaylaOS)
endif()

set(CMAKE_SYSTEM_PROCESSOR ${crosscompile_arch})
set(LAYLAOS True)

set(CMAKE_SYSROOT ${crosscompile_sysroot})
set(CMAKE_STAGING_PREFIX ${crosscompile_sysroot}/usr)
set(CMAKE_PREFIX_PATH ${crosscompile_sysroot}:${crosscompile_sysroot}/usr)

set(CMAKE_INSTALL_PREFIX /usr)

set(tools $ENV{CROSSCOMPILE_TOOLS_PATH}/bin)
set(CMAKE_C_COMPILER ${tools}/${crosscompile_arch}-laylaos-gcc)
set(CMAKE_CXX_COMPILER ${tools}/${crosscompile_arch}-laylaos-g++)

set(CMAKE_C_FLAGS "-I${crosscompile_sysroot}/usr/include -I${crosscompile_sysroot}/usr/include/freetype2 -I${crosscompile_sysroot}/usr/include/freetype2/freetype --sysroot=${crosscompile_sysroot} -D__laylaos__ -D__${crosscompile_arch}__ -D_GNU_SOURCE -mstackrealign")
set(CMAKE_CXX_FLAGS "-I${crosscompile_sysroot}/usr/include -I${crosscompile_sysroot}/usr/include/freetype2 -I${crosscompile_sysroot}/usr/include/freetype2/freetype --sysroot=${crosscompile_sysroot} -D__laylaos__ -D__${crosscompile_arch}__ -D_GNU_SOURCE -mstackrealign -fPIC")

set(CMAKE_FIND_ROOT_PATH ${crosscompile_sysroot})
list(APPEND CMAKE_FIND_ROOT_PATH ${crosscompile_sysroot}/usr)
list(APPEND CMAKE_FIND_ROOT_PATH ${crosscompile_sysroot}/usr/local)
list(APPEND CMAKE_FIND_ROOT_PATH ${crosscompile_sysroot}/usr/include/freetype2)
list(APPEND CMAKE_FIND_ROOT_PATH ${crosscompile_sysroot}/usr/include/freetype2/freetype)

set(CMAKE_LIBRARY_PATH ${crosscompile_sysroot})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

