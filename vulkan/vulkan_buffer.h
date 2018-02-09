
#ifndef GPU_VULKAN_VULKAN_BUFFER_H_
#define GPU_VULKAN_VULKAN_BUFFER_H_

#include <vulkan/vulkan.h>

namespace gpu {

class VulkanDeviceQueue;

class VulkanBuffer {
 public:
  struct VertexData {
    float x, y, z, w;
    float r, g, b, a;
  };

  VulkanBuffer();
  ~VulkanBuffer();
  bool Initialize(VulkanDeviceQueue*, VertexData*);
  VkBuffer* handle() { return &handle_; }

 private:
  bool AllocateBufferMemory(VkPhysicalDevice physical_device);

  VkDevice device_;
  VkBuffer handle_;
  VkDeviceMemory memory_;
  uint32_t size_;
};

}  // namespace gpu
#endif