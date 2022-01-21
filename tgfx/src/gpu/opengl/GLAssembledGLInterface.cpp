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

namespace pag {
static void InitTextureBarrier(const GLProcGetter* getter, GLInterface* interface,
                               const GLInfo& info) {
  if (info.version >= GL_VER(4, 5) || info.hasExtension("GL_ARB_texture_barrier")) {
    interface->textureBarrier =
        reinterpret_cast<GLTextureBarrier*>(getter->getProcAddress("glTextureBarrier"));
  } else if (info.hasExtension("GL_NV_texture_barrier")) {
    interface->textureBarrier =
        reinterpret_cast<GLTextureBarrier*>(getter->getProcAddress("glTextureBarrierNV"));
  }
}

static void InitBlitFrameBuffer(const GLProcGetter* getter, GLInterface* interface,
                                const GLInfo& info) {
  if (info.version >= GL_VER(3, 0) || info.hasExtension("GL_ARB_framebuffer_object")) {
    interface->blitFramebuffer =
        reinterpret_cast<GLBlitFramebuffer*>(getter->getProcAddress("glBlitFramebuffer"));
  } else if (info.hasExtension("GL_EXT_framebuffer_blit")) {
    interface->blitFramebuffer =
        reinterpret_cast<GLBlitFramebuffer*>(getter->getProcAddress("glBlitFramebufferEXT"));
  }
}

static void InitVertexArray(const GLProcGetter* getter, GLInterface* interface,
                            const GLInfo& info) {
  if (info.version >= GL_VER(3, 0) || info.hasExtension("GL_ARB_vertex_array_object")) {
    interface->bindVertexArray =
        reinterpret_cast<GLBindVertexArray*>(getter->getProcAddress("glBindVertexArray"));
    interface->deleteVertexArrays =
        reinterpret_cast<GLDeleteVertexArrays*>(getter->getProcAddress("glDeleteVertexArrays"));
    interface->genVertexArrays =
        reinterpret_cast<GLGenVertexArrays*>(getter->getProcAddress("glGenVertexArrays"));
  } else if (info.hasExtension("GL_APPLE_vertex_array_object")) {
    interface->bindVertexArray =
        reinterpret_cast<GLBindVertexArray*>(getter->getProcAddress("glBindVertexArrayAPPLE"));
    interface->deleteVertexArrays = reinterpret_cast<GLDeleteVertexArrays*>(
        getter->getProcAddress("glDeleteVertexArraysAPPLE"));
    interface->genVertexArrays =
        reinterpret_cast<GLGenVertexArrays*>(getter->getProcAddress("glGenVertexArraysAPPLE"));
  }
}

static void InitRenderbufferStorageMultisample(const GLProcGetter* getter, GLInterface* interface,
                                               const GLInfo& info) {
  if (info.version >= GL_VER(3, 0) || info.hasExtension("GL_ARB_framebuffer_object")) {
    interface->renderbufferStorageMultisample = reinterpret_cast<GLRenderbufferStorageMultisample*>(
        getter->getProcAddress("glRenderbufferStorageMultisample"));
  } else if (info.hasExtension("GL_EXT_framebuffer_multisample")) {
    interface->renderbufferStorageMultisample = reinterpret_cast<GLRenderbufferStorageMultisample*>(
        getter->getProcAddress("glRenderbufferStorageMultisampleEXT"));
  }
}

void GLAssembleGLInterface(const GLProcGetter* getter, GLInterface* interface, const GLInfo& info) {
  interface->checkFramebufferStatus = reinterpret_cast<GLCheckFramebufferStatus*>(
      getter->getProcAddress("glCheckFramebufferStatus"));
  interface->getError = reinterpret_cast<GLGetError*>(getter->getProcAddress("glGetError"));
  InitTextureBarrier(getter, interface, info);
  InitBlitFrameBuffer(getter, interface, info);
  InitRenderbufferStorageMultisample(getter, interface, info);
  InitVertexArray(getter, interface, info);
}
}  // namespace pag
