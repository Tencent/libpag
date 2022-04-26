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
#include "tgfx/gpu/opengl/GLDefines.h"

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

  GLActiveTexture* activeTexture = nullptr;
  GLAttachShader* attachShader = nullptr;
  GLBindAttribLocation* bindAttribLocation = nullptr;
  GLBindBuffer* bindBuffer = nullptr;
  GLBindFramebuffer* bindFramebuffer = nullptr;
  GLBindRenderbuffer* bindRenderbuffer = nullptr;
  GLBindTexture* bindTexture = nullptr;
  GLBindVertexArray* bindVertexArray = nullptr;
  GLBlendColor* blendColor = nullptr;
  GLBlendEquation* blendEquation = nullptr;
  GLBlendEquationSeparate* blendEquationSeparate = nullptr;
  GLBlendFunc* blendFunc = nullptr;
  GLBlendFuncSeparate* blendFuncSeparate = nullptr;
  GLBufferData* bufferData = nullptr;
  GLBufferSubData* bufferSubData = nullptr;
  GLCheckFramebufferStatus* checkFramebufferStatus = nullptr;
  GLClear* clear = nullptr;
  GLClearColor* clearColor = nullptr;
  GLClearStencil* clearStencil = nullptr;
  GLColorMask* colorMask = nullptr;
  GLCompileShader* compileShader = nullptr;
  GLCompressedTexImage2D* compressedTexImage2D = nullptr;
  GLCompressedTexSubImage2D* compressedTexSubImage2D = nullptr;
  GLCopyTexSubImage2D* copyTexSubImage2D = nullptr;
  GLCreateProgram* createProgram = nullptr;
  GLCreateShader* createShader = nullptr;
  GLCullFace* cullFace = nullptr;
  GLDeleteBuffers* deleteBuffers = nullptr;
  GLDeleteFramebuffers* deleteFramebuffers = nullptr;
  GLDeleteProgram* deleteProgram = nullptr;
  GLDeleteRenderbuffers* deleteRenderbuffers = nullptr;
  GLDeleteShader* deleteShader = nullptr;
  GLDeleteSync* deleteSync = nullptr;
  GLDeleteTextures* deleteTextures = nullptr;
  GLDeleteVertexArrays* deleteVertexArrays = nullptr;
  GLDepthMask* depthMask = nullptr;
  GLDisable* disable = nullptr;
  GLDisableVertexAttribArray* disableVertexAttribArray = nullptr;
  GLDrawArrays* drawArrays = nullptr;
  GLDrawElements* drawElements = nullptr;
  GLEnable* enable = nullptr;
  GLIsEnabled* isEnabled = nullptr;
  GLEnableVertexAttribArray* enableVertexAttribArray = nullptr;
  GLFenceSync* fenceSync = nullptr;
  GLFinish* finish = nullptr;
  GLFlush* flush = nullptr;
  GLFramebufferRenderbuffer* framebufferRenderbuffer = nullptr;
  GLFramebufferTexture2D* framebufferTexture2D = nullptr;
  GLFramebufferTexture2DMultisample* framebufferTexture2DMultisample = nullptr;
  GLFrontFace* frontFace = nullptr;
  GLGenBuffers* genBuffers = nullptr;
  GLGenFramebuffers* genFramebuffers = nullptr;
  GLGenerateMipmap* generateMipmap = nullptr;
  GLGenRenderbuffers* genRenderbuffers = nullptr;
  GLGenTextures* genTextures = nullptr;
  GLGenVertexArrays* genVertexArrays = nullptr;
  GLGetBufferParameteriv* getBufferParameteriv = nullptr;
  GLGetError* getError = nullptr;
  GLGetFramebufferAttachmentParameteriv* getFramebufferAttachmentParameteriv = nullptr;
  GLGetIntegerv* getIntegerv = nullptr;
  GLGetInternalformativ* getInternalformativ = nullptr;
  GLGetBooleanv* getBooleanv = nullptr;
  GLGetProgramInfoLog* getProgramInfoLog = nullptr;
  GLGetProgramiv* getProgramiv = nullptr;
  GLGetRenderbufferParameteriv* getRenderbufferParameteriv = nullptr;
  GLGetShaderInfoLog* getShaderInfoLog = nullptr;
  GLGetShaderiv* getShaderiv = nullptr;
  GLGetShaderPrecisionFormat* getShaderPrecisionFormat = nullptr;
  GLGetString* getString = nullptr;
  GLGetStringi* getStringi = nullptr;
  GLGetVertexAttribiv* getVertexAttribiv = nullptr;
  GLGetVertexAttribPointerv* getVertexAttribPointerv = nullptr;
  GLGetAttribLocation* getAttribLocation = nullptr;
  GLGetUniformLocation* getUniformLocation = nullptr;
  GLIsTexture* isTexture = nullptr;
  GLLineWidth* lineWidth = nullptr;
  GLLinkProgram* linkProgram = nullptr;
  GLPixelStorei* pixelStorei = nullptr;
  GLReadPixels* readPixels = nullptr;
  GLRenderbufferStorage* renderbufferStorage = nullptr;
  GLRenderbufferStorageMultisample* renderbufferStorageMultisample = nullptr;
  GLRenderbufferStorageMultisampleAPPLE* renderbufferStorageMultisampleAPPLE = nullptr;
  GLRenderbufferStorageMultisampleEXT* renderbufferStorageMultisampleEXT = nullptr;
  GLResolveMultisampleFramebuffer* resolveMultisampleFramebuffer = nullptr;
  GLBlitFramebuffer* blitFramebuffer = nullptr;
  GLScissor* scissor = nullptr;
  GLShaderSource* shaderSource = nullptr;
  GLStencilFunc* stencilFunc = nullptr;
  GLStencilFuncSeparate* stencilFuncSeparate = nullptr;
  GLStencilMask* stencilMask = nullptr;
  GLStencilMaskSeparate* stencilMaskSeparate = nullptr;
  GLStencilOp* stencilOp = nullptr;
  GLStencilOpSeparate* stencilOpSeparate = nullptr;
  GLTexImage2D* texImage2D = nullptr;
  GLTexParameterf* texParameterf = nullptr;
  GLTexParameterfv* texParameterfv = nullptr;
  GLTexParameteri* texParameteri = nullptr;
  GLTexParameteriv* texParameteriv = nullptr;
  GLTexSubImage2D* texSubImage2D = nullptr;
  GLTextureBarrier* textureBarrier = nullptr;
  GLUniform1f* uniform1f = nullptr;
  GLUniform1i* uniform1i = nullptr;
  GLUniform1fv* uniform1fv = nullptr;
  GLUniform1iv* uniform1iv = nullptr;
  GLUniform2f* uniform2f = nullptr;
  GLUniform2i* uniform2i = nullptr;
  GLUniform2fv* uniform2fv = nullptr;
  GLUniform2iv* uniform2iv = nullptr;
  GLUniform3f* uniform3f = nullptr;
  GLUniform3i* uniform3i = nullptr;
  GLUniform3fv* uniform3fv = nullptr;
  GLUniform3iv* uniform3iv = nullptr;
  GLUniform4f* uniform4f = nullptr;
  GLUniform4i* uniform4i = nullptr;
  GLUniform4fv* uniform4fv = nullptr;
  GLUniform4iv* uniform4iv = nullptr;
  GLUniformMatrix2fv* uniformMatrix2fv = nullptr;
  GLUniformMatrix3fv* uniformMatrix3fv = nullptr;
  GLUniformMatrix4fv* uniformMatrix4fv = nullptr;
  GLUseProgram* useProgram = nullptr;
  GLVertexAttrib1f* vertexAttrib1f = nullptr;
  GLVertexAttrib2fv* vertexAttrib2fv = nullptr;
  GLVertexAttrib3fv* vertexAttrib3fv = nullptr;
  GLVertexAttrib4fv* vertexAttrib4fv = nullptr;
  GLVertexAttribPointer* vertexAttribPointer = nullptr;
  GLViewport* viewport = nullptr;
  GLWaitSync* waitSync = nullptr;
};

}  // namespace tgfx