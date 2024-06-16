// Copyright 2019 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "LaylaOSSurfaceKHR.hpp"

#include "System/Debug.hpp"
#include "Vulkan/VkDeviceMemory.hpp"

#include <string.h>
#include <gui/event.h>
#include <gui/bitmap.h>
#include <gui/gc.h>
#include <gui/client/window.h>

namespace {
VkResult getWindowSize(const void *wptr, VkExtent2D &windowSize)
{
	struct window_t *win = (struct window_t *)wptr;
	struct window_attribs_t attribs;

	if(!win || !get_win_attribs(win->winid, &attribs))
	{
		windowSize = { 0, 0 };
		return VK_ERROR_SURFACE_LOST_KHR;
	}

	windowSize = { static_cast<uint32_t>(attribs.w),
		           static_cast<uint32_t>(attribs.h) };

	return VK_SUCCESS;
}
}  // namespace

namespace vk {

LaylaOSSurfaceKHR::LaylaOSSurfaceKHR(const VkLaylaOSSurfaceCreateInfoKHR *pCreateInfo, void *mem)
    : window(pCreateInfo->window)
{
	ASSERT(window != nullptr);
}

void LaylaOSSurfaceKHR::destroySurface(const VkAllocationCallbacks *pAllocator)
{

}

size_t LaylaOSSurfaceKHR::ComputeRequiredAllocationSize(const VkLaylaOSSurfaceCreateInfoKHR *pCreateInfo)
{
	return 0;
}

VkResult LaylaOSSurfaceKHR::getSurfaceCapabilities(const void *pSurfaceInfoPNext, VkSurfaceCapabilitiesKHR *pSurfaceCapabilities, void *pSurfaceCapabilitiesPNext) const
{
	VkExtent2D extent;

	if(getWindowSize(window, extent) != VK_SUCCESS)
	{
		return VK_ERROR_SURFACE_LOST_KHR;
	}

	pSurfaceCapabilities->currentExtent = extent;
	pSurfaceCapabilities->minImageExtent = extent;
	pSurfaceCapabilities->maxImageExtent = extent;

	setCommonSurfaceCapabilities(pSurfaceInfoPNext, pSurfaceCapabilities, pSurfaceCapabilitiesPNext);
	return VK_SUCCESS;
}

void LaylaOSSurfaceKHR::attachImage(PresentImage *image)
{
	// Nothing to do here, the current implementation based on GDI blits on
	// present instead of associating the image with the surface.
}

void LaylaOSSurfaceKHR::detachImage(PresentImage *image)
{
	// Nothing to do here, the current implementation based on GDI blits on
	// present instead of associating the image with the surface.
}

VkResult LaylaOSSurfaceKHR::present(PresentImage *image)
{
	VkExtent2D windowExtent = {};
	VkResult result = getWindowSize(window, windowExtent);
	if(result != VK_SUCCESS)
	{
		return result;
	}

	const Image *vkImage = image->getImage();
	const VkExtent3D &extent = vkImage->getExtent();
	if(windowExtent.width != extent.width || windowExtent.height != extent.height)
	{
		return VK_ERROR_OUT_OF_DATE_KHR;
	}

	int stride = vkImage->rowPitchBytes(VK_IMAGE_ASPECT_COLOR_BIT, 0);
	int bytesPerPixel = static_cast<int>(vkImage->getFormat(VK_IMAGE_ASPECT_COLOR_BIT).bytes());

	void *bits = image->getImage()->getTexelPointer({ 0, 0, 0 }, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 });

	struct bitmap32_t bmp;
	bmp.width = stride / bytesPerPixel;
	bmp.height = extent.height;
	bmp.data = (uint32_t *)bits;

	gc_blit_bitmap(((struct window_t *)window)->gc, &bmp, 0, 0, 0, 0, extent.width, extent.height);
	window_invalidate((struct window_t *)window);

	return VK_SUCCESS;
}

}  // namespace vk
