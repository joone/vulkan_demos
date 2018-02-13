// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vulkan_swap_chain.h"

#include <iostream>
//#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_image_view.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "vulkan_command_pool.h"
#include "vulkan_device_queue.h"

namespace gpu {

VulkanSwapChain::VulkanSwapChain() {}

VulkanSwapChain::~VulkanSwapChain() {
  // DCHECK(images_.empty());
  DCHECK_EQ(static_cast<VkSwapchainKHR>(VK_NULL_HANDLE), swap_chain_);
  //  DCHECK_EQ(static_cast<VkSemaphore>(VK_NULL_HANDLE),
  //  next_present_semaphore_);
}

bool VulkanSwapChain::Initialize(
    VulkanDeviceQueue* device_queue,
    VkSurfaceKHR surface,
    const VkSurfaceCapabilitiesKHR& surface_caps,
    const std::vector<VkSurfaceFormatKHR> surface_formats,
    VkCommandPoolCreateFlags command_pool_create_flags) {
  DCHECK(device_queue);
  device_queue_ = device_queue;
  return InitializeSwapChain(surface, surface_caps, surface_formats) &&
         InitializeSwapImages(surface_caps, surface_formats,
             command_pool_create_flags);
}

void VulkanSwapChain::Destroy() {
  DestroySwapImages();
  DestroySwapChain();
}

gfx::SwapResult VulkanSwapChain::SwapBuffers() {
  VkDevice device = device_queue_->GetVulkanDevice();
  VkQueue queue = device_queue_->GetPresentQueue();

  uint32_t image_index;

  std::unique_ptr<ImageData>& current_image_data = images_[current_image_];
  // Acquire then next image.
  VkResult result = vkAcquireNextImageKHR(device, swap_chain_, UINT64_MAX,
                                          current_image_data->render_semaphore,
                                          VK_NULL_HANDLE, &image_index);
  current_image_ = image_index;
  switch (result) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
      break;
    case VK_ERROR_OUT_OF_DATE_KHR:
      if (device_queue_->OnWindowSizeChanged())
        return gfx::SwapResult::SWAP_ACK;
      break;
    default:
      std::cout << "Problem occurred during swap chain image acquisition!"
                << std::endl;
      return gfx::SwapResult::SWAP_FAILED;

  }

  // Submit our command buffer for the current buffer.
  // GetCurrentCommandBuffer::Submit() in Chromium.
  // vkQueueSubmit is called.
  if (GetCurrentCommandPool()->Submit(&current_image_data->command_buffer, 1,
                                      &current_image_data->render_semaphore, 1,
                                      &current_image_data->present_semaphore) !=
      VK_SUCCESS) {
    return gfx::SwapResult::SWAP_FAILED;
  }

  VkPresentInfoKHR present_info = {
      VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,  // VkStructureType        sType
      nullptr,                   // const void             *pNext
      1,                         // waitSemaphoreCount
      &current_image_data->present_semaphore,  // const VkSemaphore         
      1,                         // uint32_t                     swapchainCount
      &swap_chain_,              // const VkSwapchainKHR        *pSwapchains
      &image_index,              // const uint32_t              *pImageIndices
      nullptr                    // VkResult                    *pResults
  };

  result = vkQueuePresentKHR(queue, &present_info);

  switch (result) {
    case VK_SUCCESS:
      break;
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_SUBOPTIMAL_KHR:
      if (device_queue_->OnWindowSizeChanged())
        return gfx::SwapResult::SWAP_ACK;
      break;
    default:
      std::cout << "Problem occurred during image presentation!" << std::endl;
      return gfx::SwapResult::SWAP_FAILED;
  }

  return gfx::SwapResult::SWAP_ACK;
}

bool VulkanSwapChain::InitializeSwapChain(
    VkSurfaceKHR surface,
    const VkSurfaceCapabilitiesKHR& surface_capabilities,
    const std::vector<VkSurfaceFormatKHR> surface_formats) {
  VkDevice device = device_queue_->GetVulkanDevice();
  // VkResult result = VK_SUCCESS;

  device_queue_->CanRender(false);

  if (device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device);
  }

  // FIXME: Don't need to clear images.
  for (size_t i = 0; i < images_.size(); ++i) {
    std::unique_ptr<ImageData>& image_data = images_[i];
    if (!image_data->image_view.get()) {
      vkDestroyImageView(device, image_data->image_view->handle(), nullptr);
      //  images_[i].image_view = VK_NULL_HANDLE;
    }
  }
  images_.clear();

  uint32_t present_modes_count;
  if ((vkGetPhysicalDeviceSurfacePresentModesKHR(
           device_queue_->GetVulkanPhysicalDevice(), surface,
           &present_modes_count, nullptr) != VK_SUCCESS) ||
      (present_modes_count == 0)) {
    std::cout << "Error occurred during presentation surface present modes "
                 "enumeration!"
              << std::endl;
    return false;
  }

  std::vector<VkPresentModeKHR> present_modes(present_modes_count);
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(
          device_queue_->GetVulkanPhysicalDevice(), surface,
          &present_modes_count, &present_modes[0]) != VK_SUCCESS) {
    std::cout << "Error occurred during presentation surface present modes "
                 "enumeration!"   << std::endl;
    return false;
  }

  uint32_t desired_number_of_images =
      GetSwapChainNumImages(surface_capabilities);
  VkSurfaceFormatKHR desired_format = GetSwapChainFormat(surface_formats);
  VkExtent2D desired_extent = GetSwapChainExtent(surface_capabilities);
  VkImageUsageFlags desired_usage =
      GetSwapChainUsageFlags(surface_capabilities);
  VkSurfaceTransformFlagBitsKHR desired_transform =
      GetSwapChainTransform(surface_capabilities);
  VkPresentModeKHR desired_present_mode =
      GetSwapChainPresentMode(present_modes);
  VkSwapchainKHR old_swap_chain = swap_chain_;

  if (static_cast<int>(desired_usage) == -1) {
    return false;
  }
  if (static_cast<int>(desired_present_mode) == -1) {
    return false;
  }
  if ((desired_extent.width == 0) || (desired_extent.height == 0)) {
    // Current surface size is (0, 0) so we can't create a swap chain and render
    // anything (CanRender == false) But we don't wont to kill the application
    // as this situation may occur i.e. when window gets minimized
    return true;
  }

  VkSwapchainCreateInfoKHR swap_chain_create_info = {
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,  // VkStructureType sType
      nullptr,                   // const void                    *pNext
      0,                         // VkSwapchainCreateFlagsKHR      flags
      surface,                   // VkSurfaceKHR                   surface
      desired_number_of_images,  // uint32_t                       minImageCount
      desired_format.format,     // VkFormat                       imageFormat
      desired_format.colorSpace,  // VkColorSpaceKHR imageColorSpace
      desired_extent,             // VkExtent2D                     imageExtent
      1,              // uint32_t                       imageArrayLayers
      desired_usage,  // VkImageUsageFlags              imageUsage
      VK_SHARING_MODE_EXCLUSIVE,  // VkSharingMode imageSharingMode
      0,        // uint32_t                       queueFamilyIndexCount
      nullptr,  // const uint32_t                *pQueueFamilyIndices
      desired_transform,  // VkSurfaceTransformFlagBitsKHR  preTransform
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,  // VkCompositeAlphaFlagBitsKHR
                                          // compositeAlpha
      desired_present_mode,  // VkPresentModeKHR               presentMode
      VK_TRUE,               // VkBool32                       clipped
      old_swap_chain         // VkSwapchainKHR                 oldSwapchain
  };

  if (vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr,
                           &swap_chain_) != VK_SUCCESS) {
    std::cout << "Could not create swap chain!" << std::endl;
    return false;
  }
  if (old_swap_chain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, old_swap_chain, nullptr);
  }

  format_ = desired_format.format;
  uint32_t image_count = 0;
  if ((vkGetSwapchainImagesKHR(device, swap_chain_, &image_count, nullptr) !=
       VK_SUCCESS) ||
      (image_count == 0)) {
    std::cout << "Could not get swap chain images!" << std::endl;
    return false;
  }

  image_count_ = image_count;
  images_.resize(image_count);

  printf("  num of images = %d\n", image_count);
  std::vector<VkImage> images(image_count);
  if (vkGetSwapchainImagesKHR(device, swap_chain_, &image_count, &images[0]) !=
      VK_SUCCESS) {
    std::cout << "Could not get swap chain images!" << std::endl;
    return false;
  }

  for (size_t i = 0; i < images_.size(); ++i) {
    images_[i].reset(new ImageData);
    std::unique_ptr<ImageData>& image_data = images_[i];
    image_data->image = images[i];
    image_data->image_view.reset(new VulkanImageView(device_queue_));
    if (!image_data->image_view->Initialize(
            images[i], VK_IMAGE_VIEW_TYPE_2D, VulkanImageView::IMAGE_TYPE_COLOR,
            format_, size_.width(), size_.height(), 0, 1, 0, 1)) {
      return false;
    }
  }

  extent_ = desired_extent;
  device_queue_->CanRender(true);
  printf("VulkanSwapChain::%s_end\n", __func__);
  return true;
}

uint32_t VulkanSwapChain::GetSwapChainNumImages(
    const VkSurfaceCapabilitiesKHR& surface_capabilities) {
  // Set of images defined in a swap chain may not always be available for
  // application to render to: One may be displayed and one may wait in a queue
  // to be presented If application wants to use more images at the same time it
  // must ask for more images
  uint32_t image_count = surface_capabilities.minImageCount + 1;
  if ((surface_capabilities.maxImageCount > 0) &&
      (image_count > surface_capabilities.maxImageCount)) {
    image_count = surface_capabilities.maxImageCount;
  }
  return image_count;
}

VkSurfaceFormatKHR VulkanSwapChain::GetSwapChainFormat(
    const std::vector<VkSurfaceFormatKHR>& surface_formats) {
  // If the list contains only one entry with undefined format
  // it means that there are no preferred surface formats and any can be chosen
  if ((surface_formats.size() == 1) &&
      (surface_formats[0].format == VK_FORMAT_UNDEFINED)) {
    return {VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR};
  }

  // Check if list contains most widely used R8 G8 B8 A8 format
  // with nonlinear color space
  for (const VkSurfaceFormatKHR& surface_format : surface_formats) {
    if (surface_format.format == VK_FORMAT_R8G8B8A8_UNORM) {
      return surface_format;
    }
  }

  // Return the first format from the list
  return surface_formats[0];
}

VkExtent2D VulkanSwapChain::GetSwapChainExtent(
    const VkSurfaceCapabilitiesKHR& surface_capabilities) {
  // Special value of surface extent is width == height == -1
  // If this is so we define the size by ourselves but it must fit within
  // defined confines
  if (surface_capabilities.currentExtent.width == 0) {
    VkExtent2D swap_chain_extent = {640, 480};
    if (swap_chain_extent.width < surface_capabilities.minImageExtent.width) {
      swap_chain_extent.width = surface_capabilities.minImageExtent.width;
    }
    if (swap_chain_extent.height < surface_capabilities.minImageExtent.height) {
      swap_chain_extent.height = surface_capabilities.minImageExtent.height;
    }
    if (swap_chain_extent.width > surface_capabilities.maxImageExtent.width) {
      swap_chain_extent.width = surface_capabilities.maxImageExtent.width;
    }
    if (swap_chain_extent.height > surface_capabilities.maxImageExtent.height) {
      swap_chain_extent.height = surface_capabilities.maxImageExtent.height;
    }
    return swap_chain_extent;
  }

  // Most of the cases we define size of the swap_chain images equal to current
  // window's size
  return surface_capabilities.currentExtent;
}

VkImageUsageFlags VulkanSwapChain::GetSwapChainUsageFlags(
    const VkSurfaceCapabilitiesKHR& surface_capabilities) {
  // Color attachment flag must always be supported
  // We can define other usage flags but we always need to check if they are
  // supported
  if (surface_capabilities.supportedUsageFlags &
      VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
    return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
           VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }
  std::cout << "VK_IMAGE_USAGE_TRANSFER_DST image usage is not supported by "
               "the swap chain!"
            << std::endl
            << "Supported swap chain's image usages include:" << std::endl
            << (surface_capabilities.supportedUsageFlags &
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                    ? "    VK_IMAGE_USAGE_TRANSFER_SRC\n"
                    : "")
            << (surface_capabilities.supportedUsageFlags &
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT
                    ? "    VK_IMAGE_USAGE_TRANSFER_DST\n"
                    : "")
            << (surface_capabilities.supportedUsageFlags &
                        VK_IMAGE_USAGE_SAMPLED_BIT
                    ? "    VK_IMAGE_USAGE_SAMPLED\n"
                    : "")
            << (surface_capabilities.supportedUsageFlags &
                        VK_IMAGE_USAGE_STORAGE_BIT
                    ? "    VK_IMAGE_USAGE_STORAGE\n"
                    : "")
            << (surface_capabilities.supportedUsageFlags &
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                    ? "    VK_IMAGE_USAGE_COLOR_ATTACHMENT\n"
                    : "")
            << (surface_capabilities.supportedUsageFlags &
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                    ? "    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT\n"
                    : "")
            << (surface_capabilities.supportedUsageFlags &
                        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
                    ? "    VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT\n"
                    : "")
            << (surface_capabilities.supportedUsageFlags &
                        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
                    ? "    VK_IMAGE_USAGE_INPUT_ATTACHMENT"
                    : "")
            << std::endl;
  return static_cast<VkImageUsageFlags>(-1);
}

VkSurfaceTransformFlagBitsKHR VulkanSwapChain::GetSwapChainTransform(
    const VkSurfaceCapabilitiesKHR& surface_capabilities) {
  // Sometimes images must be transformed before they are presented (i.e. due to
  // device's orienation being other than default orientation) If the specified
  // transform is other than current transform, presentation engine will
  // transform image during presentation operation; this operation may hit
  // performance on some platforms Here we don't want any transformations to
  // occur so if the identity transform is supported use it otherwise just use
  // the same transform as current transform
  if (surface_capabilities.supportedTransforms &
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
    return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  } else {
    return surface_capabilities.currentTransform;
  }
}

VkPresentModeKHR VulkanSwapChain::GetSwapChainPresentMode(
    std::vector<VkPresentModeKHR>& present_modes) {
  // FIFO present mode is always available
  // MAILBOX is the lowest latency V-Sync enabled mode (something like
  // triple-buffering) so use it if available
  for (VkPresentModeKHR& present_mode : present_modes) {
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return present_mode;
    }
  }
  for (VkPresentModeKHR& present_mode : present_modes) {
    if (present_mode == VK_PRESENT_MODE_FIFO_KHR) {
      return present_mode;
    }
  }
  std::cout << "FIFO present mode is not supported by the swap chain!"
            << std::endl;
  return static_cast<VkPresentModeKHR>(-1);
}

void VulkanSwapChain::DestroySwapChain() {
  VkDevice device = device_queue_->GetVulkanDevice();

  if (swap_chain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, swap_chain_, nullptr);
    swap_chain_ = VK_NULL_HANDLE;
  }
}

bool VulkanSwapChain::InitializeSwapImages(
    const VkSurfaceCapabilitiesKHR& surface_caps,
    const std::vector<VkSurfaceFormatKHR> surface_formats,
    VkCommandPoolCreateFlags command_pool_create_flags) {
  VkDevice device = device_queue_->GetVulkanDevice();

  command_pool_ = device_queue_->CreateCommandPool(this,
      command_pool_create_flags);
  if (!command_pool_)
    return false;

  VkSemaphoreCreateInfo semaphore_create_info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,  // VkStructureType sType
      nullptr,  // const void*              pNext
      0         // VkSemaphoreCreateFlags   flags
  };

  VkResult result = VK_SUCCESS;

  for (uint32_t i = 0; i < image_count_; ++i) {
    std::unique_ptr<ImageData>& image_data = images_[i];

    // Setup semaphores.
    result = vkCreateSemaphore(device, &semaphore_create_info, nullptr,
                               &image_data->render_semaphore);
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkCreateSemaphore(render) failed: " << result;
      return false;
    }

    result = vkCreateSemaphore(device, &semaphore_create_info, nullptr,
                               &image_data->present_semaphore);
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkCreateSemaphore(present) failed: " << result;
      return false;
    }

    // Initialize the command buffer for this buffer data.
    command_pool_->CreateCommandBuffer(&image_data->command_buffer);

    VkFenceCreateInfo fence_create_info = {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,  // VkStructureType
        nullptr,                      // const void *pNext
        VK_FENCE_CREATE_SIGNALED_BIT  // VkFenceCreateFlags
    };
    if (vkCreateFence(device, &fence_create_info, nullptr,
                      &image_data->Fence) != VK_SUCCESS) {
      std::cout << "Could not create a fence!" << std::endl;
      return false;
    }
  }  // end of for
  return true;
}

bool VulkanSwapChain::CreateFences() {
  VkDevice device = device_queue_->GetVulkanDevice();
  VkFenceCreateInfo fence_create_info = {
      VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,  // VkStructureType sType
      nullptr,                      // const void                    *pNext
      VK_FENCE_CREATE_SIGNALED_BIT  // VkFenceCreateFlags             flags
  };

  for (size_t i = 0; i < image_count_; ++i) {
    std::unique_ptr<ImageData>& image_data = images_[i];
    if (vkCreateFence(device, &fence_create_info, nullptr,
                      &image_data->Fence) != VK_SUCCESS) {
      std::cout << "Could not create a fence!" << std::endl;
      return false;
    }
  }
  return true;
}

// For tutorial4
bool VulkanSwapChain::WaitFences(uint32_t* resource_index,
                                 uint32_t* image_index) {
  std::unique_ptr<ImageData>& image_data = images_[*resource_index];
  VkDevice device = device_queue_->GetVulkanDevice();
  *resource_index = (*resource_index + 1) % image_count_;
  if (vkWaitForFences(device, 1, &image_data->Fence, VK_FALSE, 1000000000) !=
      VK_SUCCESS) {
    std::cout << "Waiting for fence takes too long!" << std::endl;
    return false;
  }
  vkResetFences(device, 1, &image_data->Fence);
  VkResult result = vkAcquireNextImageKHR(device, swap_chain_, UINT64_MAX,
                                          image_data->render_semaphore,
                                          VK_NULL_HANDLE, image_index);
  switch (result) {
    case VK_SUCCESS:
      break;
    case VK_SUBOPTIMAL_KHR:
      break;
    // case VK_ERROR_OUT_OF_DATE_KHR:
    //   return OnWindowSizeChanged();
    default:
      std::cout << "Problem occurred during swap chain image acquisition!"
                << std::endl;
      return false;
  }
  return true;
}

// for tutorial4
bool VulkanSwapChain::SwapBuffer2(uint32_t resource_index,
                                  uint32_t* image_index) {
  std::unique_ptr<ImageData>& image_data = images_[resource_index];
  VkQueue queue = device_queue_->GetPresentQueue();

  VkPipelineStageFlags wait_dst_stage_mask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO,  // VkStructureType              sType
      nullptr,                        // const void                  *pNext
      1,  // uint32_t                     waitSemaphoreCount
      &image_data->render_semaphore,  // const VkSemaphore *pWaitSemaphores
      &wait_dst_stage_mask,  // const VkPipelineStageFlags  *pWaitDstStageMask;
      1,                     // uint32_t                     commandBufferCount
      &image_data->command_buffer,  // const VkCommandBuffer *pCommandBuffers
      1,  // uint32_t                     signalSemaphoreCount
      &image_data->present_semaphore  // const VkSemaphore *pSignalSemaphores
  };

  if (vkQueueSubmit(queue, 1, &submit_info, image_data->Fence) != VK_SUCCESS) {
    return false;
  }

  VkPresentInfoKHR present_info = {
      VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,  // VkStructureType              sType
      nullptr,                             // const void                  *pNext
      1,  // uint32_t                     waitSemaphoreCount
      &image_data->present_semaphore,  // const VkSemaphore *pWaitSemaphores
      1,             // uint32_t                     swapchainCount
      &swap_chain_,  // const VkSwapchainKHR        *pSwapchains
      image_index,   // const uint32_t              *pImageIndices
      nullptr        // VkResult                    *pResults
  };

  int result = vkQueuePresentKHR(queue, &present_info);

  switch (result) {
    case VK_SUCCESS:
      break;
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_SUBOPTIMAL_KHR:
      break;
    //      return OnWindowSizeChanged();
    default:
      std::cout << "Problem occurred during image presentation!" << std::endl;
      return false;
  }
  return true;
}

void VulkanSwapChain::DestroySwapImages() {
  VkDevice device = device_queue_->GetVulkanDevice();
  for (const std::unique_ptr<ImageData>& image_data : images_) {
    // Destroy Image View.
    if (image_data->image_view) {
      image_data->image_view->Destroy();
      image_data->image_view.reset();
    }
    vkDestroySemaphore(device, image_data->render_semaphore, nullptr);
    vkDestroySemaphore(device, image_data->present_semaphore, nullptr);
    image_data->render_semaphore = VK_NULL_HANDLE;
    image_data->present_semaphore = VK_NULL_HANDLE;
  }
  command_pool_->Destroy();
}

VulkanSwapChain::ImageData::ImageData() {}

VulkanSwapChain::ImageData::~ImageData() {}

}  // namespace gpu
