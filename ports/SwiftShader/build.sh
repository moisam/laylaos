#!/bin/bash

#
# Script to download and build SwiftShader
#

DOWNLOAD_NAME="SwiftShader"
DOWNLOAD_URL="https://github.com/moisam/swiftshader.git"
PATCH_FILE=${DOWNLOAD_NAME}.diff
PATCH_FILE2=${DOWNLOAD_NAME}2.diff
CWD=`pwd`

# where the downloaded and extracted source will end up
DOWNLOAD_SRCDIR="${DOWNLOAD_PORTS_PATH}/SwiftShader"

# get common funcs
source ../common.sh

# check for an existing compile
check_existing ${DOWNLOAD_NAME} ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libvulkan.so

# download source
echo " ==> Downloading ${DOWNLOAD_NAME}"
echo " ==> Download will be saved in ${DOWNLOAD_PORTS_PATH}"
check_target
check_paths

cd ${DOWNLOAD_PORTS_PATH}

git clone ${DOWNLOAD_URL} && (cd swiftshader && git submodule update --init --recursive third_party/git-hooks && ./third_party/git-hooks/install_hooks.sh)

[ $? -ne 0 ] && exit_failure "$0: failed to clone ${DOWNLOAD_URL}"

mv swiftshader SwiftShader

# patch and copy our extra files
echo " ==> Patching ${DOWNLOAD_NAME}"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE} -p0 && cd ${CWD}

cd ${DOWNLOAD_PORTS_PATH}

cp ${CWD}/extra/vulkan_laylaos.h SwiftShader/include/vulkan/
cp ${CWD}/extra/VkDeviceMemoryExternalLaylaOS.hpp SwiftShader/src/Vulkan/
cp ${CWD}/extra/LaylaOSSurfaceKHR.cpp SwiftShader/src/WSI/
cp ${CWD}/extra/LaylaOSSurfaceKHR.hpp SwiftShader/src/WSI/
cp -r ${CWD}/extra/llvm/laylaos SwiftShader/third_party/llvm-10.0/configs/
cp -r ${CWD}/extra/llvm-subzero/LaylaOS SwiftShader/third_party/llvm-subzero/build/

# build
mkdir ${DOWNLOAD_SRCDIR}/build
cd ${DOWNLOAD_SRCDIR}/build

cmake .. --toolchain ${CWD}/../prep_cross_gui.cmake \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

# after config runs, modules glslang and googletest are downloaded and we
# need to patch them too
echo " ==> Patching ${DOWNLOAD_NAME} again"
echo " ==> Downloaded source is in ${DOWNLOAD_PORTS_PATH}"

cd ${DOWNLOAD_PORTS_PATH} && patch -i ${CWD}/${PATCH_FILE2} -p0 && cd ${DOWNLOAD_SRCDIR}/build

# and then re-run config
cmake .. --toolchain ${CWD}/../prep_cross_gui.cmake \
    || exit_failure "$0: failed to configure ${DOWNLOAD_NAME}"

# now build!
make || exit_failure "$0: failed to build ${DOWNLOAD_NAME}"

make install || exit_failure "$0: failed to install ${DOWNLOAD_NAME}"

# install
echo " ==> Installing the .so and the header files"

cp -r ../include/vulkan/ ${CROSSCOMPILE_SYSROOT_PATH}/usr/include
cp -r ../include/vk_video/ ${CROSSCOMPILE_SYSROOT_PATH}/usr/include
cp LaylaOS/libvulkan.so.1 ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib
ln -s libvulkan.so.1 ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libvulkan.so

# set the SONAME so everybody else can link to it without using the full pathname
patchelf --set-soname libvulkan.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libvulkan.so
patchelf --set-soname libSPIRV.so ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/libSPIRV.so

# As we installed the .so manually, we need to create a pkgconfig file for other
# software to be able to find this lib (save this as $SYSROOT/usr/lib/pkgconfig/vulkan.pc):

cat > ${CROSSCOMPILE_SYSROOT_PATH}/usr/lib/pkgconfig/vulkan.pc << EOF
# libvulcan pkg-config.
prefix=/usr
exec_prefix=\${prefix}
includedir=\${prefix}/include
libdir=\${exec_prefix}/lib

Name: vulkan
Description: Vulcan library (SwiftShader implementation)
Version: 1.0.0
Requires:
Conflicts:
Libs: -L\${libdir} -lvulkan
Libs.private:
Cflags: -I\${includedir}

EOF

# Clean up
cd ${CWD}
rm -rf ${DOWNLOAD_SRCDIR}

echo " ==>"
echo " ==> Finished building ${DOWNLOAD_NAME}"
echo " ==>"

