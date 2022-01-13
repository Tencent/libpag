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

#include <EGL/egl.h>
#include <EGL/eglext.h>
#ifdef None
#undef None
#endif
#ifdef Above
#undef Above
#endif
#ifdef Below
#undef Below
#endif

#include "gpu/opengl/GLDevice.h"

namespace pag {
class EGLDevice : public GLDevice {
 public:
  /**
   * Returns an EGL device associated with current OpenGL context. Returns nullptr if there is no
   * current OpenGL context on the calling thread.
   */
  static std::shared_ptr<EGLDevice> Current();

  /**
   * Creates an EGL device with adopted EGLDisplay, EGLSurface and EGLContext.
   */
  static std::shared_ptr<EGLDevice> MakeAdopted(EGLDisplay eglDisplay, EGLSurface eglSurface,
                                                EGLContext eglContext);

  /**
   * Creates a offscreen EGL device with specified shared context.
   */
  static std::shared_ptr<EGLDevice> Make(EGLContext sharedContext = nullptr);

  ~EGLDevice() override;

  bool sharableWith(void* nativeHandle) const override;

 protected:
  bool onMakeCurrent() override;
  void onClearCurrent() override;

 private:
  EGLDisplay eglDisplay = nullptr;
  EGLSurface eglSurface = nullptr;
  EGLContext eglContext = nullptr;
  EGLContext shareContext = nullptr;

  EGLDisplay oldEglDisplay = nullptr;
  EGLContext oldEglContext = nullptr;
  EGLSurface oldEglReadSurface = nullptr;
  EGLSurface oldEglDrawSurface = nullptr;

  static std::shared_ptr<EGLDevice> MakeFrom(EGLNativeWindowType nativeWindow,
                                             EGLContext sharedContext = nullptr);

  static std::shared_ptr<EGLDevice> Wrap(EGLDisplay eglDisplay, EGLSurface eglSurface,
                                         EGLContext eglContext, EGLContext shareContext,
                                         bool isAdopted = false);

  EGLDevice(std::unique_ptr<Context> context, void* nativeHandle);
  void swapBuffers(int64_t timestamp = INT64_MIN);

  friend class EGLWindow;
};
}  // namespace pag
