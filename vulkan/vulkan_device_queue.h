// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_DEVICE_QUEUE_H_
#define GPU_VULKAN_VULKAN_DEVICE_QUEUE_H_

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "gpu/vulkan/vulkan_export.h"

namespace gpu {

class VulkanCommandPool;
class VulkanSurface;
class VulkanSwapChain;

class VULKAN_EXPORT VulkanDeviceQueue {
 public:
  enum DeviceQueueOption {
    GRAPHICS_QUEUE_FLAG = 0x01,
    PRESENTATION_SUPPORT_QUEUE_FLAG = 0x02,
  };

  VulkanDeviceQueue();
  ~VulkanDeviceQueue();

  bool Initialize(uint32_t option);
  void Destroy();

  VkPhysicalDevice GetVulkanPhysicalDevice() const {
    DCHECK_NE(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE),
              vk_physical_device_);
    return vk_physical_device_;
  }

  VkDevice GetVulkanDevice() const {
    DCHECK_NE(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
    return vk_device_;
  }

  VkQueue GetPresentQueue() const {
    //  DCHECK_NE(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);
    return PresentQueue_;
  }

  VkQueue GetGraphicsQueue() const {
    //  DCHECK_NE(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);
    return GraphicsQueue_;
  }

  uint32_t GetPresentQueueFamilyIndex() const {
    return vk_present_queue_family_index_;
  }
  uint32_t GetGraphicsQueueFamilyIndex() const {
    return vk_graphics_queue_family_index_;
  }
  bool OnWindowSizeChanged();
  bool ReadyToDraw() { return CanRender_; }

  std::unique_ptr<gpu::VulkanCommandPool> CreateCommandPool(VulkanSwapChain*,
      VkCommandPoolCreateFlags command_pool_create_flags);

  void CanRender(bool val) { CanRender_ = val; }

 private:
  VkPhysicalDevice vk_physical_device_ = VK_NULL_HANDLE;
  VkDevice vk_device_ = VK_NULL_HANDLE;
  VkQueue GraphicsQueue_ = VK_NULL_HANDLE;
  VkQueue PresentQueue_ = VK_NULL_HANDLE;

  uint32_t vk_graphics_queue_family_index_ = UINT32_MAX;
  uint32_t vk_present_queue_family_index_ = UINT32_MAX;

  bool CanRender_ = false;

  bool CheckExtensionAvailability(
      const char* extension_name,
      const std::vector<VkExtensionProperties>& available_extensions);
  bool CheckPhysicalDeviceProperties(
      VkPhysicalDevice vk_physical_device,
      uint32_t& selected_graphics_queue_family_index,
      uint32_t& selected_present_queue_family_index);

  DISALLOW_COPY_AND_ASSIGN(VulkanDeviceQueue);
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_DEVICE_QUEUE_H_
