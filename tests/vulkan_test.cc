// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "basic_vulkan_test.h"

#include <chrono>
#include <memory>
#include <thread>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_types.h"

#include "../vulkan/vulkan_buffer.h"
#include "../vulkan/vulkan_command_buffer.h"
#include "../vulkan/vulkan_command_pool.h"
#include "../vulkan/vulkan_device_queue.h"
#include "../vulkan/vulkan_render_pass.h"
#include "../vulkan/vulkan_surface.h"
#include "../vulkan/vulkan_swap_chain.h"

// This file tests basic vulkan initialization steps.
namespace gpu {

TEST_F(BasicVulkanTest, BasicVulkanSurface) {
  GetDeviceQueue()->Initialize(
      VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG |
      VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG);
  std::unique_ptr<VulkanSurface> surface =
      VulkanSurface::CreateViewSurface(window());
  EXPECT_TRUE(surface);
  EXPECT_TRUE(surface->CreateSurface());

  SetSurface(surface.get());

  EXPECT_TRUE(surface->Initialize(GetDeviceQueue(),
                                  VulkanSurface::DEFAULT_SURFACE_FORMAT));

  VkDevice device = GetDeviceQueue()->GetVulkanDevice();
  uint32_t image_count = surface->GetSwapChain()->num_images();
  std::vector<VkImage> swap_chain_images(image_count);
  if (vkGetSwapchainImagesKHR(device, surface->GetSwapChain()->handle(),
                              &image_count,
                              &swap_chain_images[0]) != VK_SUCCESS) {
    std::cout << "Could not get swap chain images!" << std::endl;
    return;
  }

  VkCommandBufferBeginInfo cmd_buffer_begin_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  // VkStructureType sType
      nullptr,  // const void                            *pNext
      VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,  // VkCommandBufferUsageFlags
                                                     // flags
      nullptr  // const VkCommandBufferInheritanceInfo  *pInheritanceInfo
  };

  VkClearColorValue clear_color = {{1.0f, 0.8f, 0.4f, 0.0f}};

  VkImageSubresourceRange image_subresource_range = {
      VK_IMAGE_ASPECT_COLOR_BIT,  // VkImageAspectFlags aspectMask
      0,  // uint32_t                               baseMipLevel
      1,  // uint32_t                               levelCount
      0,  // uint32_t                               baseArrayLayer
      1   // uint32_t                               layerCount
  };

  for (uint32_t i = 0; i < image_count; ++i) {
    VkImageMemoryBarrier barrier_from_present_to_clear = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // VkStructureType sType
        nullptr,  // const void                            *pNext
        VK_ACCESS_MEMORY_READ_BIT,             // VkAccessFlags srcAccessMask
        VK_ACCESS_TRANSFER_WRITE_BIT,          // VkAccessFlags dstAccessMask
        VK_IMAGE_LAYOUT_UNDEFINED,             // VkImageLayout oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  // VkImageLayout newLayout
        VK_QUEUE_FAMILY_IGNORED,               // uint32_t srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,               // uint32_t dstQueueFamilyIndex
        swap_chain_images[i],    // VkImage                                image
        image_subresource_range  // VkImageSubresourceRange subresourceRange
    };

    VkImageMemoryBarrier barrier_from_clear_to_present = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // VkStructureType sType
        nullptr,  // const void                            *pNext
        VK_ACCESS_TRANSFER_WRITE_BIT,          // VkAccessFlags srcAccessMask
        VK_ACCESS_MEMORY_READ_BIT,             // VkAccessFlags dstAccessMask
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  // VkImageLayout oldLayout
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,       // VkImageLayout newLayout
        VK_QUEUE_FAMILY_IGNORED,               // uint32_t srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,               // uint32_t dstQueueFamilyIndex
        swap_chain_images[i],    // VkImage                                image
        image_subresource_range  // VkImageSubresourceRange subresourceRange
    };

    vkBeginCommandBuffer(surface->GetSwapChain()->GetCurrentCommandBuffer(i)->handle(),
                         &cmd_buffer_begin_info);
    vkCmdPipelineBarrier(surface->GetSwapChain()->GetCurrentCommandBuffer(i)->handle(),
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier_from_present_to_clear);
    vkCmdClearColorImage(surface->GetSwapChain()->GetCurrentCommandBuffer(i)->handle(),
                         swap_chain_images[i],
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1,
                         &image_subresource_range);
    vkCmdPipelineBarrier(surface->GetSwapChain()->GetCurrentCommandBuffer(i)->handle(),
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier_from_clear_to_present);
    if (vkEndCommandBuffer(surface->GetSwapChain()->GetCurrentCommandBuffer(i)->handle()) !=
        VK_SUCCESS) {
      std::cout << "Could not record command buffers!" << std::endl;
      return;
    }
  }

  // Run loop
  // Prepare notification for window destruction
  Atom delete_window_atom;
  delete_window_atom =
      XInternAtom(gfx::GetXDisplay(), "WM_DELETE_WINDOW", false);
  XSetWMProtocols(gfx::GetXDisplay(), window(), &delete_window_atom, 1);

  // Display window
  XClearWindow(gfx::GetXDisplay(), window());
  XMapWindow(gfx::GetXDisplay(), window());

  // Main message loop
  XEvent event;
  bool loop = true;
  bool resize = false;
  bool result = true;

  while (loop) {
    if (XPending(gfx::GetXDisplay())) {
      XNextEvent(gfx::GetXDisplay(), &event);
      switch (event.type) {
        // Process events
        case ConfigureNotify: {
          static int width = event.xconfigure.width;
          static int height = event.xconfigure.height;

          if (((event.xconfigure.width > 0) &&
               (event.xconfigure.width != width)) ||
              ((event.xconfigure.height > 0) &&
               (event.xconfigure.width != height))) {
            width = event.xconfigure.width;
            height = event.xconfigure.height;
            resize = true;
          }
        } break;
        case KeyPress:
          loop = false;
          break;
        case DestroyNotify:
          loop = false;
          break;
        case ClientMessage:
          if (static_cast<unsigned int>(event.xclient.data.l[0]) ==
              delete_window_atom) {
            loop = false;
          }
          break;
      }
    } else {
      // Draw
      if (GetDeviceQueue()->ReadyToDraw()) {
        if (surface->SwapBuffers() != gfx::SwapResult::SWAP_ACK) {
          result = false;
          break;
        }
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  }

  surface->Destroy();
}

TEST_F(BasicVulkanTest, BasicVulkanSurface2) {
  GetDeviceQueue()->Initialize(
      VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG |
      VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG);
  std::unique_ptr<VulkanSurface> surface =
      VulkanSurface::CreateViewSurface(window());
  EXPECT_TRUE(surface);
  EXPECT_TRUE(surface->CreateSurface());

  SetSurface(surface.get());

  EXPECT_TRUE(surface->Initialize(GetDeviceQueue(),
                                  VulkanSurface::DEFAULT_SURFACE_FORMAT));

  VulkanRenderPass render_pass(GetDeviceQueue());
  //EXPECT_TRUE(render_pass.Initialize(surface->GetSwapChain()));

  const std::string kVertexShaderSource =
      "#version 450\n"
      "out gl_PerVertex\n"
      "{"
      "  vec4 gl_Position;"
      "};\n"
      "void main() {"
      "    vec2 pos[3] = vec2[3]( vec2(-0.7, 0.7), vec2(0.7, 0.7), vec2(0.0, "
      "-0.7) );"
      "    gl_Position = vec4( pos[gl_VertexIndex], 0.0, 1.0 );"
      "}";

  const std::string kFragShaderSource =
      "#version 450\n"
      "layout(location = 0) out vec4 out_Color;\n"
      "void main() {"
      "  out_Color = vec4( 0.0, 0.4, 1.0, 1.0 );\n"
      "}";

 
      render_pass.CreatePipeline(kVertexShaderSource, kFragShaderSource,
         VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, true);

  VkCommandBufferBeginInfo graphics_commandd_buffer_begin_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  // VkStructureType sType
      nullptr,  // const void                            *pNext
      VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,  // VkCommandBufferUsageFlags
                                                     // flags
      nullptr  // const VkCommandBufferInheritanceInfo  *pInheritanceInfo
  };

  VkClearValue clear_value = {
      {{1.0f, 0.8f, 0.4f, 0.0f}},  // VkClearColorValue              color
  };

  uint32_t image_count = surface->GetSwapChain()->num_images();
  render_pass.frame_buffers_.resize(image_count);

  for (size_t i = 0; i < image_count; ++i) {
    render_pass.CreateFrameBuffer(surface->GetSwapChain(), i);

    vkBeginCommandBuffer(surface->GetSwapChain()->GetCurrentCommandBuffer(i)->handle(),
                         &graphics_commandd_buffer_begin_info);
    VkRenderPassBeginInfo render_pass_begin_info = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,  // VkStructureType sType
        nullptr,               // const void                    *pNext
        render_pass.handle(),  // VkRenderPass                   renderPass
        render_pass.frame_buffers_[i],  // VkFramebuffer framebuffer
        {// VkRect2D                       renderArea
         {
             // VkOffset2D                     offset
             0,  // int32_t                        x
             0   // int32_t                        y
         },
         {
             // VkExtent2D                     extent
             300,  // int32_t                        width
             300,  // int32_t                        height
         }},
        1,            // uint32_t                       clearValueCount
        &clear_value  // const VkClearValue            *pClearValues
    };

    vkCmdBeginRenderPass(surface->GetSwapChain()->GetCurrentCommandBuffer(i)->handle(),
                         &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(surface->GetSwapChain()->GetCurrentCommandBuffer(i)->handle(),
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      render_pass.GetGraphicsPipeline());

    vkCmdDraw(surface->GetSwapChain()->GetCurrentCommandBuffer(i)->handle(), 3, 1, 0, 0);
    vkCmdEndRenderPass(surface->GetSwapChain()->GetCurrentCommandBuffer(i)->handle());

    if (vkEndCommandBuffer(surface->GetSwapChain()->GetCurrentCommandBuffer(i)->handle()) !=
        VK_SUCCESS) {
      std::cout << "Could not record command buffer!" << std::endl;
      return;
    }
  }

  // Run loop
  // Prepare notification for window destruction
  Atom delete_window_atom;
  delete_window_atom =
      XInternAtom(gfx::GetXDisplay(), "WM_DELETE_WINDOW", false);
  XSetWMProtocols(gfx::GetXDisplay(), window(), &delete_window_atom, 1);

  // Display window
  XClearWindow(gfx::GetXDisplay(), window());
  XMapWindow(gfx::GetXDisplay(), window());

  // Main message loop
  XEvent event;
  bool loop = true;
  bool resize = false;
  bool result = true;

  while (loop) {
    if (XPending(gfx::GetXDisplay())) {
      XNextEvent(gfx::GetXDisplay(), &event);
      switch (event.type) {
        // Process events
        case ConfigureNotify: {
          static int width = event.xconfigure.width;
          static int height = event.xconfigure.height;

          if (((event.xconfigure.width > 0) &&
               (event.xconfigure.width != width)) ||
              ((event.xconfigure.height > 0) &&
               (event.xconfigure.width != height))) {
            width = event.xconfigure.width;
            height = event.xconfigure.height;
            resize = true;
          }
        } break;
        case KeyPress:
          loop = false;
          break;
        case DestroyNotify:
          loop = false;
          break;
        case ClientMessage:
          if (static_cast<unsigned int>(event.xclient.data.l[0]) ==
              delete_window_atom) {
            loop = false;
          }
          break;
      }
    } else {
      // Draw
      if (GetDeviceQueue()->ReadyToDraw()) {
        if (surface->SwapBuffers() != gfx::SwapResult::SWAP_ACK) {
          result = false;
          break;
        }
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  }

  render_pass.Destroy();
  surface->Destroy();
}

TEST_F(BasicVulkanTest, BasicVulkanSurface3) {
  GetDeviceQueue()->Initialize(
      VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG |
      VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG);
  std::unique_ptr<VulkanSurface> surface =
      VulkanSurface::CreateViewSurface(window());
  EXPECT_TRUE(surface);
  EXPECT_TRUE(surface->CreateSurface());
  SetSurface(surface.get());

  EXPECT_TRUE(surface->Initialize(GetDeviceQueue(),
                                  VulkanSurface::DEFAULT_SURFACE_FORMAT));

  // Create a RenderPass
  VulkanRenderPass render_pass(GetDeviceQueue());
  // Create a render pass.
  // Render pass is a set of data required to perform some drawing operations.
  std::vector<VkSubpassDependency> subpass_dependencies = {
      {
          VK_SUBPASS_EXTERNAL,  // uint32_t srcSubpass
          0,                    // uint32_t dstSubpass
          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,           // VkPipelineStageFlags
                                                          // srcStageMask
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // VkPipelineStageFlags
                                                          // dstStageMask
          VK_ACCESS_MEMORY_READ_BIT,             // VkAccessFlags srcAccessMask
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  // VkAccessFlags dstAccessMask
          VK_DEPENDENCY_BY_REGION_BIT  // VkDependencyFlags dependencyFlags
      },
      {
          0,                    // uint32_t srcSubpass
          VK_SUBPASS_EXTERNAL,  // uint32_t dstSubpass
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // VkPipelineStageFlags
          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,           // VkPipelineStageFlags
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  // VkAccessFlags srcAccessMask
          VK_ACCESS_MEMORY_READ_BIT,             // VkAccessFlags dstAccessMask
          VK_DEPENDENCY_BY_REGION_BIT  // VkDependencyFlags dependencyFlags
      }};

  render_pass.Initialize(surface->GetSwapChain(), subpass_dependencies);

  const std::string kVertexShaderSource =
      "#version 450\n"
      "layout(location = 0) in vec4 i_Position;\n"
      "layout(location = 1) in vec4 i_Color;\n"
      "out gl_PerVertex"
      "{"
      "  vec4 gl_Position;"
      "};"
      "layout(location = 0) out vec4 v_Color;"
      "void main() {"
      "  gl_Position = i_Position;"
      "  v_Color = i_Color;"
      "}";

  const std::string kFragShaderSource =
      "#version 450\n"
      "layout(location = 0) in vec4 v_Color;"
      "layout(location = 0) out vec4 o_Color;"
      "void main() {"
      "o_Color = v_Color;"
      "}";

      render_pass.CreatePipeline(kVertexShaderSource, kFragShaderSource,
	      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, true);

  // Tutorial04::CreateVertexBuffer
  VulkanBuffer::VertexData vertex_data[] = {
      {-0.7f, -0.7f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f},
      {-0.7f, 0.7f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f},
      {0.7f, -0.7f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f},
      {0.7f, 0.7f, 0.0f, 1.0f, 0.3f, 0.3f, 0.3f, 0.0f}};

  VulkanBuffer vertexBuffer;
  vertexBuffer.Initialize(GetDeviceQueue(), vertex_data, 4);

  // Run loop
  // Prepare notification for window destruction
  Atom delete_window_atom;
  delete_window_atom =
      XInternAtom(gfx::GetXDisplay(), "WM_DELETE_WINDOW", false);
  XSetWMProtocols(gfx::GetXDisplay(), window(), &delete_window_atom, 1);

  // Display window
  XClearWindow(gfx::GetXDisplay(), window());
  XMapWindow(gfx::GetXDisplay(), window());

  // Main message loop
  XEvent event;
  bool loop = true;
  bool resize = false;

  while (loop) {
    if (XPending(gfx::GetXDisplay())) {
      XNextEvent(gfx::GetXDisplay(), &event);
      switch (event.type) {
        // Process events
        case ConfigureNotify: {
          static int width = event.xconfigure.width;
          static int height = event.xconfigure.height;

          if (((event.xconfigure.width > 0) &&
               (event.xconfigure.width != width)) ||
              ((event.xconfigure.height > 0) &&
               (event.xconfigure.width != height))) {
            width = event.xconfigure.width;
            height = event.xconfigure.height;
            resize = true;
          }
        } break;
        case KeyPress:
          loop = false;
          break;
        case DestroyNotify:
          loop = false;
          break;
        case ClientMessage:
          if (static_cast<unsigned int>(event.xclient.data.l[0]) ==
              delete_window_atom) {
            loop = false;
          }
          break;
      }
    } else {
      // Draw
      if (!GetDeviceQueue()->ReadyToDraw()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        printf("not ready to draw\n");
        continue;
      }

      // Tutorial04::Draw()
      static uint32_t resource_index = 0;
      uint32_t image_index = 0;
      surface->GetSwapChain()->WaitFences(&resource_index, &image_index);
      printf(" new image_index=%d\n", image_index);
      printf(" resource_index=%d\n", resource_index);
      // Tutorial04::PrepareFrame() is called in Draw();
      if (!render_pass.CreateFrameBuffer(surface->GetSwapChain(),
                                         resource_index)) {
        printf("fail to create a frame buffer\n");
        return;
      }

      VkCommandBufferBeginInfo command_buffer_begin_info = {
          VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  // VkStructureType sType
          nullptr,  // const void                            *pNext
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,  // VkCommandBufferUsageFlags
                                                        // flags
          nullptr  // const VkCommandBufferInheritanceInfo  *pInheritanceInfo
      };

      vkBeginCommandBuffer(
          surface->GetSwapChain()->GetCurrentCommandBuffer(resource_index)->handle(),
          &command_buffer_begin_info);

      VkImageSubresourceRange image_subresource_range = {
          VK_IMAGE_ASPECT_COLOR_BIT,  // VkImageAspectFlags aspectMask
          0,  // uint32_t                               baseMipLevel
          1,  // uint32_t                               levelCount
          0,  // uint32_t                               baseArrayLayer
          1   // uint32_t                               layerCount
      };

      if (GetDeviceQueue()->GetPresentQueue() !=
          GetDeviceQueue()->GetGraphicsQueue()) {
        VkImageMemoryBarrier barrier_from_present_to_draw = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // VkStructureType sType
            nullptr,  // const void                            *pNext
            VK_ACCESS_MEMORY_READ_BIT,        // VkAccessFlags srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,        // VkAccessFlags dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,        // VkImageLayout oldLayout
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  // VkImageLayout newLayout
            GetDeviceQueue()
                ->GetPresentQueueFamilyIndex(),  // uint32_t srcQueueFamilyIndex
            GetDeviceQueue()
                ->GetGraphicsQueueFamilyIndex(),  // uint32_t
                                                  // dstQueueFamilyIndex
            surface->GetSwapChain()->GetImage(image_index),  // VkImage image
            image_subresource_range  // VkImageSubresourceRange subresourceRange
        };

        vkCmdPipelineBarrier(
            surface->GetSwapChain()->GetCurrentCommandBuffer(resource_index)->handle(),
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0,
            nullptr, 1, &barrier_from_present_to_draw);
      }

      VkClearValue clear_value = {
          {{1.0f, 0.8f, 0.4f, 0.0f}},  // VkClearColorValue color
      };

      VkRenderPassBeginInfo render_pass_begin_info = {
          VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,  // VkStructureType sType
          nullptr,               // const void                            *pNext
          render_pass.handle(),  // VkRenderPass renderPass
          render_pass
              .frame_buffers_[resource_index],  // VkFramebuffer framebuffer
          {
              // VkRect2D                               renderArea
              {
                  // VkOffset2D                             offset
                  0,  // int32_t                                x
                  0   // int32_t                                y
              },
              surface->GetSwapChain()->GetExtent(),  // VkExtent2D extent;
          },
          1,  // uint32_t                               clearValueCount
          &clear_value  // const VkClearValue                    *pClearValues
      };

      vkCmdBeginRenderPass(
          surface->GetSwapChain()->GetCurrentCommandBuffer(resource_index)->handle(),
          &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(
          surface->GetSwapChain()->GetCurrentCommandBuffer(resource_index)->handle(),
          VK_PIPELINE_BIND_POINT_GRAPHICS, render_pass.GetGraphicsPipeline());

      VkViewport viewport = {
          0.0f,  // float            x
          0.0f,  // float            y
          static_cast<float>(
              surface->GetSwapChain()->GetExtent().width),  // float width
          static_cast<float>(
              surface->GetSwapChain()->GetExtent().height),  // float height
          0.0f,  // float            minDepth
          1.0f   // float            maxDepth
      };

      VkRect2D scissor = {
          {
              // VkOffset2D        offset
              0,  // int32_t           x
              0   // int32_t           y
          },
          {
              // VkExtent2D        extent
              surface->GetSwapChain()->GetExtent().width,  // uint32_t width
              surface->GetSwapChain()->GetExtent().height  // uint32_t height
          }};

      vkCmdSetViewport(
          surface->GetSwapChain()->GetCurrentCommandBuffer(resource_index)->handle(), 0, 1,
          &viewport);
      vkCmdSetScissor(
          surface->GetSwapChain()->GetCurrentCommandBuffer(resource_index)->handle(), 0, 1,
          &scissor);

      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(
          surface->GetSwapChain()->GetCurrentCommandBuffer(resource_index)->handle(), 0, 1,
          vertexBuffer.handle(), &offset);
      vkCmdDraw(surface->GetSwapChain()->GetCurrentCommandBuffer(resource_index)->handle(), 4,
                1, 0, 0);
      vkCmdEndRenderPass(
          surface->GetSwapChain()->GetCurrentCommandBuffer(resource_index)->handle());

      if (GetDeviceQueue()->GetGraphicsQueue() !=
          GetDeviceQueue()->GetPresentQueue()) {
        VkImageMemoryBarrier barrier_from_draw_to_present = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // VkStructureType sType
            nullptr,  // const void                            *pNext
            VK_ACCESS_MEMORY_READ_BIT,        // VkAccessFlags srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,        // VkAccessFlags dstAccessMask
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  // VkImageLayout oldLayout
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  // VkImageLayout newLayout
            GetDeviceQueue()
                ->GetPresentQueueFamilyIndex(),  // uint32_t srcQueueFamilyIndex
            GetDeviceQueue()
                ->GetGraphicsQueueFamilyIndex(),  // uint32_t
                                                  // dstQueueFamilyIndex
            surface->GetSwapChain()->GetImage(image_index),  // VkImage image
            image_subresource_range  // VkImageSubresourceRange subresourceRange
        };
        vkCmdPipelineBarrier(
            surface->GetSwapChain()->GetCurrentCommandBuffer(resource_index)->handle(),
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
            &barrier_from_draw_to_present);
      }

      if (vkEndCommandBuffer(surface->GetSwapChain()->GetCurrentCommandBuffer(
              resource_index)->handle()) != VK_SUCCESS) {
        std::cout << "Could not record command buffer!" << std::endl;
        return;
      }
      // end of Tutorial04::PrepareFrame
      surface->GetSwapChain()->SwapBuffer2(resource_index, &image_index);
      printf("Draw_end\n");
    }
  }  // end of while

  render_pass.Destroy();
  surface->Destroy();
}

}  // namespace gpu
