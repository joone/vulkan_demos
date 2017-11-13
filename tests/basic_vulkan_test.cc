// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "basic_vulkan_test.h"

#include "native_window.h"
#include "ui/gfx/geometry/rect.h"

namespace gpu {

void BasicVulkanTest::SetUp() {
  const gfx::Rect kDefaultBounds(10, 10, 500, 500);
  window_ = CreateNativeWindow(kDefaultBounds);
}

void BasicVulkanTest::TearDown() {
  DestroyNativeWindow(window_);
  window_ = gfx::kNullAcceleratedWidget;
  device_queue_.Destroy();
}

}  // namespace gpu
