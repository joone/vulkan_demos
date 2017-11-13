// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vulkan_surface.h"

#include <iostream>

#include "base/macros.h"
//#include "gpu/vulkan/vulkan_command_buffer.h"
#include "vulkan_device_queue.h"
#include "vulkan_implementation.h"
#include "gpu/vulkan/vulkan_platform.h"
#include "vulkan_swap_chain.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"
#endif  // defined(USE_X11)

namespace gpu {

namespace {
/*
const VkFormat kPreferredVkFormats32[] = {
    VK_FORMAT_B8G8R8A8_UNORM,  // FORMAT_BGRA8888,
    VK_FORMAT_R8G8B8A8_UNORM,  // FORMAT_RGBA8888,
};

const VkFormat kPreferredVkFormats16[] = {
    VK_FORMAT_R5G6B5_UNORM_PACK16,  // FORMAT_RGB565,
};
*/

}  // namespace

class VulkanWSISurface : public VulkanSurface {
 public:
  explicit VulkanWSISurface(gfx::AcceleratedWidget window) : 
      window_(window), surface_(VK_NULL_HANDLE) {}

  ~VulkanWSISurface() override {
    DCHECK_EQ(static_cast<VkSurfaceKHR>(VK_NULL_HANDLE), surface_);
  }

  bool CreateSurface() override {
    VkXlibSurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,   // VkStructureType                sType
      nullptr,                                          // const void                    *pNext
      0,                                                // VkXlibSurfaceCreateFlagsKHR    flags
      gfx::GetXDisplay(),                                // Display                       *dpy
      window_                                     // Window                         window
    };

    if(vkCreateXlibSurfaceKHR(GetVulkanInstance(),
        &surface_create_info, nullptr, &surface_) == VK_SUCCESS) {
      return true;
    }

    std::cout << "Could not create presentation surface!" << std::endl;
    return false;
  }

  bool Initialize(VulkanDeviceQueue* device_queue,
                  VulkanSurface::Format format) override {
    printf("VulkanWSISurface::%s\n", __func__);

    // from CreateSwapChain(); 
    if( device_queue->GetVulkanDevice() != VK_NULL_HANDLE ) {
      vkDeviceWaitIdle(device_queue->GetVulkanDevice());
    }

    VkSurfaceCapabilitiesKHR surface_capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_queue->GetVulkanPhysicalDevice(),
        surface_, &surface_capabilities ) != VK_SUCCESS ) {
      std::cout << "Could not check presentation surface capabilities!" << std::endl;
      return false;
    }

     // Get list of supported formats.
    uint32_t formats_count;
    if ((vkGetPhysicalDeviceSurfaceFormatsKHR(device_queue->GetVulkanPhysicalDevice(), surface_,
        &formats_count, nullptr ) != VK_SUCCESS) || (formats_count == 0) ) {
      std::cout << "Error occurred during presentation surface formats enumeration!" << std::endl;
      return false;
    }

    std::vector<VkSurfaceFormatKHR> surface_formats( formats_count );
    if( vkGetPhysicalDeviceSurfaceFormatsKHR(device_queue->GetVulkanPhysicalDevice(), surface_,
        &formats_count, &surface_formats[0] ) != VK_SUCCESS ) {
      std::cout << "Error occurred during presentation surface formats enumeration!" << std::endl;
      return false;
    }

    uint32_t present_modes_count;
    if ((vkGetPhysicalDeviceSurfacePresentModesKHR(device_queue->GetVulkanPhysicalDevice(), surface_,
        &present_modes_count, nullptr ) != VK_SUCCESS) || (present_modes_count == 0) ) {
      std::cout << "Error occurred during presentation surface present modes enumeration!" << std::endl;
      return false;
    }

    if (!swap_chain_.Initialize(device_queue, surface_, surface_capabilities,
                                surface_formats)) {
      return false;
    }

    printf("VulkanWSISurface::%s_end\n", __func__);
    return true;
  }

  void Destroy() override {
    swap_chain_.Destroy();
    vkDestroySurfaceKHR(GetVulkanInstance(), surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
  }

  gfx::SwapResult SwapBuffers() override { return swap_chain_.SwapBuffers(); }
  VulkanSwapChain* GetSwapChain() override { return &swap_chain_; }
//  void Finish() override { vkQueueWaitIdle(device_queue_->GetVulkanQueue()); }
  VkSurfaceKHR handle() override { return surface_; }

 protected:
  gfx::AcceleratedWidget window_;
  gfx::Size size_;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkSurfaceFormatKHR surface_format_ = {};
//  VulkanDeviceQueue* device_queue_ = nullptr;
  VulkanSwapChain swap_chain_;
};

VulkanSurface::~VulkanSurface() {}

// static
std::unique_ptr<VulkanSurface> VulkanSurface::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  return std::unique_ptr<VulkanSurface>(new VulkanWSISurface(window));
}

VulkanSurface::VulkanSurface() {}

}  // namespace gpu
