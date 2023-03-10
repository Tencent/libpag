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

#include "GLAssembledGLESInterface.h"

#include "GLInterface.h"

namespace tgfx {
static void InitFramebufferTexture2DMultisample(const GLProcGetter* getter, GLFunctions* functions,
                                                const GLInfo& info) {
  if (info.hasExtension("GL_EXT_multisampled_render_to_texture")) {
    functions->framebufferTexture2DMultisample =
        reinterpret_cast<GLFramebufferTexture2DMultisample*>(
            getter->getProcAddress("glFramebufferTexture2DMultisampleEXT"));
  } else if (info.hasExtension("GL_IMG_multisampled_render_to_texture")) {
    functions->framebufferTexture2DMultisample =
        reinterpret_cast<GLFramebufferTexture2DMultisample*>(
            getter->getProcAddress("glFramebufferTexture2DMultisampleIMG"));
  }
}

static void InitRenderbufferStorageMultisample(const GLProcGetter* getter, GLFunctions* functions,
                                               const GLInfo& info) {
  if (info.version >= GL_VER(3, 0)) {
    functions->renderbufferStorageMultisample = reinterpret_cast<GLRenderbufferStorageMultisample*>(
        getter->getProcAddress("glRenderbufferStorageMultisample"));
  } else if (info.hasExtension("GL_CHROMIUM_framebuffer_multisample")) {
    functions->renderbufferStorageMultisample = reinterpret_cast<GLRenderbufferStorageMultisample*>(
        getter->getProcAddress("glRenderbufferStorageMultisampleCHROMIUM"));
  } else if (info.hasExtension("GL_ANGLE_framebuffer_multisample")) {
    functions->renderbufferStorageMultisample = reinterpret_cast<GLRenderbufferStorageMultisample*>(
        getter->getProcAddress("glRenderbufferStorageMultisampleANGLE"));
  }
  if (info.hasExtension("GL_EXT_multisampled_render_to_texture")) {
    functions->renderbufferStorageMultisampleEXT =
        reinterpret_cast<GLRenderbufferStorageMultisampleEXT*>(
            getter->getProcAddress("glRenderbufferStorageMultisampleEXT"));
  }
  if (info.hasExtension("GL_IMG_multisampled_render_to_texture")) {
    functions->renderbufferStorageMultisampleEXT =
        reinterpret_cast<GLRenderbufferStorageMultisampleEXT*>(
            getter->getProcAddress("glRenderbufferStorageMultisampleIMG"));
  }
  if (info.hasExtension("GL_APPLE_framebuffer_multisample")) {
    functions->renderbufferStorageMultisampleAPPLE =
        reinterpret_cast<GLRenderbufferStorageMultisampleAPPLE*>(
            getter->getProcAddress("glRenderbufferStorageMultisampleAPPLE"));
  }
}

static void InitBlitFramebuffer(const GLProcGetter* getter, GLFunctions* functions,
                                const GLInfo& info) {
  if (info.version >= GL_VER(3, 0)) {
    functions->blitFramebuffer =
        reinterpret_cast<GLBlitFramebuffer*>(getter->getProcAddress("glBlitFramebuffer"));
  } else if (info.hasExtension("GL_CHROMIUM_framebuffer_multisample")) {
    functions->blitFramebuffer =
        reinterpret_cast<GLBlitFramebuffer*>(getter->getProcAddress("glBlitFramebufferCHROMIUM"));
  } else if (info.hasExtension("GL_ANGLE_framebuffer_blit")) {
    functions->blitFramebuffer =
        reinterpret_cast<GLBlitFramebuffer*>(getter->getProcAddress("glBlitFramebufferANGLE"));
  }
}

static void InitVertexArray(const GLProcGetter* getter, GLFunctions* functions,
                            const GLInfo& info) {
  if (info.version >= GL_VER(3, 0)) {
    functions->bindVertexArray =
        reinterpret_cast<GLBindVertexArray*>(getter->getProcAddress("glBindVertexArray"));
    functions->deleteVertexArrays =
        reinterpret_cast<GLDeleteVertexArrays*>(getter->getProcAddress("glDeleteVertexArrays"));
    functions->genVertexArrays =
        reinterpret_cast<GLGenVertexArrays*>(getter->getProcAddress("glGenVertexArrays"));
  } else if (info.hasExtension("GL_OES_vertex_array_object")) {
    functions->bindVertexArray =
        reinterpret_cast<GLBindVertexArray*>(getter->getProcAddress("glBindVertexArrayOES"));
    functions->deleteVertexArrays =
        reinterpret_cast<GLDeleteVertexArrays*>(getter->getProcAddress("glDeleteVertexArraysOES"));
    functions->genVertexArrays =
        reinterpret_cast<GLGenVertexArrays*>(getter->getProcAddress("glGenVertexArraysOES"));
  }
}

void GLAssembleGLESInterface(const GLProcGetter* getter, GLFunctions* functions,
                             const GLInfo& info) {
  if (info.hasExtension("GL_NV_texture_barrier")) {
    functions->textureBarrier =
        reinterpret_cast<GLTextureBarrier*>(getter->getProcAddress("glTextureBarrierNV"));
  }
  InitBlitFramebuffer(getter, functions, info);
  InitRenderbufferStorageMultisample(getter, functions, info);
  InitFramebufferTexture2DMultisample(getter, functions, info);
  InitVertexArray(getter, functions, info);
}
}  // namespace tgfx
