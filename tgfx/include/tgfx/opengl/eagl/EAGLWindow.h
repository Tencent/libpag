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

#include "EAGLDevice.h"
#include "tgfx/gpu/Window.h"

namespace tgfx {
class EAGLWindow : public Window {
 public:
  /**
   * Creates a new window from a CAEAGLLayer with specified device.
   */
  static std::shared_ptr<EAGLWindow> MakeFrom(CAEAGLLayer* layer,
                                              std::shared_ptr<GLDevice> device = nullptr);
  /**
   * Creates a new window from a CVPixelBuffer with specified device.
   */
  static std::shared_ptr<EAGLWindow> MakeFrom(CVPixelBufferRef pixelBuffer,
                                              std::shared_ptr<GLDevice> device = nullptr);

  ~EAGLWindow() override;

 protected:
  std::shared_ptr<Surface> onCreateSurface(Context* context) override;
  void onPresent(Context* context, int64_t presentationTime) override;

 private:
  unsigned frameBufferID = 0;
  GLuint colorBuffer = 0;
  CAEAGLLayer* layer = nil;
  CVPixelBufferRef pixelBuffer = nil;

  explicit EAGLWindow(std::shared_ptr<Device> device);
};
}  // namespace tgfx
