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
  gpu::VulkanDeviceQueue device_queue_;
  device_queue_.Initialize(VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG |
                           VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG);
  // Create a Xlib surface and swap chain.
  std::unique_ptr<VulkanSurface> surface =
      VulkanSurface::CreateViewSurface(window_);
  surface->CreateSurface();
  surface->Initialize(&device_queue_, VulkanSurface::DEFAULT_SURFACE_FORMAT);

  VkDevice device = device_queue_.GetVulkanDevice();
  uint32_t image_count = surface->GetSwapChain()->num_images();
  std::vector<VkImage> swap_chain_images(image_count);
  if (vkGetSwapchainImagesKHR(device, surface->GetSwapChain()->handle(),
                              &image_count,
                              &swap_chain_images[0]) != VK_SUCCESS) {
    std::cout << "Could not get swap chain images!" << std::endl;
    return 0;
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

    vkBeginCommandBuffer(*surface->GetSwapChain()->GetCommandBuffer(i),
                         &cmd_buffer_begin_info);
    vkCmdPipelineBarrier(*surface->GetSwapChain()->GetCommandBuffer(i),
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier_from_present_to_clear);
    vkCmdClearColorImage(*surface->GetSwapChain()->GetCommandBuffer(i),
                         swap_chain_images[i],
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1,
                         &image_subresource_range);
    vkCmdPipelineBarrier(*surface->GetSwapChain()->GetCommandBuffer(i),
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier_from_clear_to_present);
    if (vkEndCommandBuffer(*surface->GetSwapChain()->GetCommandBuffer(i)) !=
        VK_SUCCESS) {
      std::cout << "Could not record command buffers!" << std::endl;
      return 0;
    }
  }

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
      if (device_queue_.ReadyToDraw()) {
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

  gpu::DestroyNativeWindow(window_);
  window_ = gfx::kNullAcceleratedWidget;
  device_queue_.Destroy();
  return 0;
}
