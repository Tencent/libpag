/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CGLDevice.h"
#include "tgfx/gpu/Window.h"

namespace tgfx {
class CGLWindow : public Window {
 public:
  /**
   * Creates a new window from an NSView with specified shared context.
   */
  static std::shared_ptr<CGLWindow> MakeFrom(NSView* view, CGLContextObj sharedContext = nullptr);
  /**
   * Creates a new window from a CVPixelBuffer with specified device.
   */
  static std::shared_ptr<CGLWindow> MakeFrom(CVPixelBufferRef pixelBuffer,
                                             std::shared_ptr<GLDevice> device = nullptr);

  ~CGLWindow() override;

 protected:
  std::shared_ptr<Surface> onCreateSurface(Context* context) override;
  void onPresent(Context* context, int64_t presentationTime) override;

 private:
  NSView* view = nil;
  CVPixelBufferRef pixelBuffer = nil;

  explicit CGLWindow(std::shared_ptr<Device> device);
};
}  // namespace tgfx
