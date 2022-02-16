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
#include <cstring>

namespace tgfx {
#if !defined(GL_FUNCTION_TYPE)
#if defined(_WIN32) && !defined(_WIN32_WCE) && !defined(__SCITECH_SNAP__)
#define GL_FUNCTION_TYPE __stdcall
#else
#define GL_FUNCTION_TYPE
#endif
#endif

typedef intptr_t GLintptr;
typedef intptr_t GLsizeiptr;

extern "C" {

using GLActiveTexture = void GL_FUNCTION_TYPE(unsigned texture);
using GLAttachShader = void GL_FUNCTION_TYPE(unsigned program, unsigned shader);
using GLBindBuffer = void GL_FUNCTION_TYPE(unsigned target, unsigned buffer);
using GLBindVertexArray = void GL_FUNCTION_TYPE(unsigned vertexArray);
using GLBindFramebuffer = void GL_FUNCTION_TYPE(unsigned target, unsigned framebuffer);
using GLBindRenderbuffer = void GL_FUNCTION_TYPE(unsigned target, unsigned renderbuffer);
using GLBindTexture = void GL_FUNCTION_TYPE(unsigned target, unsigned texture);
using GLBlendEquation = void GL_FUNCTION_TYPE(unsigned mode);
using GLBlendEquationSeparate = void GL_FUNCTION_TYPE(unsigned modeRGB, unsigned modeAlpha);
using GLBlendFunc = void GL_FUNCTION_TYPE(unsigned sfactor, unsigned dfactor);
using GLBlendFuncSeparate = void GL_FUNCTION_TYPE(unsigned sfactorRGB, unsigned dfactorRGB,
                                                  unsigned sfactorAlpha, unsigned dfactorAlpha);
using GLBufferData = void GL_FUNCTION_TYPE(unsigned target, GLsizeiptr size, const void* data,
                                           unsigned usage);
using GLCheckFramebufferStatus = unsigned GL_FUNCTION_TYPE(unsigned target);
using GLClear = void GL_FUNCTION_TYPE(unsigned mask);
using GLClearColor = void GL_FUNCTION_TYPE(float red, float green, float blue, float alpha);
using GLCompileShader = void GL_FUNCTION_TYPE(unsigned shader);
using GLCopyTexSubImage2D = void GL_FUNCTION_TYPE(unsigned target, int level, int xoffset,
                                                  int yoffset, int x, int y, int width, int height);
using GLCreateProgram = unsigned GL_FUNCTION_TYPE();
using GLCreateShader = unsigned GL_FUNCTION_TYPE(unsigned type);
using GLDeleteBuffers = void GL_FUNCTION_TYPE(int n, const unsigned* buffers);
using GLDeleteFramebuffers = void GL_FUNCTION_TYPE(int n, const unsigned* framebuffers);
using GLDeleteProgram = void GL_FUNCTION_TYPE(unsigned program);
using GLDeleteRenderbuffers = void GL_FUNCTION_TYPE(int n, const unsigned* renderbuffers);
using GLDeleteShader = void GL_FUNCTION_TYPE(unsigned shader);
using GLDeleteTextures = void GL_FUNCTION_TYPE(int n, const unsigned* textures);
using GLDeleteVertexArrays = void GL_FUNCTION_TYPE(int n, const unsigned* vertexArrays);
using GLDepthMask = void GL_FUNCTION_TYPE(unsigned char flag);
using GLDisable = void GL_FUNCTION_TYPE(unsigned cap);
using GLDisableVertexAttribArray = void GL_FUNCTION_TYPE(unsigned index);
using GLDrawArrays = void GL_FUNCTION_TYPE(unsigned mode, int first, int count);
using GLDrawElements = void GL_FUNCTION_TYPE(unsigned mode, int count, unsigned type,
                                             const void* indices);
using GLEnable = void GL_FUNCTION_TYPE(unsigned cap);
using GLIsEnabled = unsigned char GL_FUNCTION_TYPE(unsigned cap);
using GLEnableVertexAttribArray = void GL_FUNCTION_TYPE(unsigned index);
using GLFinish = void GL_FUNCTION_TYPE();
using GLFlush = void GL_FUNCTION_TYPE();
using GLFramebufferRenderbuffer = void GL_FUNCTION_TYPE(unsigned target, unsigned attachment,
                                                        unsigned renderbuffertarget,
                                                        unsigned renderbuffer);
using GLFramebufferTexture2D = void GL_FUNCTION_TYPE(unsigned target, unsigned attachment,
                                                     unsigned textarget, unsigned texture,
                                                     int level);
using GLFramebufferTexture2DMultisample = void GL_FUNCTION_TYPE(unsigned target,
                                                                unsigned attachment,
                                                                unsigned textarget,
                                                                unsigned texture, int level,
                                                                int samples);
using GLGenBuffers = void GL_FUNCTION_TYPE(int n, unsigned* buffers);
using GLGenVertexArrays = void GL_FUNCTION_TYPE(int n, unsigned* vertexArrays);
using GLGenFramebuffers = void GL_FUNCTION_TYPE(int n, unsigned* framebuffers);
using GLGenRenderbuffers = void GL_FUNCTION_TYPE(int n, unsigned* renderbuffers);
using GLGenTextures = void GL_FUNCTION_TYPE(int n, unsigned* textures);
using GLGetError = unsigned GL_FUNCTION_TYPE();
using GLGetIntegerv = void GL_FUNCTION_TYPE(unsigned pname, int* params);
using GLGetBooleanv = void GL_FUNCTION_TYPE(unsigned pname, unsigned char* data);
using GLGetProgramInfoLog = void GL_FUNCTION_TYPE(unsigned program, int bufsize, int* length,
                                                  char* infolog);
using GLGetProgramiv = void GL_FUNCTION_TYPE(unsigned program, unsigned pname, int* params);
using GLGetRenderbufferParameteriv = void GL_FUNCTION_TYPE(unsigned target, unsigned pname,
                                                           int* params);
using GLGetShaderInfoLog = void GL_FUNCTION_TYPE(unsigned shader, int bufsize, int* length,
                                                 char* infolog);
using GLGetShaderiv = void GL_FUNCTION_TYPE(unsigned shader, unsigned pname, int* params);
using GLGetString = const unsigned char* GL_FUNCTION_TYPE(unsigned name);
using GLGetStringi = const unsigned char* GL_FUNCTION_TYPE(unsigned name, unsigned index);
using GLGetVertexAttribiv = void GL_FUNCTION_TYPE(unsigned index, unsigned pname, int* params);
using GLGetVertexAttribPointerv = void GL_FUNCTION_TYPE(unsigned index, unsigned pname,
                                                        void** pointer);
using GLGetAttribLocation = int GL_FUNCTION_TYPE(unsigned program, const char* name);
using GLGetUniformLocation = int GL_FUNCTION_TYPE(unsigned program, const char* name);
using GLLinkProgram = void GL_FUNCTION_TYPE(unsigned program);
using GLPixelStorei = void GL_FUNCTION_TYPE(unsigned pname, int param);
using GLReadPixels = void GL_FUNCTION_TYPE(int x, int y, int width, int height, unsigned format,
                                           unsigned type, void* pixels);
using GLRenderbufferStorage = void GL_FUNCTION_TYPE(unsigned target, unsigned internalformat,
                                                    int width, int height);
using GLRenderbufferStorageMultisample = void GL_FUNCTION_TYPE(unsigned target, int samples,
                                                               unsigned internalformat, int width,
                                                               int height);
using GLRenderbufferStorageMultisampleAPPLE = void GL_FUNCTION_TYPE(unsigned target, int samples,
                                                                    unsigned internalformat,
                                                                    int width, int height);
using GLRenderbufferStorageMultisampleEXT = void GL_FUNCTION_TYPE(unsigned target, int samples,
                                                                  unsigned internalformat,
                                                                  int width, int height);
using GLResolveMultisampleFramebuffer = void GL_FUNCTION_TYPE();
using GLBlitFramebuffer = void GL_FUNCTION_TYPE(int srcX0, int srcY0, int srcX1, int srcY1,
                                                int dstX0, int dstY0, int dstX1, int dstY1,
                                                unsigned mask, unsigned filter);
using GLScissor = void GL_FUNCTION_TYPE(int x, int y, int width, int height);
using GLShaderSource = void GL_FUNCTION_TYPE(unsigned shader, int count, const char* const* str,
                                             const int* length);
using GLTexImage2D = void GL_FUNCTION_TYPE(unsigned target, int level, int internalformat,
                                           int width, int height, int border, unsigned format,
                                           unsigned type, const void* pixels);
using GLTexParameteri = void GL_FUNCTION_TYPE(unsigned target, unsigned pname, int param);
using GLTexParameteriv = void GL_FUNCTION_TYPE(unsigned target, unsigned pname, const int* params);
using GLTexSubImage2D = void GL_FUNCTION_TYPE(unsigned target, int level, int xoffset, int yoffset,
                                              int width, int height, unsigned format, unsigned type,
                                              const void* pixels);
using GLUniform1f = void GL_FUNCTION_TYPE(int location, float v0);
using GLUniform1i = void GL_FUNCTION_TYPE(int location, int v0);
using GLUniform2f = void GL_FUNCTION_TYPE(int location, float v0, float v1);
using GLUniform3f = void GL_FUNCTION_TYPE(int location, float v0, float v1, float v2);
using GLUniform4fv = void GL_FUNCTION_TYPE(int location, int count, const float* v);
using GLUniformMatrix3fv = void GL_FUNCTION_TYPE(int location, int count, unsigned char transpose,
                                                 const float* value);
using GLUseProgram = void GL_FUNCTION_TYPE(unsigned program);
using GLVertexAttribPointer = void GL_FUNCTION_TYPE(unsigned indx, int size, unsigned type,
                                                    unsigned char normalized, int stride,
                                                    const void* ptr);
using GLViewport = void GL_FUNCTION_TYPE(int x, int y, int width, int height);

/* ARB_internalformat_query */
using GLGetInternalformativ = void GL_FUNCTION_TYPE(unsigned target, unsigned internalformat,
                                                    unsigned pname, int bufSize, int* params);
using GLTextureBarrier = void GL_FUNCTION_TYPE();
using GLGetShaderPrecisionFormat = void GL_FUNCTION_TYPE(unsigned shadertype,
                                                         unsigned precisiontype, int* range,
                                                         int* precision);
using GLFenceSync = void* GL_FUNCTION_TYPE(unsigned condition, unsigned flags);
using GLWaitSync = void GL_FUNCTION_TYPE(void* sync, unsigned flags, uint64_t timeout);
using GLDeleteSync = void GL_FUNCTION_TYPE(void* sync);
}  // extern "C"

// This is a lighter-weight std::function, trying to reduce code size and compile time by only
// supporting the exact use cases we require.
template <typename T>
class GLFunction;

template <typename R, typename... Args>
class GLFunction<R GL_FUNCTION_TYPE(Args...)> {
 public:
  GLFunction() = default;

  GLFunction(std::nullptr_t) {
  }

  template <typename Closure>
  GLFunction(Closure closure) : GLFunction() {
    static_assert(sizeof(Closure) <= sizeof(buffer), "buffer is too small");
    memcpy(buffer, &closure, sizeof(closure));
    call = [](const void* buf, Args... args) {
      auto closure = reinterpret_cast<const Closure*>(buf);
      return (*closure)(std::forward<Args>(args)...);
    };
  }

  R operator()(Args... args) const {
    return call(buffer, std::forward<Args>(args)...);
  }

  explicit operator bool() const {
    return call != nullptr;
  }

  void reset() {
    call = nullptr;
  }

 private:
  using Call = R(const void* buf, Args...);
  Call* call = nullptr;
  size_t buffer[4] = {};
};

}  // namespace tgfx