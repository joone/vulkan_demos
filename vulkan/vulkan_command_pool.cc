// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vulkan_command_pool.h"

#include <iostream>

#include "base/logging.h"
//#include "vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "vulkan_device_queue.h"
#include "vulkan_surface.h"
#include "vulkan_swap_chain.h"

namespace gpu {

VulkanCommandPool::VulkanCommandPool(VulkanDeviceQueue* device_queue,
                                     VulkanSwapChain* swap_chain)
    : device_queue_(device_queue), swap_chain_(swap_chain) {}

VulkanCommandPool::~VulkanCommandPool() {
  DCHECK_EQ(0u, command_buffer_count_);
  DCHECK_EQ(static_cast<VkCommandPool>(VK_NULL_HANDLE), handle_);
}

// Swap chains is connected with drawing and preparing command buffers.
// Execute commands on a device we submit them to queues through command
// buffers.
bool VulkanCommandPool::Initialize(VkCommandPoolCreateFlags flags) {
  printf("VulkanCommandPool::%s\n", __func__);

  VkDevice vk_device = device_queue_->GetVulkanDevice();

  // CreateCommandPool
  /*VkCommandPoolCreateInfo cmd_pool_create_info = {
    VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,     // VkStructureType sType
    nullptr,                                        // const void* pNext flags,
  // VkCommandPoolCreateFlags     flags
    device_queue_->GetPresentQueueFamilyIndex()     // uint32_t queueFamilyIndex
  };*/

  // for tutorial4 (this value should be passed through a parameter)
  VkCommandPoolCreateInfo cmd_pool_create_info = {
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,  // VkStructureType sType
      nullptr,  // const void                    *pNext
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |  // VkCommandPoolCreateFlags
                                                         // flags
          VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      device_queue_->GetPresentQueueFamilyIndex()  // uint32_t queueFamilyIndex
  };

  if (vkCreateCommandPool(vk_device, &cmd_pool_create_info, nullptr,
                          &handle_) != VK_SUCCESS) {
    std::cout << "Could not create a command pool!" << std::endl;
    return false;
  }

  return true;
}

void VulkanCommandPool::Destroy() {
  DCHECK_EQ(0u, command_buffer_count_);
  if (VK_NULL_HANDLE != handle_) {
    vkDestroyCommandPool(device_queue_->GetVulkanDevice(), handle_, nullptr);
    handle_ = VK_NULL_HANDLE;
  }
}

bool VulkanCommandPool::CreateCommandBuffer(VkCommandBuffer* command_buffer) {
  printf("VulkanCommandPool::%s\n", __func__);
  VkDevice device = device_queue_->GetVulkanDevice();
  // AllocateCommandBuffers
  VkCommandBufferAllocateInfo cmd_buffer_allocate_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,  // VkStructureType sType
      nullptr,  // const void*                  pNext
      handle_,  // VkCommandPool                commandPool
      VK_COMMAND_BUFFER_LEVEL_PRIMARY,  // VkCommandBufferLevel         level
      1  // FIXME: no hard coded                // uint32_t bufferCount
  };
  if (vkAllocateCommandBuffers(device, &cmd_buffer_allocate_info,
                               command_buffer) != VK_SUCCESS) {
    std::cout << "Could not allocate command buffers!" << std::endl;
    return false;
  }
  return true;
}

bool VulkanCommandPool::Submit(VkCommandBuffer* command_buffer,
                               uint32_t num_wait_semaphores,
                               VkSemaphore* wait_semaphores,
                               uint32_t num_signal_semaphores,
                               VkSemaphore* signal_semaphores) {
  // For Tutorial2
  // VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  // for  Tutorial3
  VkPipelineStageFlags wait_dst_stage_mask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submit_info = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO,  // VkStructureType              sType
      nullptr,                        // const void                  *pNext
      num_wait_semaphores,    // uint32_t                     waitSemaphoreCount
      wait_semaphores,        // const VkSemaphore           *pWaitSemaphores
      &wait_dst_stage_mask,   // const VkPipelineStageFlags  *pWaitDstStageMask;
      1,                      // uint32_t                     commandBufferCount
      command_buffer,         // const VkCommandBuffer       *pCommandBuffers
      num_signal_semaphores,  // uint32_t signalSemaphoreCount
      signal_semaphores       // const VkSemaphore           *pSignalSemaphores
  };

  if (vkQueueSubmit(device_queue_->GetPresentQueue(), 1, &submit_info,
                    VK_NULL_HANDLE) != VK_SUCCESS) {
    return false;
  }

  return VK_SUCCESS;
}

}  // namespace gpu
