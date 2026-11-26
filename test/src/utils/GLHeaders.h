/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdio>

#ifdef TGFX_USE_OPENGL

#if defined(PAG_USE_SWIFTSHADER) || defined(__EMSCRIPTEN__) || \
    (defined(_WIN32) && defined(PAG_USE_ANGLE))
// SwiftShader, Emscripten and the Windows ANGLE build all provide GLES 3 entry points directly.
#include <GLES3/gl3.h>
#elif defined(_WIN32)
// Windows ships only the OpenGL 1.1 headers. The tests need a few GL 3.0 framebuffer entry
// points, so declare them here and resolve them through wglGetProcAddress at first use.
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
// windows.h must precede GL/gl.h because GL/gl.h depends on the WINGDIAPI and APIENTRY macros.
#include <windows.h>
#include <GL/gl.h>
// clang-format on

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif

inline PROC GetGLProc(const char* name) {
  static HMODULE library = LoadLibraryA("opengl32.dll");
  if (library != nullptr) {
    auto address = GetProcAddress(library, name);
    if (address != nullptr) {
      return address;
    }
  }
  return wglGetProcAddress(name);
}

inline void ReportMissingGLProc(const char* name) {
  static const char* lastReported = nullptr;
  if (lastReported != name) {
    lastReported = name;
    fprintf(stderr, "[GLHeaders] %s is unavailable; the GL 3 framebuffer calls are incomplete.\n",
            name);
  }
}

inline void glGenFramebuffers(GLsizei n, GLuint* framebuffers) {
  using Proc = void(APIENTRY*)(GLsizei, GLuint*);
  auto proc = reinterpret_cast<Proc>(GetGLProc("glGenFramebuffers"));
  if (proc != nullptr) {
    proc(n, framebuffers);
  } else {
    ReportMissingGLProc("glGenFramebuffers");
  }
}

inline void glBindFramebuffer(GLenum target, GLuint framebuffer) {
  using Proc = void(APIENTRY*)(GLenum, GLuint);
  auto proc = reinterpret_cast<Proc>(GetGLProc("glBindFramebuffer"));
  if (proc != nullptr) {
    proc(target, framebuffer);
  } else {
    ReportMissingGLProc("glBindFramebuffer");
  }
}

inline void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
                                   GLuint texture, GLint level) {
  using Proc = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
  auto proc = reinterpret_cast<Proc>(GetGLProc("glFramebufferTexture2D"));
  if (proc != nullptr) {
    proc(target, attachment, textarget, texture, level);
  } else {
    ReportMissingGLProc("glFramebufferTexture2D");
  }
}

inline GLenum glCheckFramebufferStatus(GLenum target) {
  using Proc = GLenum(APIENTRY*)(GLenum);
  auto proc = reinterpret_cast<Proc>(GetGLProc("glCheckFramebufferStatus"));
  if (proc != nullptr) {
    return proc(target);
  }
  ReportMissingGLProc("glCheckFramebufferStatus");
  return 0;
}

inline void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  using Proc = void(APIENTRY*)(GLsizei, const GLuint*);
  auto proc = reinterpret_cast<Proc>(GetGLProc("glDeleteFramebuffers"));
  if (proc != nullptr) {
    proc(n, framebuffers);
  } else {
    ReportMissingGLProc("glDeleteFramebuffers");
  }
}
#else
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#endif

#endif  // TGFX_USE_OPENGL
