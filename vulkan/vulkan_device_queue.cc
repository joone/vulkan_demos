// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vulkan_device_queue.h"

#include <iostream>
#include <memory>
#include <unordered_set>
#include <vector>

#include "gpu/vulkan/vulkan_platform.h"
#include "vulkan_command_pool.h"
#include "vulkan_implementation.h"
#include "vulkan_surface.h"
#include "vulkan_swap_chain.h"

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include "ui/gfx/x/x11_types.h"
#endif  // defined(VK_USE_PLATFORM_XLIB_KHR)

namespace gpu {

VulkanDeviceQueue::VulkanDeviceQueue() {}

VulkanDeviceQueue::~VulkanDeviceQueue() {
  DCHECK_EQ(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE), vk_physical_device_);
  // DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
}

bool VulkanDeviceQueue::OnWindowSizeChanged() {
  printf("VulkanDeviceQueue::%s\n", __func__);

  /* Clear();

  if( !CreateSwapChain() ) {
     return false;
  }
  if( !CreateCommandBuffers() ) {
    return false;
  }
 */

  return true;
}

bool VulkanDeviceQueue::Initialize(uint32_t options) {
  printf("VulkanDeviceQueue::%s\n", __func__);
  VkInstance vk_instance = gpu::GetVulkanInstance();
  if (VK_NULL_HANDLE == vk_instance)
    return false;

  //
  // from CreateDevice()
  //
  uint32_t num_devices = 0;
  if ((vkEnumeratePhysicalDevices(vk_instance, &num_devices, nullptr) !=
       VK_SUCCESS) ||
      (num_devices == 0)) {
    std::cout << "Error occurred during physical devices enumeration!"
              << std::endl;
    return false;
  }

  std::vector<VkPhysicalDevice> physical_devices(num_devices);
  if (vkEnumeratePhysicalDevices(vk_instance, &num_devices,
                                 &physical_devices[0]) != VK_SUCCESS) {
    std::cout << "Error occurred during physical devices enumeration!"
              << std::endl;
    return false;
  }

  uint32_t selected_graphics_queue_family_index = UINT32_MAX;
  uint32_t selected_present_queue_family_index = UINT32_MAX;

  for (uint32_t i = 0; i < num_devices; ++i) {
    if (CheckPhysicalDeviceProperties(physical_devices[i],
                                      selected_graphics_queue_family_index,
                                      selected_present_queue_family_index)) {
      vk_physical_device_ = physical_devices[i];
    }
  }

  if (vk_physical_device_ == VK_NULL_HANDLE) {
    std::cout
        << "Could not select physical device based on the chosen properties!"
        << std::endl;
    return false;
  }

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
  std::vector<float> queue_priorities = {1.0f};

  queue_create_infos.push_back({
      VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,  // VkStructureType sType
      nullptr,  // const void                  *pNext
      0,        // VkDeviceQueueCreateFlags     flags
      selected_graphics_queue_family_index,  // uint32_t queueFamilyIndex
      static_cast<uint32_t>(queue_priorities.size()),  // uint32_t queueCount
      &queue_priorities[0]  // const float                 *pQueuePriorities
  });

  if (selected_graphics_queue_family_index !=
      selected_present_queue_family_index) {
    queue_create_infos.push_back({
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,  // VkStructureType sType
        nullptr,  // const void                  *pNext
        0,        // VkDeviceQueueCreateFlags     flags
        selected_present_queue_family_index,  // uint32_t queueFamilyIndex
        static_cast<uint32_t>(queue_priorities.size()),  // uint32_t queueCount
        &queue_priorities[0]  // const float                 *pQueuePriorities
    });
  }

  std::vector<const char*> extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkDeviceCreateInfo device_create_info = {
      VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,  // VkStructureType sType
      nullptr,  // const void                        *pNext
      0,        // VkDeviceCreateFlags                flags
      static_cast<uint32_t>(
          queue_create_infos.size()),  // uint32_t queueCreateInfoCount
      &queue_create_infos[0],          // const VkDeviceQueueCreateInfo
                                       // *pQueueCreateInfos
      0,        // uint32_t                           enabledLayerCount
      nullptr,  // const char * const                *ppEnabledLayerNames
      static_cast<uint32_t>(
          extensions.size()),  // uint32_t enabledExtensionCount
      &extensions[0],          // const char * const *ppEnabledExtensionNames
      nullptr  // const VkPhysicalDeviceFeatures    *pEnabledFeatures
  };

  if (vkCreateDevice(vk_physical_device_, &device_create_info, nullptr,
                     &vk_device_) != VK_SUCCESS) {
    std::cout << "Could not create Vulkan device!" << std::endl;
    return false;
  }

  vk_graphics_queue_family_index_ = selected_graphics_queue_family_index;
  vk_present_queue_family_index_ = selected_present_queue_family_index;
  //
  // end of CreateDevice()
  //

  printf("VulkanDeviceQueue::%s_end\n", __func__);

  // GetDeviceQueue()
  vkGetDeviceQueue(vk_device_, vk_graphics_queue_family_index_, 0,
                   &GraphicsQueue_);
  vkGetDeviceQueue(vk_device_, vk_present_queue_family_index_, 0,
                   &PresentQueue_);

  return true;
}

bool VulkanDeviceQueue::CheckExtensionAvailability(
    const char* extension_name,
    const std::vector<VkExtensionProperties>& available_extensions) {
  for (size_t i = 0; i < available_extensions.size(); ++i) {
    if (strcmp(available_extensions[i].extensionName, extension_name) == 0) {
      return true;
    }
  }
  return false;
}

bool VulkanDeviceQueue::CheckPhysicalDeviceProperties(
    VkPhysicalDevice vk_physical_device,
    uint32_t& selected_graphics_queue_family_index,
    uint32_t& selected_present_queue_family_index) {
  uint32_t extensions_count = 0;
  if ((vkEnumerateDeviceExtensionProperties(vk_physical_device, nullptr,
                                            &extensions_count,
                                            nullptr) != VK_SUCCESS) ||
      (extensions_count == 0)) {
    std::cout << "Error occurred during physical device " << vk_physical_device
              << " extensions enumeration!" << std::endl;
    return false;
  }

  std::vector<VkExtensionProperties> available_extensions(extensions_count);
  if (vkEnumerateDeviceExtensionProperties(
          vk_physical_device, nullptr, &extensions_count,
          &available_extensions[0]) != VK_SUCCESS) {
    std::cout << "Error occurred during physical device " << vk_physical_device
              << " extensions enumeration!" << std::endl;
    return false;
  }

  std::vector<const char*> device_extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  for (size_t i = 0; i < device_extensions.size(); ++i) {
    if (!CheckExtensionAvailability(device_extensions[i],
                                    available_extensions)) {
      std::cout << "Physical device " << vk_physical_device
                << " doesn't support extension named \"" << device_extensions[i]
                << "\"!" << std::endl;
      return false;
    }
  }

  VkPhysicalDeviceProperties device_properties;
  VkPhysicalDeviceFeatures device_features;

  vkGetPhysicalDeviceProperties(vk_physical_device, &device_properties);
  vkGetPhysicalDeviceFeatures(vk_physical_device, &device_features);

  uint32_t major_version = VK_VERSION_MAJOR(device_properties.apiVersion);

  if ((major_version < 1) ||
      (device_properties.limits.maxImageDimension2D < 4096)) {
    std::cout << "Physical device " << vk_physical_device
              << " doesn't support required parameters!" << std::endl;
    return false;
  }

  uint32_t queue_families_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device,
                                           &queue_families_count, nullptr);
  if (queue_families_count == 0) {
    std::cout << "Physical device " << vk_physical_device
              << " doesn't have any queue families!" << std::endl;
    return false;
  }

  std::vector<VkQueueFamilyProperties> queue_family_properties(
      queue_families_count);
  std::vector<VkBool32> queue_present_support(queue_families_count);

  vkGetPhysicalDeviceQueueFamilyProperties(
      vk_physical_device, &queue_families_count, &queue_family_properties[0]);

  uint32_t graphics_queue_family_index = UINT32_MAX;
  uint32_t present_queue_family_index = UINT32_MAX;

#if defined(VK_USE_PLATFORM_XLIB_KHR)
  Display* xdisplay = gfx::GetXDisplay();
  VisualID visual_id =
      XVisualIDFromVisual(DefaultVisual(xdisplay, DefaultScreen(xdisplay)));
#endif  //

  // Check whether a given queue family from a given physical device supports a
  // swap chain or, to be more precise, whether it supports presenting images to
  // a given surface.
  for (uint32_t i = 0; i < queue_families_count; ++i) {
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    queue_present_support[i] = vkGetPhysicalDeviceXlibPresentationSupportKHR(
        vk_physical_device, i, xdisplay, visual_id);
#else
#error Non-Supported Vulkan implementation.
#endif
    printf("queue family %d\n", i);
    if ((queue_family_properties[i].queueCount > 0) &&
        (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      // Select first queue that supports graphics
      if (graphics_queue_family_index == UINT32_MAX) {
        graphics_queue_family_index = i;
      }

      // If there is queue that supports both graphics and present - prefer it
      if (queue_present_support[i]) {
        printf(
            "The GPU on system supports graphics and present queue togehter\n");
        selected_graphics_queue_family_index = i;
        selected_present_queue_family_index = i;
        return true;
      }
    }
  }

  // We don't have queue that supports both graphics and present so we have to
  // use separate queues
  for (uint32_t i = 0; i < queue_families_count; ++i) {
    if (queue_present_support[i]) {
      present_queue_family_index = i;
      break;
    }
  }

  // If this device doesn't support queues with graphics and present
  // capabilities don't use it
  if ((graphics_queue_family_index == UINT32_MAX) ||
      (present_queue_family_index == UINT32_MAX)) {
    std::cout << "Could not find queue family with required properties on "
                 "physical device "
              << vk_physical_device_ << "!" << std::endl;
    return false;
  }

  selected_graphics_queue_family_index = graphics_queue_family_index;
  selected_present_queue_family_index = present_queue_family_index;
  return true;
}

void VulkanDeviceQueue::Destroy() {
  printf("VulkanDeviceQueue::%s\n", __func__);
  if (VK_NULL_HANDLE != vk_device_) {
    vkDestroyDevice(vk_device_, nullptr);
    // vk_device_ = VK_NULL_HANDLE;
  }

  vk_graphics_queue_family_index_ = UINT32_MAX;
  vk_present_queue_family_index_ = UINT32_MAX;

  vk_physical_device_ = VK_NULL_HANDLE;
}

std::unique_ptr<VulkanCommandPool> VulkanDeviceQueue::CreateCommandPool(
    VulkanSwapChain* swap_chain) {
  std::unique_ptr<VulkanCommandPool> command_pool(
      new VulkanCommandPool(this, swap_chain));

  // For tutorial4: VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
  // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
  if (!command_pool->Initialize(
          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
          VK_COMMAND_POOL_CREATE_TRANSIENT_BIT))
    return nullptr;

  return command_pool;
}

}  // namespace gpu
