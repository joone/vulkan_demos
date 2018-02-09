// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_COMMAND_POOL_H_
#define GPU_VULKAN_VULKAN_COMMAND_POOL_H_

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "base/macros.h"

namespace gpu {

class VulkanCommandBuffer;
class VulkanDeviceQueue;
class VulkanSwapChain;

class VulkanCommandPool {
 public:
  explicit VulkanCommandPool(VulkanDeviceQueue* device_queue,
                             VulkanSwapChain* swap_chain);
  ~VulkanCommandPool();

  bool Initialize(VkCommandPoolCreateFlags flags = 0);
  void Destroy();

  bool CreateCommandBuffer(VkCommandBuffer* command_buffer);
  std::unique_ptr<VulkanCommandBuffer> CreatePrimaryCommandBuffer();
  std::unique_ptr<VulkanCommandBuffer> CreateSecondaryCommandBuffer();
  bool Submit(VkCommandBuffer* command_buffer,
              uint32_t num_wait_semaphores,
              VkSemaphore* wait_semaphores,
              uint32_t num_signal_semaphores,
              VkSemaphore* signal_semaphores);

  VkCommandPool handle() { return handle_; }

 private:
  friend class VulkanCommandBuffer;

  void IncrementCommandBufferCount();
  void DecrementCommandBufferCount();

  VulkanDeviceQueue* device_queue_;
  VulkanSwapChain* swap_chain_;
  VkCommandPool handle_ = VK_NULL_HANDLE;
  uint32_t command_buffer_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(VulkanCommandPool);
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_COMMAND_POOL_H_
