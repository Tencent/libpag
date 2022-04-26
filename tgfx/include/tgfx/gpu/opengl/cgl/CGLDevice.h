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

#include <AppKit/AppKit.h>
#include "tgfx/gpu/opengl/GLDevice.h"

namespace tgfx {
class CGLDevice : public GLDevice {
 public:
  /**
   * Creates an CGL device with adopted CGL context.
   */
  static std::shared_ptr<CGLDevice> MakeAdopted(CGLContextObj cglContext);

  ~CGLDevice() override;

  bool sharableWith(void* nativeHandle) const override;

  CGLContextObj cglContext() const;

  CVOpenGLTextureCacheRef getTextureCache();

  void releaseTexture(CVOpenGLTextureRef texture);

 protected:
  bool onMakeCurrent() override;
  void onClearCurrent() override;

 private:
  NSOpenGLContext* glContext = nil;
  CGLContextObj oldContext = nil;
  CVOpenGLTextureCacheRef textureCache = nil;

  static std::shared_ptr<CGLDevice> Wrap(CGLContextObj cglContext, bool isAdopted = false);

  explicit CGLDevice(CGLContextObj cglContext);

  friend class GLDevice;
  friend class CGLWindow;
};
}  // namespace tgfx
