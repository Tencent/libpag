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

namespace pag {
static void InitFramebufferTexture2DMultisample(const GLProcGetter* getter, GLInterface* interface,
                                                const GLInfo& info) {
  if (info.hasExtension("GL_EXT_multisampled_render_to_texture")) {
    interface->framebufferTexture2DMultisample =
        reinterpret_cast<GLFramebufferTexture2DMultisample*>(
            getter->getProcAddress("glFramebufferTexture2DMultisampleEXT"));
  } else if (info.hasExtension("GL_IMG_multisampled_render_to_texture")) {
    interface->framebufferTexture2DMultisample =
        reinterpret_cast<GLFramebufferTexture2DMultisample*>(
            getter->getProcAddress("glFramebufferTexture2DMultisampleIMG"));
  }
}

static void InitRenderbufferStorageMultisample(const GLProcGetter* getter, GLInterface* interface,
                                               const GLInfo& info) {
  if (info.version >= GL_VER(3, 0)) {
    interface->renderbufferStorageMultisample = reinterpret_cast<GLRenderbufferStorageMultisample*>(
        getter->getProcAddress("glRenderbufferStorageMultisample"));
  } else if (info.hasExtension("GL_CHROMIUM_framebuffer_multisample")) {
    interface->renderbufferStorageMultisample = reinterpret_cast<GLRenderbufferStorageMultisample*>(
        getter->getProcAddress("glRenderbufferStorageMultisampleCHROMIUM"));
  } else if (info.hasExtension("GL_ANGLE_framebuffer_multisample")) {
    interface->renderbufferStorageMultisample = reinterpret_cast<GLRenderbufferStorageMultisample*>(
        getter->getProcAddress("glRenderbufferStorageMultisampleANGLE"));
  }
  if (info.hasExtension("GL_EXT_multisampled_render_to_texture")) {
    interface->renderbufferStorageMultisampleEXT =
        reinterpret_cast<GLRenderbufferStorageMultisampleEXT*>(
            getter->getProcAddress("glRenderbufferStorageMultisampleEXT"));
  }
  if (info.hasExtension("GL_IMG_multisampled_render_to_texture")) {
    interface->renderbufferStorageMultisampleEXT =
        reinterpret_cast<GLRenderbufferStorageMultisampleEXT*>(
            getter->getProcAddress("glRenderbufferStorageMultisampleIMG"));
  }
  if (info.hasExtension("GL_APPLE_framebuffer_multisample")) {
    interface->renderbufferStorageMultisampleAPPLE =
        reinterpret_cast<GLRenderbufferStorageMultisampleAPPLE*>(
            getter->getProcAddress("glRenderbufferStorageMultisampleAPPLE"));
  }
}

static void InitBlitFramebuffer(const GLProcGetter* getter, GLInterface* interface,
                                const GLInfo& info) {
  if (info.version >= GL_VER(3, 0)) {
    interface->blitFramebuffer =
        reinterpret_cast<GLBlitFramebuffer*>(getter->getProcAddress("glBlitFramebuffer"));
  } else if (info.hasExtension("GL_CHROMIUM_framebuffer_multisample")) {
    interface->blitFramebuffer =
        reinterpret_cast<GLBlitFramebuffer*>(getter->getProcAddress("glBlitFramebufferCHROMIUM"));
  } else if (info.hasExtension("GL_ANGLE_framebuffer_blit")) {
    interface->blitFramebuffer =
        reinterpret_cast<GLBlitFramebuffer*>(getter->getProcAddress("glBlitFramebufferANGLE"));
  }
}

static void InitVertexArray(const GLProcGetter* getter, GLInterface* interface,
                            const GLInfo& info) {
  if (info.version >= GL_VER(3, 0)) {
    interface->bindVertexArray =
        reinterpret_cast<GLBindVertexArray*>(getter->getProcAddress("glBindVertexArray"));
    interface->deleteVertexArrays =
        reinterpret_cast<GLDeleteVertexArrays*>(getter->getProcAddress("glDeleteVertexArrays"));
    interface->genVertexArrays =
        reinterpret_cast<GLGenVertexArrays*>(getter->getProcAddress("glGenVertexArrays"));
  } else if (info.hasExtension("GL_OES_vertex_array_object")) {
    interface->bindVertexArray =
        reinterpret_cast<GLBindVertexArray*>(getter->getProcAddress("glBindVertexArrayOES"));
    interface->deleteVertexArrays =
        reinterpret_cast<GLDeleteVertexArrays*>(getter->getProcAddress("glDeleteVertexArraysOES"));
    interface->genVertexArrays =
        reinterpret_cast<GLGenVertexArrays*>(getter->getProcAddress("glGenVertexArraysOES"));
  }
}

void GLAssembleGLESInterface(const GLProcGetter* getter, GLInterface* interface,
                             const GLInfo& info) {
  interface->checkFramebufferStatus = reinterpret_cast<GLCheckFramebufferStatus*>(
      getter->getProcAddress("glCheckFramebufferStatus"));
  interface->getError = reinterpret_cast<GLGetError*>(getter->getProcAddress("glGetError"));
  if (info.hasExtension("GL_NV_texture_barrier")) {
    interface->textureBarrier =
        reinterpret_cast<GLTextureBarrier*>(getter->getProcAddress("glTextureBarrierNV"));
  }
  InitBlitFramebuffer(getter, interface, info);
  InitRenderbufferStorageMultisample(getter, interface, info);
  InitFramebufferTexture2DMultisample(getter, interface, info);
  InitVertexArray(getter, interface, info);
}
}  // namespace pag
