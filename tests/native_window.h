// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace gpu {

class VulkanDeviceQueue;
class VulkanSurface;

gfx::AcceleratedWidget CreateNativeWindow(const gfx::Rect& bounds);
void DestroyNativeWindow(gfx::AcceleratedWidget window);
bool RunMainLoop(gfx::AcceleratedWidget window,
                 VulkanDeviceQueue*,
                 VulkanSurface*);

}  // namespace gpu
