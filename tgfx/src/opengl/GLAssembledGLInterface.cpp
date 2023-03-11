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

#include "GLAssembledGLInterface.h"

#include "GLInterface.h"

namespace tgfx {
static void InitTextureBarrier(const GLProcGetter* getter, GLFunctions* functions,
                               const GLInfo& info) {
  if (info.version >= GL_VER(4, 5) || info.hasExtension("GL_ARB_texture_barrier")) {
    functions->textureBarrier =
        reinterpret_cast<GLTextureBarrier*>(getter->getProcAddress("glTextureBarrier"));
  } else if (info.hasExtension("GL_NV_texture_barrier")) {
    functions->textureBarrier =
        reinterpret_cast<GLTextureBarrier*>(getter->getProcAddress("glTextureBarrierNV"));
  }
}

static void InitBlitFrameBuffer(const GLProcGetter* getter, GLFunctions* functions,
                                const GLInfo& info) {
  if (info.version >= GL_VER(3, 0) || info.hasExtension("GL_ARB_framebuffer_object")) {
    functions->blitFramebuffer =
        reinterpret_cast<GLBlitFramebuffer*>(getter->getProcAddress("glBlitFramebuffer"));
  } else if (info.hasExtension("GL_EXT_framebuffer_blit")) {
    functions->blitFramebuffer =
        reinterpret_cast<GLBlitFramebuffer*>(getter->getProcAddress("glBlitFramebufferEXT"));
  }
}

static void InitVertexArray(const GLProcGetter* getter, GLFunctions* functions,
                            const GLInfo& info) {
  if (info.version >= GL_VER(3, 0) || info.hasExtension("GL_ARB_vertex_array_object")) {
    functions->bindVertexArray =
        reinterpret_cast<GLBindVertexArray*>(getter->getProcAddress("glBindVertexArray"));
    functions->deleteVertexArrays =
        reinterpret_cast<GLDeleteVertexArrays*>(getter->getProcAddress("glDeleteVertexArrays"));
    functions->genVertexArrays =
        reinterpret_cast<GLGenVertexArrays*>(getter->getProcAddress("glGenVertexArrays"));
  } else if (info.hasExtension("GL_APPLE_vertex_array_object")) {
    functions->bindVertexArray =
        reinterpret_cast<GLBindVertexArray*>(getter->getProcAddress("glBindVertexArrayAPPLE"));
    functions->deleteVertexArrays = reinterpret_cast<GLDeleteVertexArrays*>(
        getter->getProcAddress("glDeleteVertexArraysAPPLE"));
    functions->genVertexArrays =
        reinterpret_cast<GLGenVertexArrays*>(getter->getProcAddress("glGenVertexArraysAPPLE"));
  }
}

static void InitRenderbufferStorageMultisample(const GLProcGetter* getter, GLFunctions* functions,
                                               const GLInfo& info) {
  if (info.version >= GL_VER(3, 0) || info.hasExtension("GL_ARB_framebuffer_object")) {
    functions->renderbufferStorageMultisample = reinterpret_cast<GLRenderbufferStorageMultisample*>(
        getter->getProcAddress("glRenderbufferStorageMultisample"));
  } else if (info.hasExtension("GL_EXT_framebuffer_multisample")) {
    functions->renderbufferStorageMultisample = reinterpret_cast<GLRenderbufferStorageMultisample*>(
        getter->getProcAddress("glRenderbufferStorageMultisampleEXT"));
  }
}

void GLAssembleGLInterface(const GLProcGetter* getter, GLFunctions* functions, const GLInfo& info) {
  InitTextureBarrier(getter, functions, info);
  InitBlitFrameBuffer(getter, functions, info);
  InitRenderbufferStorageMultisample(getter, functions, info);
  InitVertexArray(getter, functions, info);
}
}  // namespace tgfx
