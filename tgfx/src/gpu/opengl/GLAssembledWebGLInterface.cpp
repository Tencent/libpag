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

#include "GLAssembledWebGLInterface.h"

#include "GLInterface.h"

namespace tgfx {
static unsigned GetErrorFake() {
  return GL::NO_ERROR;
}

static unsigned CheckFramebufferStatusFake(unsigned) {
  return GL::FRAMEBUFFER_COMPLETE;
}

static void InitVertexArray(const GLProcGetter* getter, GLInterface* interface,
                            const GLInfo& info) {
  if (info.version >= GL_VER(2, 0)) {
    interface->bindVertexArray =
        reinterpret_cast<GLBindVertexArray*>(getter->getProcAddress("glBindVertexArray"));
    interface->deleteVertexArrays =
        reinterpret_cast<GLDeleteVertexArrays*>(getter->getProcAddress("glDeleteVertexArrays"));
    interface->genVertexArrays =
        reinterpret_cast<GLGenVertexArrays*>(getter->getProcAddress("glGenVertexArrays"));
  } else if (info.hasExtension("GL_OES_vertex_array_object") ||
             info.hasExtension("OES_vertex_array_object")) {
    interface->bindVertexArray =
        reinterpret_cast<GLBindVertexArray*>(getter->getProcAddress("glBindVertexArrayOES"));
    interface->deleteVertexArrays =
        reinterpret_cast<GLDeleteVertexArrays*>(getter->getProcAddress("glDeleteVertexArraysOES"));
    interface->genVertexArrays =
        reinterpret_cast<GLGenVertexArrays*>(getter->getProcAddress("glGenVertexArraysOES"));
  }
}

void GLAssembleWebGLInterface(const GLProcGetter* getter, GLInterface* interface,
                              const GLInfo& info) {
  interface->getError = reinterpret_cast<GLGetError*>(GetErrorFake);
  interface->checkFramebufferStatus =
      reinterpret_cast<GLCheckFramebufferStatus*>(CheckFramebufferStatusFake);
  if (info.version >= GL_VER(2, 0)) {
    interface->blitFramebuffer =
        reinterpret_cast<GLBlitFramebuffer*>(getter->getProcAddress("glBlitFramebuffer"));
    interface->renderbufferStorageMultisample = reinterpret_cast<GLRenderbufferStorageMultisample*>(
        getter->getProcAddress("glRenderbufferStorageMultisample"));
  }
  InitVertexArray(getter, interface, info);
}
}  // namespace tgfx
