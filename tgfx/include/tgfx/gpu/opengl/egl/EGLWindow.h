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

#include "EGLDevice.h"
#include "tgfx/gpu/Window.h"

namespace tgfx {
class EGLWindow : public Window {
 public:
  /**
   * Returns an EGLWindow associated with current EGLSurface. Returns nullptr if there is no current
   * EGLSurface on the calling thread.
   * If the rendering size changes，eglQuerySurface based on ANativeWindow may give the wrong size.
   */
  static std::shared_ptr<EGLWindow> Current(int width, int height);

  /**
   * Creates a new window from an EGL native window with specified shared context.
   * If the rendering size changes，eglQuerySurface based on ANativeWindow may give the wrong size.
   */
  static std::shared_ptr<EGLWindow> MakeFrom(EGLNativeWindowType nativeWindow, int width,
                                             int height, EGLContext sharedContext = nullptr);

 protected:
  std::shared_ptr<Surface> onCreateSurface(Context* context) override;
  void onPresent(Context* context, int64_t presentationTime) override;

 private:
  int _width = 0;
  int _height = 0;

  explicit EGLWindow(std::shared_ptr<Device> device, int width, int height);
};
}  // namespace tgfx
