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

#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <utility>

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

using GLActiveTexture = void GL_FUNCTION_TYPE(unsigned texture);
using GLAttachShader = void GL_FUNCTION_TYPE(unsigned program, unsigned shader);
using GLBindAttribLocation = void GL_FUNCTION_TYPE(unsigned program, unsigned index,
                                                   const char* name);
using GLBindBuffer = void GL_FUNCTION_TYPE(unsigned target, unsigned buffer);
using GLBindVertexArray = void GL_FUNCTION_TYPE(unsigned vertexArray);
using GLBindFramebuffer = void GL_FUNCTION_TYPE(unsigned target, unsigned framebuffer);
using GLBindRenderbuffer = void GL_FUNCTION_TYPE(unsigned target, unsigned renderbuffer);
using GLBindTexture = void GL_FUNCTION_TYPE(unsigned target, unsigned texture);
using GLBlendColor = void GL_FUNCTION_TYPE(float red, float green, float blue, float alpha);
using GLBlendEquation = void GL_FUNCTION_TYPE(unsigned mode);
using GLBlendEquationSeparate = void GL_FUNCTION_TYPE(unsigned modeRGB, unsigned modeAlpha);
using GLBlendFunc = void GL_FUNCTION_TYPE(unsigned sfactor, unsigned dfactor);
using GLBlendFuncSeparate = void GL_FUNCTION_TYPE(unsigned sfactorRGB, unsigned dfactorRGB,
                                                  unsigned sfactorAlpha, unsigned dfactorAlpha);
using GLBufferData = void GL_FUNCTION_TYPE(unsigned target, GLsizeiptr size, const void* data,
                                           unsigned usage);
using GLBufferSubData = void GL_FUNCTION_TYPE(unsigned target, GLintptr offset, GLsizeiptr size,
                                              const void* data);
using GLCheckFramebufferStatus = unsigned GL_FUNCTION_TYPE(unsigned target);
using GLClear = void GL_FUNCTION_TYPE(unsigned mask);
using GLClearColor = void GL_FUNCTION_TYPE(float red, float green, float blue, float alpha);
using GLClearStencil = void GL_FUNCTION_TYPE(int s);
using GLColorMask = void GL_FUNCTION_TYPE(unsigned char red, unsigned char green,
                                          unsigned char blue, unsigned char alpha);
using GLCompileShader = void GL_FUNCTION_TYPE(unsigned shader);
using GLCompressedTexImage2D = void GL_FUNCTION_TYPE(unsigned target, int level,
                                                     unsigned internalformat, int width, int height,
                                                     int border, int imageSize, const void* data);
using GLCompressedTexSubImage2D = void GL_FUNCTION_TYPE(unsigned target, int level, int xoffset,
                                                        int yoffset, int width, int height,
                                                        unsigned format, int imageSize,
                                                        const void* data);
using GLCopyTexSubImage2D = void GL_FUNCTION_TYPE(unsigned target, int level, int xoffset,
                                                  int yoffset, int x, int y, int width, int height);
using GLCreateProgram = unsigned GL_FUNCTION_TYPE();
using GLCreateShader = unsigned GL_FUNCTION_TYPE(unsigned type);
using GLCullFace = void GL_FUNCTION_TYPE(unsigned mode);
using GLDeleteBuffers = void GL_FUNCTION_TYPE(int n, const unsigned* buffers);
using GLDeleteFramebuffers = void GL_FUNCTION_TYPE(int n, const unsigned* framebuffers);
using GLDeleteProgram = void GL_FUNCTION_TYPE(unsigned program);
using GLDeleteRenderbuffers = void GL_FUNCTION_TYPE(int n, const unsigned* renderbuffers);
using GLDeleteShader = void GL_FUNCTION_TYPE(unsigned shader);
using GLDeleteSync = void GL_FUNCTION_TYPE(void* sync);
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
using GLFenceSync = void* GL_FUNCTION_TYPE(unsigned condition, unsigned flags);
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
using GLFrontFace = void GL_FUNCTION_TYPE(unsigned mode);
using GLGenBuffers = void GL_FUNCTION_TYPE(int n, unsigned* buffers);
using GLGenVertexArrays = void GL_FUNCTION_TYPE(int n, unsigned* vertexArrays);
using GLGenFramebuffers = void GL_FUNCTION_TYPE(int n, unsigned* framebuffers);
using GLGenerateMipmap = void GL_FUNCTION_TYPE(unsigned target);
using GLGenRenderbuffers = void GL_FUNCTION_TYPE(int n, unsigned* renderbuffers);
using GLGenTextures = void GL_FUNCTION_TYPE(int n, unsigned* textures);
using GLGetBooleanv = void GL_FUNCTION_TYPE(unsigned pname, unsigned char* data);
using GLGetBufferParameteriv = void GL_FUNCTION_TYPE(unsigned target, unsigned pname, int* params);
using GLGetError = unsigned GL_FUNCTION_TYPE();
using GLGetFramebufferAttachmentParameteriv = void GL_FUNCTION_TYPE(unsigned target,
                                                                    unsigned attachment,
                                                                    unsigned pname, int* params);
using GLGetIntegerv = void GL_FUNCTION_TYPE(unsigned pname, int* params);
using GLGetInternalformativ = void GL_FUNCTION_TYPE(unsigned target, unsigned internalformat,
                                                    unsigned pname, int bufSize, int* params);
using GLGetProgramInfoLog = void GL_FUNCTION_TYPE(unsigned program, int bufsize, int* length,
                                                  char* infolog);
using GLGetProgramiv = void GL_FUNCTION_TYPE(unsigned program, unsigned pname, int* params);
using GLGetRenderbufferParameteriv = void GL_FUNCTION_TYPE(unsigned target, unsigned pname,
                                                           int* params);
using GLGetShaderInfoLog = void GL_FUNCTION_TYPE(unsigned shader, int bufsize, int* length,
                                                 char* infolog);
using GLGetShaderiv = void GL_FUNCTION_TYPE(unsigned shader, unsigned pname, int* params);
using GLGetShaderPrecisionFormat = void GL_FUNCTION_TYPE(unsigned shadertype,
                                                         unsigned precisiontype, int* range,
                                                         int* precision);
using GLGetString = const unsigned char* GL_FUNCTION_TYPE(unsigned name);
using GLGetStringi = const unsigned char* GL_FUNCTION_TYPE(unsigned name, unsigned index);
using GLGetVertexAttribiv = void GL_FUNCTION_TYPE(unsigned index, unsigned pname, int* params);
using GLGetVertexAttribPointerv = void GL_FUNCTION_TYPE(unsigned index, unsigned pname,
                                                        void** pointer);
using GLGetAttribLocation = int GL_FUNCTION_TYPE(unsigned program, const char* name);
using GLGetUniformLocation = int GL_FUNCTION_TYPE(unsigned program, const char* name);
using GLIsTexture = unsigned char GL_FUNCTION_TYPE(unsigned texture);
using GLLineWidth = void GL_FUNCTION_TYPE(float width);
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
using GLStencilFunc = void GL_FUNCTION_TYPE(unsigned func, int ref, unsigned mask);
using GLStencilFuncSeparate = void GL_FUNCTION_TYPE(unsigned face, unsigned func, int ref,
                                                    unsigned mask);
using GLStencilMask = void GL_FUNCTION_TYPE(unsigned mask);
using GLStencilMaskSeparate = void GL_FUNCTION_TYPE(unsigned face, unsigned mask);
using GLStencilOp = void GL_FUNCTION_TYPE(unsigned fail, unsigned zfail, unsigned zpass);
using GLStencilOpSeparate = void GL_FUNCTION_TYPE(unsigned face, unsigned fail, unsigned zfail,
                                                  unsigned zpass);
using GLTexImage2D = void GL_FUNCTION_TYPE(unsigned target, int level, int internalformat,
                                           int width, int height, int border, unsigned format,
                                           unsigned type, const void* pixels);
using GLTexParameterf = void GL_FUNCTION_TYPE(unsigned target, unsigned pname, float param);
using GLTexParameterfv = void GL_FUNCTION_TYPE(unsigned target, unsigned pname,
                                               const float* params);
using GLTexParameteri = void GL_FUNCTION_TYPE(unsigned target, unsigned pname, int param);
using GLTexParameteriv = void GL_FUNCTION_TYPE(unsigned target, unsigned pname, const int* params);
using GLTexSubImage2D = void GL_FUNCTION_TYPE(unsigned target, int level, int xoffset, int yoffset,
                                              int width, int height, unsigned format, unsigned type,
                                              const void* pixels);
using GLTextureBarrier = void GL_FUNCTION_TYPE();
using GLUniform1f = void GL_FUNCTION_TYPE(int location, float v0);
using GLUniform1i = void GL_FUNCTION_TYPE(int location, int v0);
using GLUniform1fv = void GL_FUNCTION_TYPE(int location, int count, const float* v);
using GLUniform1iv = void GL_FUNCTION_TYPE(int location, int count, const int* v);
using GLUniform2f = void GL_FUNCTION_TYPE(int location, float v0, float v1);
using GLUniform2i = void GL_FUNCTION_TYPE(int location, int v0, int v1);
using GLUniform2fv = void GL_FUNCTION_TYPE(int location, int count, const float* v);
using GLUniform2iv = void GL_FUNCTION_TYPE(int location, int count, const int* v);
using GLUniform3f = void GL_FUNCTION_TYPE(int location, float v0, float v1, float v2);
using GLUniform3i = void GL_FUNCTION_TYPE(int location, int v0, int v1, int v2);
using GLUniform3fv = void GL_FUNCTION_TYPE(int location, int count, const float* v);
using GLUniform3iv = void GL_FUNCTION_TYPE(int location, int count, const int* v);
using GLUniform4f = void GL_FUNCTION_TYPE(int location, float v0, float v1, float v2, float v3);
using GLUniform4i = void GL_FUNCTION_TYPE(int location, int v0, int v1, int v2, int v3);
using GLUniform4fv = void GL_FUNCTION_TYPE(int location, int count, const float* v);
using GLUniform4iv = void GL_FUNCTION_TYPE(int location, int count, const int* v);
using GLUniformMatrix2fv = void GL_FUNCTION_TYPE(int location, int count, unsigned char transpose,
                                                 const float* value);
using GLUniformMatrix3fv = void GL_FUNCTION_TYPE(int location, int count, unsigned char transpose,
                                                 const float* value);
using GLUniformMatrix4fv = void GL_FUNCTION_TYPE(int location, int count, unsigned char transpose,
                                                 const float* value);
using GLUseProgram = void GL_FUNCTION_TYPE(unsigned program);
using GLVertexAttrib1f = void GL_FUNCTION_TYPE(unsigned indx, float value);
using GLVertexAttrib2fv = void GL_FUNCTION_TYPE(unsigned indx, const float* values);
using GLVertexAttrib3fv = void GL_FUNCTION_TYPE(unsigned indx, const float* values);
using GLVertexAttrib4fv = void GL_FUNCTION_TYPE(unsigned indx, const float* values);
using GLVertexAttribPointer = void GL_FUNCTION_TYPE(unsigned indx, int size, unsigned type,
                                                    unsigned char normalized, int stride,
                                                    const void* ptr);
using GLViewport = void GL_FUNCTION_TYPE(int x, int y, int width, int height);
using GLWaitSync = void GL_FUNCTION_TYPE(void* sync, unsigned flags, uint64_t timeout);

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

class Context;

/**
 * GLFunctions holds the OpenGL function pointers.
 */
class GLFunctions {
 public:
  /**
   * Returns the GLFunctions pointer associated with the specified context.
   */
  static const GLFunctions* Get(const Context* context);

  GLFunction<GLActiveTexture> activeTexture = nullptr;
  GLFunction<GLAttachShader> attachShader = nullptr;
  GLFunction<GLBindAttribLocation> bindAttribLocation = nullptr;
  GLFunction<GLBindBuffer> bindBuffer = nullptr;
  GLFunction<GLBindFramebuffer> bindFramebuffer = nullptr;
  GLFunction<GLBindRenderbuffer> bindRenderbuffer = nullptr;
  GLFunction<GLBindTexture> bindTexture = nullptr;
  GLFunction<GLBindVertexArray> bindVertexArray = nullptr;
  GLFunction<GLBlendColor> blendColor = nullptr;
  GLFunction<GLBlendEquation> blendEquation = nullptr;
  GLFunction<GLBlendEquationSeparate> blendEquationSeparate = nullptr;
  GLFunction<GLBlendFunc> blendFunc = nullptr;
  GLFunction<GLBlendFuncSeparate> blendFuncSeparate = nullptr;
  GLFunction<GLBufferData> bufferData = nullptr;
  GLFunction<GLBufferSubData> bufferSubData = nullptr;
  GLFunction<GLCheckFramebufferStatus> checkFramebufferStatus = nullptr;
  GLFunction<GLClear> clear = nullptr;
  GLFunction<GLClearColor> clearColor = nullptr;
  GLFunction<GLClearStencil> clearStencil = nullptr;
  GLFunction<GLColorMask> colorMask = nullptr;
  GLFunction<GLCompileShader> compileShader = nullptr;
  GLFunction<GLCompressedTexImage2D> compressedTexImage2D = nullptr;
  GLFunction<GLCompressedTexSubImage2D> compressedTexSubImage2D = nullptr;
  GLFunction<GLCopyTexSubImage2D> copyTexSubImage2D = nullptr;
  GLFunction<GLCreateProgram> createProgram = nullptr;
  GLFunction<GLCreateShader> createShader = nullptr;
  GLFunction<GLCullFace> cullFace = nullptr;
  GLFunction<GLDeleteBuffers> deleteBuffers = nullptr;
  GLFunction<GLDeleteFramebuffers> deleteFramebuffers = nullptr;
  GLFunction<GLDeleteProgram> deleteProgram = nullptr;
  GLFunction<GLDeleteRenderbuffers> deleteRenderbuffers = nullptr;
  GLFunction<GLDeleteShader> deleteShader = nullptr;
  GLFunction<GLDeleteSync> deleteSync = nullptr;
  GLFunction<GLDeleteTextures> deleteTextures = nullptr;
  GLFunction<GLDeleteVertexArrays> deleteVertexArrays = nullptr;
  GLFunction<GLDepthMask> depthMask = nullptr;
  GLFunction<GLDisable> disable = nullptr;
  GLFunction<GLDisableVertexAttribArray> disableVertexAttribArray = nullptr;
  GLFunction<GLDrawArrays> drawArrays = nullptr;
  GLFunction<GLDrawElements> drawElements = nullptr;
  GLFunction<GLEnable> enable = nullptr;
  GLFunction<GLIsEnabled> isEnabled = nullptr;
  GLFunction<GLEnableVertexAttribArray> enableVertexAttribArray = nullptr;
  GLFunction<GLFenceSync> fenceSync = nullptr;
  GLFunction<GLFinish> finish = nullptr;
  GLFunction<GLFlush> flush = nullptr;
  GLFunction<GLFramebufferRenderbuffer> framebufferRenderbuffer = nullptr;
  GLFunction<GLFramebufferTexture2D> framebufferTexture2D = nullptr;
  GLFunction<GLFramebufferTexture2DMultisample> framebufferTexture2DMultisample = nullptr;
  GLFunction<GLFrontFace> frontFace = nullptr;
  GLFunction<GLGenBuffers> genBuffers = nullptr;
  GLFunction<GLGenFramebuffers> genFramebuffers = nullptr;
  GLFunction<GLGenerateMipmap> generateMipmap = nullptr;
  GLFunction<GLGenRenderbuffers> genRenderbuffers = nullptr;
  GLFunction<GLGenTextures> genTextures = nullptr;
  GLFunction<GLGenVertexArrays> genVertexArrays = nullptr;
  GLFunction<GLGetBufferParameteriv> getBufferParameteriv = nullptr;
  GLFunction<GLGetError> getError = nullptr;
  GLFunction<GLGetFramebufferAttachmentParameteriv> getFramebufferAttachmentParameteriv = nullptr;
  GLFunction<GLGetIntegerv> getIntegerv = nullptr;
  GLFunction<GLGetInternalformativ> getInternalformativ = nullptr;
  GLFunction<GLGetBooleanv> getBooleanv = nullptr;
  GLFunction<GLGetProgramInfoLog> getProgramInfoLog = nullptr;
  GLFunction<GLGetProgramiv> getProgramiv = nullptr;
  GLFunction<GLGetRenderbufferParameteriv> getRenderbufferParameteriv = nullptr;
  GLFunction<GLGetShaderInfoLog> getShaderInfoLog = nullptr;
  GLFunction<GLGetShaderiv> getShaderiv = nullptr;
  GLFunction<GLGetShaderPrecisionFormat> getShaderPrecisionFormat = nullptr;
  GLFunction<GLGetString> getString = nullptr;
  GLFunction<GLGetStringi> getStringi = nullptr;
  GLFunction<GLGetVertexAttribiv> getVertexAttribiv = nullptr;
  GLFunction<GLGetVertexAttribPointerv> getVertexAttribPointerv = nullptr;
  GLFunction<GLGetAttribLocation> getAttribLocation = nullptr;
  GLFunction<GLGetUniformLocation> getUniformLocation = nullptr;
  GLFunction<GLIsTexture> isTexture = nullptr;
  GLFunction<GLLineWidth> lineWidth = nullptr;
  GLFunction<GLLinkProgram> linkProgram = nullptr;
  GLFunction<GLPixelStorei> pixelStorei = nullptr;
  GLFunction<GLReadPixels> readPixels = nullptr;
  GLFunction<GLRenderbufferStorage> renderbufferStorage = nullptr;
  GLFunction<GLRenderbufferStorageMultisample> renderbufferStorageMultisample = nullptr;
  GLFunction<GLRenderbufferStorageMultisampleAPPLE> renderbufferStorageMultisampleAPPLE = nullptr;
  GLFunction<GLRenderbufferStorageMultisampleEXT> renderbufferStorageMultisampleEXT = nullptr;
  GLFunction<GLResolveMultisampleFramebuffer> resolveMultisampleFramebuffer = nullptr;
  GLFunction<GLBlitFramebuffer> blitFramebuffer = nullptr;
  GLFunction<GLScissor> scissor = nullptr;
  GLFunction<GLShaderSource> shaderSource = nullptr;
  GLFunction<GLStencilFunc> stencilFunc = nullptr;
  GLFunction<GLStencilFuncSeparate> stencilFuncSeparate = nullptr;
  GLFunction<GLStencilMask> stencilMask = nullptr;
  GLFunction<GLStencilMaskSeparate> stencilMaskSeparate = nullptr;
  GLFunction<GLStencilOp> stencilOp = nullptr;
  GLFunction<GLStencilOpSeparate> stencilOpSeparate = nullptr;
  GLFunction<GLTexImage2D> texImage2D = nullptr;
  GLFunction<GLTexParameterf> texParameterf = nullptr;
  GLFunction<GLTexParameterfv> texParameterfv = nullptr;
  GLFunction<GLTexParameteri> texParameteri = nullptr;
  GLFunction<GLTexParameteriv> texParameteriv = nullptr;
  GLFunction<GLTexSubImage2D> texSubImage2D = nullptr;
  GLFunction<GLTextureBarrier> textureBarrier = nullptr;
  GLFunction<GLUniform1f> uniform1f = nullptr;
  GLFunction<GLUniform1i> uniform1i = nullptr;
  GLFunction<GLUniform1fv> uniform1fv = nullptr;
  GLFunction<GLUniform1iv> uniform1iv = nullptr;
  GLFunction<GLUniform2f> uniform2f = nullptr;
  GLFunction<GLUniform2i> uniform2i = nullptr;
  GLFunction<GLUniform2fv> uniform2fv = nullptr;
  GLFunction<GLUniform2iv> uniform2iv = nullptr;
  GLFunction<GLUniform3f> uniform3f = nullptr;
  GLFunction<GLUniform3i> uniform3i = nullptr;
  GLFunction<GLUniform3fv> uniform3fv = nullptr;
  GLFunction<GLUniform3iv> uniform3iv = nullptr;
  GLFunction<GLUniform4f> uniform4f = nullptr;
  GLFunction<GLUniform4i> uniform4i = nullptr;
  GLFunction<GLUniform4fv> uniform4fv = nullptr;
  GLFunction<GLUniform4iv> uniform4iv = nullptr;
  GLFunction<GLUniformMatrix2fv> uniformMatrix2fv = nullptr;
  GLFunction<GLUniformMatrix3fv> uniformMatrix3fv = nullptr;
  GLFunction<GLUniformMatrix4fv> uniformMatrix4fv = nullptr;
  GLFunction<GLUseProgram> useProgram = nullptr;
  GLFunction<GLVertexAttrib1f> vertexAttrib1f = nullptr;
  GLFunction<GLVertexAttrib2fv> vertexAttrib2fv = nullptr;
  GLFunction<GLVertexAttrib3fv> vertexAttrib3fv = nullptr;
  GLFunction<GLVertexAttrib4fv> vertexAttrib4fv = nullptr;
  GLFunction<GLVertexAttribPointer> vertexAttribPointer = nullptr;
  GLFunction<GLViewport> viewport = nullptr;
  GLFunction<GLWaitSync> waitSync = nullptr;
};

}  // namespace tgfx