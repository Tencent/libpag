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
static void InitVertexArray(const GLProcGetter* getter, GLFunctions* functions,
                            const GLInfo& info) {
  if (info.version >= GL_VER(2, 0)) {
    functions->bindVertexArray =
        reinterpret_cast<GLBindVertexArray*>(getter->getProcAddress("glBindVertexArray"));
    functions->deleteVertexArrays =
        reinterpret_cast<GLDeleteVertexArrays*>(getter->getProcAddress("glDeleteVertexArrays"));
    functions->genVertexArrays =
        reinterpret_cast<GLGenVertexArrays*>(getter->getProcAddress("glGenVertexArrays"));
  } else if (info.hasExtension("GL_OES_vertex_array_object") ||
             info.hasExtension("OES_vertex_array_object")) {
    functions->bindVertexArray =
        reinterpret_cast<GLBindVertexArray*>(getter->getProcAddress("glBindVertexArrayOES"));
    functions->deleteVertexArrays =
        reinterpret_cast<GLDeleteVertexArrays*>(getter->getProcAddress("glDeleteVertexArraysOES"));
    functions->genVertexArrays =
        reinterpret_cast<GLGenVertexArrays*>(getter->getProcAddress("glGenVertexArraysOES"));
  }
}

void GLAssembleWebGLInterface(const GLProcGetter* getter, GLFunctions* functions,
                              const GLInfo& info) {
  if (info.version >= GL_VER(2, 0)) {
    functions->blitFramebuffer =
        reinterpret_cast<GLBlitFramebuffer*>(getter->getProcAddress("glBlitFramebuffer"));
    functions->renderbufferStorageMultisample = reinterpret_cast<GLRenderbufferStorageMultisample*>(
        getter->getProcAddress("glRenderbufferStorageMultisample"));
  }
  InitVertexArray(getter, functions, info);
}
}  // namespace tgfx
