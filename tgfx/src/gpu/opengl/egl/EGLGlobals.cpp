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

#include "EGLGlobals.h"
#include <EGL/eglext.h>
#if defined(_WIN32)
#include "EGLDisplayWrapper.h"
#endif

namespace tgfx {

EGLGlobals InitializeEGL() {
  EGLGlobals globals = {};
  EGLint majorVersion;
  EGLint minorVersion;
#if defined(_WIN32)
  do {
    globals.display = EGLDisplayWrapper::EGLGetPlatformDisplay();
  } while (eglInitialize(globals.display, &majorVersion, &minorVersion) == EGL_FALSE &&
           EGLDisplayWrapper::HasNext());
#else
  globals.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(globals.display, &majorVersion, &minorVersion);
#endif
  eglBindAPI(EGL_OPENGL_ES_API);
  EGLint numConfigs = 0;
  const EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                                  EGL_WINDOW_BIT,
                                  EGL_RENDERABLE_TYPE,
                                  EGL_OPENGL_ES2_BIT,
                                  EGL_RED_SIZE,
                                  8,
                                  EGL_GREEN_SIZE,
                                  8,
                                  EGL_BLUE_SIZE,
                                  8,
                                  EGL_ALPHA_SIZE,
                                  8,
                                  EGL_STENCIL_SIZE,
                                  8,
                                  EGL_NONE};
  eglChooseConfig(globals.display, configAttribs, &globals.windowConfig, 1, &numConfigs);

  const EGLint configPbufferAttribs[] = {EGL_SURFACE_TYPE,
                                         EGL_PBUFFER_BIT,
                                         EGL_RENDERABLE_TYPE,
                                         EGL_OPENGL_ES2_BIT,
                                         EGL_RED_SIZE,
                                         8,
                                         EGL_GREEN_SIZE,
                                         8,
                                         EGL_BLUE_SIZE,
                                         8,
                                         EGL_ALPHA_SIZE,
                                         8,
                                         EGL_STENCIL_SIZE,
                                         8,
                                         EGL_NONE};
  eglChooseConfig(globals.display, configPbufferAttribs, &globals.pbufferConfig, 1, &numConfigs);
  return globals;
}

const EGLGlobals* EGLGlobals::Get() {
  static const EGLGlobals globals = InitializeEGL();
  return &globals;
}
}  // namespace tgfx