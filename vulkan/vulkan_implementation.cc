// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_implementation.h"

#include <iostream>
#include <unordered_set>
#include <vector>
#include "base/logging.h"
#include "base/macros.h"
#include "gpu/vulkan/vulkan_platform.h"

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include "ui/gfx/x/x11_types.h"
#endif  // defined(VK_USE_PLATFORM_XLIB_KHR)

namespace gpu {

static bool CheckExtensionAvailability(
    const char* extension_name,
    const std::vector<VkExtensionProperties>& available_extensions) {
  for (size_t i = 0; i < available_extensions.size(); ++i) {
    if (strcmp(available_extensions[i].extensionName, extension_name) == 0) {
      return true;
    }
  }
  return false;
}

struct VulkanInstance {
  VulkanInstance() {}

  void Initialize() { valid = InitializeVulkanInstance(); }

  bool InitializeVulkanInstance() {
    printf("%s\n", __func__);
    uint32_t extensions_count = 0;
    if ((vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count,
                                                nullptr) != VK_SUCCESS) ||
        (extensions_count == 0)) {
      std::cout << "Error occurred during instance extensions enumeration!"
                << std::endl;
      return false;
    }

    std::vector<VkExtensionProperties> available_extensions(extensions_count);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count,
                                               &available_extensions[0]) !=
        VK_SUCCESS) {
      std::cout << "Error occurred during instance extensions enumeration!"
                << std::endl;
      return false;
    }

    std::vector<const char*> extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_XCB_KHR)
      VK_KHR_XCB_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
      VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#endif
    };

    for (size_t i = 0; i < extensions.size(); ++i) {
      if (!CheckExtensionAvailability(extensions[i], available_extensions)) {
        std::cout << "Could not find instance extension named \""
                  << extensions[i] << "\"!" << std::endl;
        return false;
      }
    }

    VkApplicationInfo application_info = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO,  // VkStructureType            sType
        nullptr,                             // const void                *pNext
        "API without Secrets: Introduction to Vulkan",  // const char
                                                        // *pApplicationName
        VK_MAKE_VERSION(1, 0, 0),    // uint32_t applicationVersion
        "Vulkan Tutorial by Intel",  // const char                *pEngineName
        VK_MAKE_VERSION(1, 0, 0),    // uint32_t                   engineVersion
        VK_MAKE_VERSION(1, 0, 0)     // uint32_t                   apiVersion
    };

    VkInstanceCreateInfo instance_create_info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,  // VkStructureType sType
        nullptr,            // const void                *pNext
        0,                  // VkInstanceCreateFlags      flags
        &application_info,  // const VkApplicationInfo   *pApplicationInfo
        0,                  // uint32_t                   enabledLayerCount
        nullptr,            // const char * const        *ppEnabledLayerNames
        static_cast<uint32_t>(extensions.size()),  // enabledExtensionCount
        &extensions[0]  // const char * const        *ppEnabledExtensionNames
    };

    if (vkCreateInstance(&instance_create_info, nullptr, &vk_instance) !=
        VK_SUCCESS) {
      std::cout << "Could not create Vulkan instance!" << std::endl;
      return false;
    }
    printf("%s_end\n", __func__);
    return true;
  }

  bool valid = false;
  VkInstance vk_instance = VK_NULL_HANDLE;
};

static VulkanInstance* vulkan_instance = nullptr;

bool InitializeVulkan() {
  printf("%s\n", __func__);
  DCHECK(!vulkan_instance);
  vulkan_instance = new VulkanInstance;
  vulkan_instance->Initialize();
  return vulkan_instance->valid;
}

bool VulkanSupported() {
  DCHECK(vulkan_instance);
  return vulkan_instance->valid;
}

VkInstance GetVulkanInstance() {
  DCHECK(vulkan_instance);
  DCHECK(vulkan_instance->valid);
  return vulkan_instance->vk_instance;
}

}  // namespace gpu
