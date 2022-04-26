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

#import <UIKit/UIKit.h>
#include "tgfx/gpu/opengl/GLDevice.h"

namespace tgfx {
class EAGLDevice : public GLDevice {
 public:
  /**
   * Creates an EAGL device with adopted EAGL context.
   */
  static std::shared_ptr<EAGLDevice> MakeAdopted(EAGLContext* eaglContext);

  ~EAGLDevice() override;

  bool sharableWith(void* nativeHandle) const override;

  EAGLContext* eaglContext() const;

  CVOpenGLESTextureCacheRef getTextureCache();

  void releaseTexture(CVOpenGLESTextureRef texture);

 protected:
  bool onMakeCurrent() override;
  void onClearCurrent() override;

 private:
  EAGLContext* _eaglContext = nil;
  EAGLContext* oldContext = nil;
  CVOpenGLESTextureCacheRef textureCache = nil;
  size_t cacheArrayIndex = 0;

  static std::shared_ptr<EAGLDevice> Wrap(EAGLContext* eaglContext, bool isAdopted);
  static void NotifyReferenceReachedZero(EAGLDevice* device);

  explicit EAGLDevice(EAGLContext* eaglContext);
  bool makeCurrent(bool force = false);
  void clearCurrent();
  void finish();

  friend class GLDevice;
  friend class EAGLWindow;

  friend void ApplicationWillResignActive();
  friend void ApplicationDidBecomeActive();
};
}  // namespace tgfx
