// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_RENDER_PASS_H_
#define GPU_VULKAN_VULKAN_RENDER_PASS_H_

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

#include "base/macros.h"
#include "gpu/vulkan/vulkan_export.h"

namespace gpu {

class CommandBufferRecorderBase;
class VulkanDeviceQueue;
// class VulkanImageView;
class VulkanSwapChain;

class VULKAN_EXPORT VulkanRenderPass {
 public:
  enum class AttachmentType {
    // Use image view of the swap chain image.
    ATTACHMENT_TYPE_SWAP_IMAGE,

    // Use image view of the attachment data.
    ATTACHMENT_TYPE_ATTACHMENT_VIEW,
  };

  enum class ImageLayoutType {
    // Undefined image layout.
    IMAGE_LAYOUT_UNDEFINED,

    // Image layout whiches matches the image view.
    IMAGE_LAYOUT_TYPE_IMAGE_VIEW,

    // Image layout for presenting.
    IMAGE_LAYOUT_TYPE_PRESENT,
  };

  explicit VulkanRenderPass(VulkanDeviceQueue* device_queue);
  ~VulkanRenderPass();

  bool Initialize(const VulkanSwapChain* swap_chain,
      std::vector<VkSubpassDependency>& subpass_dependencies);
  void Destroy();

  void SetClearValue(uint32_t attachment_index, VkClearValue clear_value);
  bool CreatePipeline(const std::string& vertexShader,
                      const std::string& fragmentShader,
                      VkPrimitiveTopology primitiveTopology,
                      bool qvertex_binding = false);
  bool CreateFrameBuffer(const VulkanSwapChain* swap_chain,
                         uint32_t resource_index);

  VkRenderPass handle() { return render_pass_; }
  // There is 1 frame buffer for every swap chain image.
  std::vector<VkFramebuffer> frame_buffers_;
  VkPipeline GetGraphicsPipeline() { return graphics_pipeline_; }

  //  bool CreateRenderingResource(uint32_t num_resoures);
  // for Resource, Tutorial4
  static const size_t ResourcesCount_ = 3;
  /*  struct RenderingResourcesData {
      VkFramebuffer                         Framebuffer;
     // VkCommandBuffer                       CommandBuffer;
      VkSemaphore                           ImageAvailableSemaphore;
      VkSemaphore                           FinishedRenderingSemaphore;
      VkFence                               Fence;
    };
    std::vector<RenderingResourcesData>   RenderingResources_;
  */
 private:
  VulkanDeviceQueue* device_queue_ = nullptr;
  const VulkanSwapChain* swap_chain_ = nullptr;
  //  uint32_t num_sub_passes_ = 0;
  //  uint32_t current_sub_pass_ = 0;
  bool executing_ = false;
  //  VkSubpassContents execution_type_ = VK_SUBPASS_CONTENTS_INLINE;
  VkRenderPass render_pass_ = VK_NULL_HANDLE;

  // There is 1 clear color for every attachment which needs a clear.
  std::vector<VkClearValue> attachment_clear_values_;

  // There is 1 clear index for every attachment which needs a clear. This is
  // kept in a separate array since it is only used setting clear values.
  std::vector<uint32_t> attachment_clear_indexes_;

  VkPipeline graphics_pipeline_;
  VkPipelineLayout pipeline_layout_;

  DISALLOW_COPY_AND_ASSIGN(VulkanRenderPass);
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_RENDER_PASS_H_
