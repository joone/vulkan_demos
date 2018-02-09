// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_SWAP_CHAIN_H_
#define GPU_VULKAN_VULKAN_SWAP_CHAIN_H_

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "gpu/vulkan/vulkan_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/swap_result.h"

namespace gpu {

// class VulkanCommandBuffer;
class VulkanCommandPool;
class VulkanDeviceQueue;
class VulkanImageView;

class VulkanSwapChain {
 public:
  VulkanSwapChain();
  ~VulkanSwapChain();

  bool Initialize(VulkanDeviceQueue* device_queue,
                  VkSurfaceKHR surface,
                  const VkSurfaceCapabilitiesKHR& surface_caps,
                  const std::vector<VkSurfaceFormatKHR>);
  void Destroy();

  gfx::SwapResult SwapBuffers();

  uint32_t num_images() const { return static_cast<uint32_t>(images_.size()); }
  uint32_t current_image() const { return current_image_; }
  const gfx::Size& size() const { return size_; }

  VulkanImageView* GetImageView(uint32_t index) const {
    DCHECK_LT(index, images_.size());
    return images_[index]->image_view.get();
  }

  VulkanImageView* GetCurrentImageView() const {
    return GetImageView(current_image_);
  }

  VulkanCommandPool* GetCurrentCommandPool() const {
    return command_pool_.get();
  }

  VkCommandBuffer* GetCommandBuffer(uint32_t index) const {
    DCHECK_LT(index, images_.size());
    return &images_[index]->command_buffer;
  }

  VkSemaphore* GetImageAvailableSemaphore(uint32_t index) {
    return &images_[index]->render_semaphore;
  }

  VkSemaphore* GetFinishedRenderingSemaphore(uint32_t index) {
    return &images_[index]->present_semaphore;
  }
  VkFence* GetFence(uint32_t index) { return &images_[index]->Fence; }

  VkSwapchainKHR handle() const { return swap_chain_; }
  VkFormat format() const { return format_; }
  VkExtent2D GetExtent() const { return extent_; }

  VkImage GetImage(uint32_t index) { return images_[index]->image; }

  bool CreateFences();
  bool WaitFences(uint32_t* resource_index, uint32_t* image_index);
  bool SwapBuffer2(uint32_t resource_index, uint32_t* image_index);

 private:
  bool InitializeSwapChain(VkSurfaceKHR surface,
                           const VkSurfaceCapabilitiesKHR& surface_caps,
                           const std::vector<VkSurfaceFormatKHR>);
  void DestroySwapChain();

  bool InitializeSwapImages(const VkSurfaceCapabilitiesKHR& surface_caps,
                            const std::vector<VkSurfaceFormatKHR>);
  void DestroySwapImages();

  uint32_t GetSwapChainNumImages(
      const VkSurfaceCapabilitiesKHR& surface_capabilities);
  VkSurfaceFormatKHR GetSwapChainFormat(
      const std::vector<VkSurfaceFormatKHR>& surface_formats);
  VkExtent2D GetSwapChainExtent(
      const VkSurfaceCapabilitiesKHR& surface_capabilities);
  VkSurfaceTransformFlagBitsKHR GetSwapChainTransform(
      const VkSurfaceCapabilitiesKHR& surface_capabilities);
  VkImageUsageFlags GetSwapChainUsageFlags(
      const VkSurfaceCapabilitiesKHR& surface_capabilities);
  VkPresentModeKHR GetSwapChainPresentMode(
      std::vector<VkPresentModeKHR>& present_modes);

  VulkanDeviceQueue* device_queue_;
  VkSwapchainKHR swap_chain_ = VK_NULL_HANDLE;

  std::unique_ptr<VulkanCommandPool> command_pool_;

  gfx::Size size_;

  struct ImageData {
    ImageData();
    ~ImageData();

    VkImage image = VK_NULL_HANDLE;
    std::unique_ptr<VulkanImageView> image_view;
    // std::unique_ptr<VulkanCommandBuffer> command_buffer;
    VkCommandBuffer command_buffer;
    VkSampler Sampler;
    VkDeviceMemory Memory;

    // Image Available
    VkSemaphore render_semaphore = VK_NULL_HANDLE;
    // Rendering Finished
    VkSemaphore present_semaphore = VK_NULL_HANDLE;
    // For Tutorial 4
    VkFence Fence;
  };

  std::vector<std::unique_ptr<ImageData>> images_;
  VkExtent2D extent_;
  uint32_t current_image_ = 0;
  uint32_t image_count_ = 0;

  VkFormat format_ = VK_FORMAT_UNDEFINED;
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_SWAP_CHAIN_H_
