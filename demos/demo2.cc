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

  // Create a render pass.
  // Render pass is a set of data required to perform some drawing operations.
  VulkanRenderPass render_pass(&device_queue_);
  render_pass.Initialize(surface->GetSwapChain());

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

  // Create a pipeline using vkCreatePipelineLayout and
  // vkCreateGraphicsPipelines.
  render_pass.CreatePipeline(kVertexShaderSource, kFragShaderSource,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  VkCommandBufferBeginInfo graphics_commandd_buffer_begin_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  // VkStructureTyp sType
      nullptr,  // const void                 *pNext
      VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,  // VkCommandBufferUsageFlags
      nullptr  // const VkCommandBufferInheritanceInfo  *pInheritanceInfo
  };

  VkClearValue clear_value = {
      {{1.0f, 0.8f, 0.4f, 0.0f}},  // VkClearColorValue              color
  };

  uint32_t image_count = surface->GetSwapChain()->num_images();
  render_pass.frame_buffers_.resize(image_count);

  for (uint32_t i = 0; i < image_count; ++i) {
    // A framebuffer is a set of textures(attachments) we are rendering into
    // using vkCreateFrameBufer The framebuffer specifies what images are used
    // as attachmenets on which the render pass operates.9
    render_pass.CreateFrameBuffer(surface->GetSwapChain(), i);
    vkBeginCommandBuffer(*surface->GetSwapChain()->GetCommandBuffer(i),
                         &graphics_commandd_buffer_begin_info);
    VkRenderPassBeginInfo render_pass_begin_info = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,  // VkStructureType sType
        nullptr,                        // const void              *pNext
        render_pass.handle(),           // VkRenderPass             renderPass
        render_pass.frame_buffers_[i],  // VkFramebuffer            framebuffer
        {                               // VkRect2D                 renderArea
         {
             // VkOffset2D               offset
             0,  // int32_t                  x
             0   // int32_t                  y
         },
         {
             // VkExtent2D               extent
             300,  // int32_t                  width
             300,  // int32_t                  height
         }},
        1,            // uint32_t                       clearValueCount
        &clear_value  // const VkClearValue            *pClearValues
    };

    vkCmdBeginRenderPass(*surface->GetSwapChain()->GetCommandBuffer(i),
                         &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(*surface->GetSwapChain()->GetCommandBuffer(i),
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      render_pass.GetGraphicsPipeline());
    vkCmdDraw(*surface->GetSwapChain()->GetCommandBuffer(i), 3, 1, 0, 0);
    vkCmdEndRenderPass(*surface->GetSwapChain()->GetCommandBuffer(i));

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

  render_pass.Destroy();
  surface->Destroy();

  gpu::DestroyNativeWindow(window_);
  window_ = gfx::kNullAcceleratedWidget;
  device_queue_.Destroy();

  return 0;
}
