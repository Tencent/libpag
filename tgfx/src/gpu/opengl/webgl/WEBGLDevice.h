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

#include <emscripten/html5_webgl.h>

#include "gpu/opengl/GLDevice.h"

namespace pag {
class WEBGLDevice : public GLDevice {
 public:
  /**
   * Returns an WEBGL device associated with current OpenGL context. Returns nullptr if there is no
   * current OpenGL context on the calling thread.
   */
  static std::shared_ptr<WEBGLDevice> Current();

  /**
   * Creates a new WEBGL device from a canvas.
   */
  static std::shared_ptr<WEBGLDevice> MakeFrom(const std::string& canvasID);

  ~WEBGLDevice() override;

  bool sharableWith(void* nativeHandle) const override;

 protected:
  bool onMakeCurrent() override;
  void onClearCurrent() override;

 private:
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = 0;
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE oldContext = 0;

  static std::shared_ptr<WEBGLDevice> Wrap(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context,
                                           bool isAdopted = false);

  WEBGLDevice(std::unique_ptr<Context> context, EMSCRIPTEN_WEBGL_CONTEXT_HANDLE nativeHandle);

  friend class WEBGLWindow;
};
}  // namespace pag
