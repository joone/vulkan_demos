
#include <vulkan/vulkan.h>
#include "gpu/vulkan/vulkan_export.h"

namespace intel {

class VULKAN_EXPORT VulkanInstance {
 public:
  VulkanInstance();
  ~VulkanInstance() { }

  static bool Create();
  void Initialize(); 
  VkInstance* handle();

 private:
  VkInstance vk_instance;
  static VulkanInstance* vulkan_instance_;
};

}  // namespace gpu
