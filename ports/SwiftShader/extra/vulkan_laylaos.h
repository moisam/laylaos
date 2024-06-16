#ifndef VULKAN_LAYLAOS_H_
#define VULKAN_LAYLAOS_H_ 1

/*
** Copyright 2015-2023 The Khronos Group Inc.
**
** SPDX-License-Identifier: Apache-2.0
*/

/*
** This header is generated from the Khronos Vulkan XML API Registry.
**
*/


#ifdef __cplusplus
extern "C" {
#endif



// VK_KHR_laylaos_surface is a preprocessor guard. Do not pass it to API calls.
#define VK_KHR_laylaos_surface 1
#define VK_KHR_LAYLAOS_SURFACE_SPEC_VERSION   6
#define VK_KHR_LAYLAOS_SURFACE_EXTENSION_NAME "VK_KHR_laylaos_surface"
typedef VkFlags VkLaylaOSSurfaceCreateFlagsKHR;
typedef struct VkLaylaOSSurfaceCreateInfoKHR {
    VkStructureType               sType;
    const void*                   pNext;
    VkLaylaOSSurfaceCreateFlagsKHR    flags;
    void*                  window;
} VkLaylaOSSurfaceCreateInfoKHR;

typedef VkResult (VKAPI_PTR *PFN_vkCreateLaylaOSSurfaceKHR)(VkInstance instance, const VkLaylaOSSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
typedef VkBool32 (VKAPI_PTR *PFN_vkGetPhysicalDeviceLaylaOSPresentationSupportKHR)(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkCreateLaylaOSSurfaceKHR(
    VkInstance                                  instance,
    const VkLaylaOSSurfaceCreateInfoKHR*        pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSurfaceKHR*                               pSurface);

VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceLaylaOSPresentationSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex);
#endif

#ifdef __cplusplus
}
#endif

#endif
