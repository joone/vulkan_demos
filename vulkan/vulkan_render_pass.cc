// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vulkan_render_pass.h"

#include <iostream>

#include "base/logging.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "ui/gfx/geometry/size.h"
#include "vulkan_buffer.h"
#include "vulkan_device_queue.h"
#include "vulkan_image_view.h"
#include "vulkan_shader_module.h"
#include "vulkan_swap_chain.h"

namespace gpu {

VulkanRenderPass::VulkanRenderPass(VulkanDeviceQueue* device_queue)
    : device_queue_(device_queue) {}

VulkanRenderPass::~VulkanRenderPass() {
  DCHECK_EQ(static_cast<VkRenderPass>(VK_NULL_HANDLE), render_pass_);
  DCHECK(frame_buffers_.empty());
}

// Render pass describes the internal organization of rendering resources
// (images/attachments), how they are used, and how they change during the
// rendering process.
bool VulkanRenderPass::Initialize(const VulkanSwapChain* swap_chain) {
  printf("VulkanRenderPass::%s\n", __func__);
  DCHECK(!executing_);
  DCHECK_EQ(static_cast<VkRenderPass>(VK_NULL_HANDLE), render_pass_);
  DCHECK(frame_buffers_.empty());
  // DCHECK(render_pass_data.ValidateData(swap_chain));

  VkDevice device = device_queue_->GetVulkanDevice();
  //  VkResult result = VK_SUCCESS;

  swap_chain_ = swap_chain;

  // for Tutorial4
  frame_buffers_.resize(ResourcesCount_);
  // Create VkRenderPass;
  VkAttachmentDescription attachment_descriptions[] = {{
      0,                             // VkAttachmentDescriptionFlags   flags
      swap_chain_->format(),         // VkFormat                       format
      VK_SAMPLE_COUNT_1_BIT,         // VkSampleCountFlagBits          samples
      VK_ATTACHMENT_LOAD_OP_CLEAR,   // VkAttachmentLoadOp             loadOp
      VK_ATTACHMENT_STORE_OP_STORE,  // VkAttachmentStoreOp            storeOp
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,   // VkAttachmentLoadOp stencilLoadOp
      VK_ATTACHMENT_STORE_OP_DONT_CARE,  // VkAttachmentStoreOp stencilStoreOp
      VK_IMAGE_LAYOUT_UNDEFINED,         // VkImageLayout initialLayout;
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR    // VkImageLayout finalLayout
  }};

  VkAttachmentReference color_attachment_references[] = {{
      0,  // uint32_t                       attachment
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // VkImageLayout layout
  }};

  VkSubpassDescription subpass_descriptions[] = {{
      0,                                // VkSubpassDescriptionFlags      flags
      VK_PIPELINE_BIND_POINT_GRAPHICS,  // VkPipelineBindPoint pipelineBindPoint
      0,        // uint32_t                       inputAttachmentCount
      nullptr,  // const VkAttachmentReference   *pInputAttachments
      1,        // uint32_t                       colorAttachmentCount
      color_attachment_references,  // VkAttachmentReference(color attachments)
      nullptr,  // const VkAttachmentReference   *pResolveAttachments
      nullptr,  // const VkAttachmentReference   *pDepthStencilAttachment
      0,        // uint32_t                       preserveAttachmentCount
      nullptr   // const uint32_t*                pPreserveAttachments
  }};

  // Tutorial4 defines dependencies.
  // When we create a render pass and provide a description for it,
  // the same information is specified through subpass dependencies
  std::vector<VkSubpassDependency> dependencies = {
      {
          VK_SUBPASS_EXTERNAL,  // uint32_t                       srcSubpass
          0,                    // uint32_t                       dstSubpass
          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,           // VkPipelineStageFlags
                                                          // srcStageMask
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // VkPipelineStageFlags
                                                          // dstStageMask
          VK_ACCESS_MEMORY_READ_BIT,             // VkAccessFlags srcAccessMask
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  // VkAccessFlags dstAccessMask
          VK_DEPENDENCY_BY_REGION_BIT  // VkDependencyFlags dependencyFlags
      },
      {
          0,                    // uint32_t                       srcSubpass
          VK_SUBPASS_EXTERNAL,  // uint32_t                       dstSubpass
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // VkPipelineStageFlags
                                                          // srcStageMask
          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,           // VkPipelineStageFlags
                                                          // dstStageMask
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  // VkAccessFlags srcAccessMask
          VK_ACCESS_MEMORY_READ_BIT,             // VkAccessFlags dstAccessMask
          VK_DEPENDENCY_BY_REGION_BIT  // VkDependencyFlags dependencyFlags
      }};

  // Needs for Totorial4
  VkRenderPassCreateInfo render_pass_create_info = {
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,  // VkStructureType sType
      nullptr,  // const void                    *pNext
      0,        // VkRenderPassCreateFlags        flags
      1,        // uint32_t                       attachmentCount
      attachment_descriptions,  // const VkAttachmentDescription *pAttachments
      1,                        // uint32_t                       subpassCount
      subpass_descriptions,     // const VkSubpassDescription    *pSubpasses
      static_cast<uint32_t>(dependencies.size()),  // uint32_t dependencyCount
      &dependencies[0]  // const VkSubpassDependency     *pDependencies
  };

  if (vkCreateRenderPass(device, &render_pass_create_info, nullptr,
                         &render_pass_) != VK_SUCCESS) {
    std::cout << "Could not create render pass!" << std::endl;
    return false;
  }

  return true;
}

bool VulkanRenderPass::CreateFrameBuffer(const VulkanSwapChain* swap_chain,
                                         uint32_t resource_index) {
  VkDevice device = device_queue_->GetVulkanDevice();

  // for Tutorial4
  if (frame_buffers_.size()) {
    if (frame_buffers_[resource_index] != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, frame_buffers_[resource_index], nullptr);
      frame_buffers_[resource_index] = VK_NULL_HANDLE;
    }
  }

  VkImageView image_view = swap_chain->GetImageView(resource_index)->handle();
  VkFramebufferCreateInfo framebuffer_create_info = {
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,  // VkStructureType sType
      nullptr,       // const void                    *pNext
      0,             // VkFramebufferCreateFlags       flags
      render_pass_,  // VkRenderPass                   renderPass
      1,
      &image_view,  // const VkImageView             *pAttachments
      300,          // uint32_t                       width
      300,          // uint32_t                       height
      1             // uint32_t                       layers
  };

  if (vkCreateFramebuffer(device, &framebuffer_create_info, nullptr,
                          &frame_buffers_[resource_index]) != VK_SUCCESS) {
    std::cout << "Could not create a framebuffer!" << std::endl;
    return false;
  }

  return true;
}

// resize = true for tutorial4(demo3)
bool VulkanRenderPass::CreatePipeline(const std::string& kVertexShaderSource,
                                      const std::string& kFragShaderSource,
                                      bool resize) {
  printf("VulkanRenderPass::%s  resize=%d\n", __func__, resize);
  VkDevice device = device_queue_->GetVulkanDevice();

  VulkanShaderModule vertex_shader_module(device);
  vertex_shader_module.InitializeGLSL(VulkanShaderModule::ShaderType::VERTEX,
                                      "vetext", "main", kVertexShaderSource);

  VulkanShaderModule fragment_shader_module(device);
  fragment_shader_module.InitializeGLSL(
      VulkanShaderModule::ShaderType::FRAGMENT, "fragment", "main",
      kFragShaderSource);

  if (!vertex_shader_module.IsValid()) {
    std::cout << "vertext shader error = "
              << vertex_shader_module.GetErrorMessages();
    return false;
  }

  if (!fragment_shader_module.IsValid()) {
    std::cout << "fragment shader error = "
              << fragment_shader_module.GetErrorMessages();
    return false;
  }

  std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos = {
      // Vertex shader
      {
          VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,  // VkStructureType
                                                                // sType
          nullptr,  // const void                                    *pNext
          0,        // VkPipelineShaderStageCreateFlags               flags
          VK_SHADER_STAGE_VERTEX_BIT,     // VkShaderStageFlagBits stage
          vertex_shader_module.handle(),  // VkShaderModule module
          "main",  // const char                                    *pName
          nullptr  // const VkSpecializationInfo *pSpecializationInfo
      },
      // Fragment shader
      {
          VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,  // VkStructureType
                                                                // sType
          nullptr,  // const void                                    *pNext
          0,        // VkPipelineShaderStageCreateFlags               flags
          VK_SHADER_STAGE_FRAGMENT_BIT,     // VkShaderStageFlagBits stage
          fragment_shader_module.handle(),  // VkShaderModule module
          "main",  // const char                                    *pName
          nullptr  // const VkSpecializationInfo *pSpecializationInfo
      }};

  // only for tutorial4
  std::vector<VkVertexInputBindingDescription> vertex_binding_descriptions = {{
      0,                                 // uint32_t          binding
      sizeof(VulkanBuffer::VertexData),  // uint32_t          stride
      VK_VERTEX_INPUT_RATE_VERTEX        // VkVertexInputRate inputRate
  }};

  std::vector<VkVertexInputAttributeDescription> vertex_attribute_descriptions =
      {{
           0,  // uint32_t location
           vertex_binding_descriptions[0].binding,       // uint32_t binding
           VK_FORMAT_R32G32B32A32_SFLOAT,                // VkFormat format
           offsetof(struct VulkanBuffer::VertexData, x)  // uint32_t offset
       },
       {
           1,  // uint32_t location
           vertex_binding_descriptions[0].binding,  // uint32_t binding
           VK_FORMAT_R32G32B32A32_SFLOAT,  // VkFormat format
           offsetof(struct VulkanBuffer::VertexData, r)  // uint32_t offset
       }};
  // end of only for tutorial4

  VkVertexInputBindingDescription* pVertexBindingDescriptions = nullptr;
  VkVertexInputAttributeDescription* pVertexAttributeDescriptions = nullptr;
  uint32_t binding_size = 0;
  uint32_t attribute_size = 0;

  if (resize) {
    pVertexBindingDescriptions = &vertex_binding_descriptions[0];
    pVertexAttributeDescriptions = &vertex_attribute_descriptions[0];
    binding_size = static_cast<uint32_t>(vertex_binding_descriptions.size());
    attribute_size =
        static_cast<uint32_t>(vertex_attribute_descriptions.size());
  }

  VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,  // StructureType
                                                                  // sType
      nullptr,       // const void   *pNext
      0,             // VkPipelineVertexInputStateCreateFlags          flags;
      binding_size,  // uint32_t vertexBindingDescriptionCount
      pVertexBindingDescriptions,  // const VkVertexInputBindingDescription
                                   // *pVertexBindingDescriptions
      attribute_size,              // uint32_t vertexAttributeDescriptionCount
      pVertexAttributeDescriptions};

  VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,  // VkStructureType
                                                                    // sType
      nullptr,  // const void                                    *pNext
      0,        // VkPipelineInputAssemblyStateCreateFlags        flags
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,  // VkPrimitiveTopology topology
      VK_FALSE                              // VkBool32 primitiveRestartEnable
  };

  // for tutorial3
  VkViewport viewport = {
      0.0f,    // x
      0.0f,    // y
      300.0f,  // width
      300.0f,  // height
      0.0f,    // minDepth
      1.0f     // maxDepth
  };

  // for tutorial3
  VkRect2D scissor = {{
                          // VkOffset2D offset
                          0,  // x
                          0   // y
                      },
                      {
                          // VkExtent2D extent
                          300,  // width
                          300   // height
                      }};

  VkViewport* pViewport = nullptr;
  VkRect2D* pScissor = nullptr;

  if (!resize) {
    pViewport = &viewport;
    pScissor = &scissor;
  }

  VkPipelineViewportStateCreateInfo viewport_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,  // VkStructureType
                                                              // sType
      nullptr,  // const void                                    *pNext
      0,        // VkPipelineViewportStateCreateFlags             flags
      1,        // uint32_t                                       viewportCount
      pViewport,  // const VkViewport                              *pViewports
      1,          // uint32_t                                       scissorCount
      pScissor    // const VkRect2D                                *pScissors
  };

  // Preparing the Rasterization State’s Description
  VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,  // VkStructureType
                                                                   // sType
      nullptr,   // const void                                    *pNext
      0,         // VkPipelineRasterizationStateCreateFlags        flags
      VK_FALSE,  // VkBool32 depthClampEnable
      VK_FALSE,  // VkBool32 rasterizerDiscardEnable
      VK_POLYGON_MODE_FILL,             // VkPolygonMode polygonMode
      VK_CULL_MODE_BACK_BIT,            // VkCullModeFlags cullMode
      VK_FRONT_FACE_COUNTER_CLOCKWISE,  // VkFrontFace frontFace
      VK_FALSE,                         // VkBool32 depthBiasEnable
      0.0f,                             // float depthBiasConstantFactor
      0.0f,  // float                                          depthBiasClamp
      0.0f,  // float depthBiasSlopeFactor
      1.0f   // float                                          lineWidth
  };

  // Preparing the Rasterization State’s Description
  VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,  // VkStructureType
                                                                 // sType
      nullptr,  // const void                                    *pNext
      0,        // VkPipelineMultisampleStateCreateFlags          flags
      VK_SAMPLE_COUNT_1_BIT,  // VkSampleCountFlagBits rasterizationSamples
      VK_FALSE,               // VkBool32 sampleShadingEnable
      1.0f,  // float                                          minSampleShading
      nullptr,   // const VkSampleMask                            *pSampleMask
      VK_FALSE,  // VkBool32 alphaToCoverageEnable
      VK_FALSE   // VkBool32 alphaToOneEnable
  };

  // Setting the Blending State’s Description
  VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
      VK_FALSE,  // VkBool32                                       blendEnable
      VK_BLEND_FACTOR_ONE,   // VkBlendFactor srcColorBlendFactor
      VK_BLEND_FACTOR_ZERO,  // VkBlendFactor dstColorBlendFactor
      VK_BLEND_OP_ADD,       // VkBlendOp colorBlendOp
      VK_BLEND_FACTOR_ONE,   // VkBlendFactor srcAlphaBlendFactor
      VK_BLEND_FACTOR_ZERO,  // VkBlendFactor dstAlphaBlendFactor
      VK_BLEND_OP_ADD,       // VkBlendOp alphaBlendOp
      VK_COLOR_COMPONENT_R_BIT |
          VK_COLOR_COMPONENT_G_BIT |  // VkColorComponentFlags colorWriteMask
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

  VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,  // VkStructureType
                                                                 // sType
      nullptr,   // const void                                    *pNext
      0,         // VkPipelineColorBlendStateCreateFlags           flags
      VK_FALSE,  // VkBool32                                       logicOpEnable
      VK_LOGIC_OP_COPY,  // VkLogicOp logicOp
      1,  // uint32_t                                       attachmentCount
      &color_blend_attachment_state,  // const
                                      // VkPipelineColorBlendAttachmentState
                                      // *pAttachments
      {0.0f, 0.0f, 0.0f, 0.0f}        // float blendConstants[4]
  };

  // only for tutorial4
  std::vector<VkDynamicState> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,  // VkStructureType
                                                             // sType
      nullptr,  // const void                                    *pNext
      0,        // VkPipelineDynamicStateCreateFlags              flags
      static_cast<uint32_t>(
          dynamic_states.size()),  // uint32_t dynamicStateCount
      &dynamic_states[0]           // const VkDynamicState *pDynamicStates
  };
  // end of tutorial4

  const VkPipelineDynamicStateCreateInfo* pDynamicStateCreateInfo = nullptr;
  if (resize)
    pDynamicStateCreateInfo = &dynamic_state_create_info;

  // Tutorial::CreatePipelineLayout(): Creating a Pipeline Layout
  VkPipelineLayoutCreateInfo layout_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,  // VkStructureType sType
      nullptr,  // const void                    *pNext
      0,        // VkPipelineLayoutCreateFlags    flags
      0,        // uint32_t                       setLayoutCount
      nullptr,  // const VkDescriptorSetLayout   *pSetLayouts
      0,        // uint32_t                       pushConstantRangeCount
      nullptr   // const VkPushConstantRange     *pPushConstantRanges
  };

  if (vkCreatePipelineLayout(device, &layout_create_info, nullptr,
                             &pipeline_layout_) != VK_SUCCESS) {
    std::cout << "Could not create pipeline layout!" << std::endl;
    return false;
  }
  // end of creating a Pipeline Layout (only for tutorial3?)

  VkGraphicsPipelineCreateInfo pipeline_create_info = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,  // VkStructureType sType
      nullptr,  // const void                                    *pNext
      0,        // VkPipelineCreateFlags                          flags
      static_cast<uint32_t>(
          shader_stage_create_infos.size()),  // uint32_t stageCount
      &shader_stage_create_infos[0],   // const VkPipelineShaderStageCreateInfo
      &vertex_input_state_create_info, // VkPipelineVertexInputStateCreateInfo
      &input_assembly_state_create_info, // VkPipelineInputAssemblyStateCreateInfo
                                          // *pInputAssemblyState
      nullptr,                     // const VkPipelineTessellationStateCreateInfo
      &viewport_state_create_info, // const VkPipelineViewportStateCreateInfo
      &rasterization_state_create_info, // VkPipelineRasterizationStateCreateInfo
      &multisample_state_create_info,  // VkPipelineMultisampleStateCreateInfo
      nullptr,                         // const VkPipelineDepthStencilStateCreateInfo
      &color_blend_state_create_info,  // VkPipelineColorBlendStateCreateInfo
                                       // *pColorBlendState
      pDynamicStateCreateInfo,         //   (tutorial4)       // const
                                       //   VkPipelineDynamicStateCreateInfo
                                       //   *pDynamicState
      pipeline_layout_,                // VkPipelineLayout layout
      render_pass_,                    // VkRenderPass renderPass
      0,               // uint32_t subpass
      VK_NULL_HANDLE,  // VkPipeline basePipelineHandle
      -1  // int32_t basePipelineIndex
  };

  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                &pipeline_create_info, nullptr,
                                &graphics_pipeline_) != VK_SUCCESS) {
    std::cout << "Could not create graphics pipeline!" << std::endl;
    return false;
  }

  vertex_shader_module.Destroy();
  fragment_shader_module.Destroy();
  printf("VulkanRenderPass::%s_end\n", __func__);
  return true;
}

void VulkanRenderPass::Destroy() {
  VkDevice device = device_queue_->GetVulkanDevice();

  for (VkFramebuffer frame_buffer : frame_buffers_) {
    vkDestroyFramebuffer(device, frame_buffer, nullptr);
  }
  frame_buffers_.clear();

  if (VK_NULL_HANDLE != render_pass_) {
    vkDestroyRenderPass(device, render_pass_, nullptr);
    render_pass_ = VK_NULL_HANDLE;
  }

  vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
  swap_chain_ = nullptr;
  // attachment_clear_values_.clear();
  // attachment_clear_indexes_.clear();
}

void VulkanRenderPass::SetClearValue(uint32_t attachment_index,
                                     VkClearValue clear_value) {}

}  // namespace gpu
