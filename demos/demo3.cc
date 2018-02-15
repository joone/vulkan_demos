#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "base/command_line.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11_types.h"

#include "../tests/native_window.h"
#include "../vulkan/vulkan_buffer.h"
#include "../vulkan/vulkan_command_pool.h"
#include "../vulkan/vulkan_command_buffer.h"
#include "../vulkan/vulkan_device_queue.h"
#include "../vulkan/vulkan_implementation.h"
#include "../vulkan/vulkan_render_pass.h"
#include "../vulkan/vulkan_surface.h"
#include "../vulkan/vulkan_swap_chain.h"

using namespace gpu;

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // Create a window.
  gfx::AcceleratedWidget window_ = gfx::kNullAcceleratedWidget;
  const gfx::Rect kDefaultBounds(10, 10, 500, 500);
  window_ = gpu::CreateNativeWindow(kDefaultBounds);

  // Createa a Vulkan instrance.
  const bool success = gpu::InitializeVulkan();
  CHECK(success);

  // Create a device and queue.
  gpu::VulkanDeviceQueue device_queue;
  device_queue.Initialize(VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG |
                          VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG);

  // Create a Xlib surface and swap chain.
  std::unique_ptr<VulkanSurface> surface =
      VulkanSurface::CreateViewSurface(window_);
  surface->CreateSurface();
  surface->Initialize(&device_queue, VulkanSurface::DEFAULT_SURFACE_FORMAT,
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

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

  VulkanRenderPass render_pass(&device_queue);
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

  // Create a pipeline using vkCreatePipelineLayout and
  // vkCreateGraphicsPipelines.
  render_pass.CreatePipeline(kVertexShaderSource, kFragShaderSource,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, true);

  // Tutorial04::CreateVertexBuffer
  VulkanBuffer::VertexData vertex_data[] = {
    {
      -0.7f, -0.7f, 0.0f, 1.0f,
      1.0f, 0.0f, 0.0f, 0.0f
    },
    {
      -0.7f, 0.7f, 0.0f, 1.0f,
      0.0f, 1.0f, 0.0f, 0.0f
    },
    {
      0.7f, -0.7f, 0.0f, 1.0f,
      0.0f, 0.0f, 1.0f, 0.0f
    },
    {
      0.7f, 0.7f, 0.0f, 1.0f,
      0.3f, 0.3f, 0.3f, 0.0f
    }
  };

  VulkanBuffer vertexBuffer;
  vertexBuffer.Initialize(&device_queue, vertex_data, 4);


  // Run loop
  // Prepare notification for window destruction
  Atom delete_window_atom;
  delete_window_atom =
      XInternAtom(gfx::GetXDisplay(), "WM_DELETE_WINDOW", false);
  XSetWMProtocols(gfx::GetXDisplay(), window_, &delete_window_atom, 1);

  // Display window
  XClearWindow(gfx::GetXDisplay(), window_);
  XMapWindow(gfx::GetXDisplay(), window_);

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
      static uint32_t resource_index = 0;
      uint32_t image_index = 0;
      surface->GetSwapChain()->WaitFences(&resource_index, &image_index);
      // Tutorial04::PrepareFrame() is called in Draw();
      if (!render_pass.CreateFrameBuffer(surface->GetSwapChain(),
                                         resource_index)) {
         std::cout << "fail to create a frame buffer\n"  << std::endl;
        return 0;
      }
      // for single use

     /* ScopedSingleUseCommandBufferRecorder
          ScopedSingleUseCommandBufferRecorder;*/
      VkCommandBufferBeginInfo command_buffer_begin_info = {
          VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  // VkStructureType sType
          nullptr,  // const void                            *pNext
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,  // VkCommandBufferUsageFlags
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

      if (device_queue.GetPresentQueue() != device_queue.GetGraphicsQueue()) {
        VkImageMemoryBarrier barrier_from_present_to_draw = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // VkStructureType sType
            nullptr,  // const void                            *pNext
            VK_ACCESS_MEMORY_READ_BIT,        // VkAccessFlags srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,        // VkAccessFlags dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,        // VkImageLayout oldLayout
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  // VkImageLayout newLayout
            device_queue
                .GetPresentQueueFamilyIndex(),  // uint32_t srcQueueFamilyIndex
            device_queue
                .GetGraphicsQueueFamilyIndex(),  // uint32_t dstQueueFamilyIndex
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
                  0,  // x
                  0   // y
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
              surface->GetSwapChain()->GetExtent().width),
          static_cast<float>(
              surface->GetSwapChain()->GetExtent().height),
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
              surface->GetSwapChain()->GetExtent().width,
              surface->GetSwapChain()->GetExtent().height
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

      if (device_queue.GetGraphicsQueue() != device_queue.GetPresentQueue()) {
        VkImageMemoryBarrier barrier_from_draw_to_present = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // VkStructureType sType
            nullptr,  // const void                            *pNext
            VK_ACCESS_MEMORY_READ_BIT,        // VkAccessFlags srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,        // VkAccessFlags dstAccessMask
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  // VkImageLayout oldLayout
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  // VkImageLayout newLayout
            device_queue
                .GetPresentQueueFamilyIndex(),  // uint32_t srcQueueFamilyIndex
            device_queue
                .GetGraphicsQueueFamilyIndex(),  // uint32_t dstQueueFamilyIndex
            surface->GetSwapChain()->GetImage(resource_index),  // VkImage image
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
        return 0;
      }
      // end of Tutorial04::PrepareFrame
      surface->GetSwapChain()->SwapBuffer2(resource_index, &image_index);
    }
  }  // end of while

  render_pass.Destroy();
  surface->Destroy();

  gpu::DestroyNativeWindow(window_);
  window_ = gfx::kNullAcceleratedWidget;
  device_queue.Destroy();

  return 0;
}
