// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_window.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "../vulkan/vulkan_device_queue.h"
#include "../vulkan/vulkan_surface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_types.h"

#include <chrono>
#include <thread>

namespace gpu {

gfx::AcceleratedWidget CreateNativeWindow(const gfx::Rect& bounds) {
  printf("%s\n", __func__);
  /*  XDisplay* display = gfx::GetXDisplay();
    XSetWindowAttributes swa;
    swa.event_mask = StructureNotifyMask | ExposureMask;
    swa.override_redirect = True;
    XID window = XCreateWindow(
        display, RootWindow(display, DefaultScreen(display)),  // parent
        bounds.x(), bounds.y(), bounds.width(), bounds.height(),
        0,               // border width
        CopyFromParent,  // depth
        InputOutput,
        CopyFromParent,  // visual
        CWEventMask | CWOverrideRedirect, &swa);
    XMapWindow(display, window);

    while (1) {
      XEvent event;
      XNextEvent(display, &event);
      if (event.type == MapNotify && event.xmap.window == window)
        break;
    }
  */

  XDisplay* display = gfx::GetXDisplay();
  if (!display) {
    return false;
  }

  int default_screen = DefaultScreen(display);

  XID window = XCreateSimpleWindow(
      display, DefaultRootWindow(display), bounds.x(), bounds.y(),
      bounds.width(), bounds.height(), 1, BlackPixel(display, default_screen),
      WhitePixel(display, default_screen));

  // XSync( Parameters.DisplayPtr, false );
  XSetStandardProperties(display, window, "Vulkan Test", "Vulkan Test", None,
                         nullptr, 0, nullptr);
  XSelectInput(display, window,
               ExposureMask | KeyPressMask | StructureNotifyMask);
  return window;
}

void DestroyNativeWindow(gfx::AcceleratedWidget window) {
  printf("%s\n", __func__);
  XDestroyWindow(gfx::GetXDisplay(), window);
}

}  // namespace gpu
