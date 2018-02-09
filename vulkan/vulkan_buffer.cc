#include "vulkan_buffer.h"

#include <iostream>

#include "vulkan_device_queue.h"

namespace gpu {

VulkanBuffer::VulkanBuffer() :
    device_(VK_NULL_HANDLE),
    handle_(VK_NULL_HANDLE),
    memory_(VK_NULL_HANDLE)
{
}

VulkanBuffer::~VulkanBuffer() {
}

bool VulkanBuffer::Initialize(VulkanDeviceQueue* device_queue, VertexData* vertex_data) {
  printf("VulkanBuffer::%s\n", __func__);
  device_ = device_queue->GetVulkanDevice();
  size_ = sizeof(*vertex_data) * 4;
  printf(" Vertext size=%d\n", size_);

  VkBufferCreateInfo buffer_create_info = {
    VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,             // VkStructureType        sType
    nullptr,                                          // const void            *pNext
    0,                                                // VkBufferCreateFlags    flags
    size_,                                            // VkDeviceSize           size
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,                // VkBufferUsageFlags     usage
    VK_SHARING_MODE_EXCLUSIVE,                        // VkSharingMode          sharingMode
    0,                                                // uint32_t               queueFamilyIndexCount
    nullptr                                           // const uint32_t        *pQueueFamilyIndices
  };

  if (vkCreateBuffer(device_, &buffer_create_info, nullptr, &handle_ ) != VK_SUCCESS ) {
    std::cout << "Could not create a vertex buffer!" << std::endl;
    return false;
  }

  if (!AllocateBufferMemory(device_queue->GetVulkanPhysicalDevice()) ) {
    std::cout << "Could not allocate memory for a vertex buffer!" << std::endl;
    return false;
  }

  if( vkBindBufferMemory(device_, handle_, memory_, 0 ) != VK_SUCCESS ) {
    std::cout << "Could not bind memory for a vertex buffer!" << std::endl;
    return false;
  }

  void *vertex_buffer_memory_pointer;
  if( vkMapMemory(device_, memory_, 0, size_, 0, &vertex_buffer_memory_pointer ) != VK_SUCCESS ) {
    std::cout << "Could not map memory and upload data to a vertex buffer!" << std::endl;
    return false;
  }

  memcpy(vertex_buffer_memory_pointer, vertex_data, size_ );

  VkMappedMemoryRange flush_range = {
    VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,            // VkStructureType        sType
    nullptr,                                          // const void            *pNext
    memory_,                                          // VkDeviceMemory         memory
    0,                                                // VkDeviceSize           offset
    VK_WHOLE_SIZE                                     // VkDeviceSize           size
  };
  
  vkFlushMappedMemoryRanges(device_, 1, &flush_range );
  vkUnmapMemory(device_, memory_);
  
  return true;
}

bool VulkanBuffer::AllocateBufferMemory(VkPhysicalDevice physical_device) {
  printf("VulkanBuffer::%s\n", __func__);
  VkMemoryRequirements buffer_memory_requirements;
  vkGetBufferMemoryRequirements(device_, handle_, &buffer_memory_requirements );

  VkPhysicalDeviceMemoryProperties memory_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties );

  for( uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i ) {
    if( (buffer_memory_requirements.memoryTypeBits & (1 << i)) &&
      (memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ) {

      VkMemoryAllocateInfo memory_allocate_info = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,     // VkStructureType                        sType
        nullptr,                                    // const void                            *pNext
        buffer_memory_requirements.size,            // VkDeviceSize                           allocationSize
        i                                           // uint32_t                               memoryTypeIndex
      };

      if( vkAllocateMemory(device_, &memory_allocate_info, nullptr, &memory_ ) == VK_SUCCESS ) {
        return true;
      }
    }
  }
  return false;
}

} // namespace gpu
