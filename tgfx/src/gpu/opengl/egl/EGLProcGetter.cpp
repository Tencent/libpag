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

#include "EGLProcGetter.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <cstring>

namespace pag {

static void* egl_get_gl_proc(void*, const char name[]) {
  // https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_get_all_proc_addresses.txt
  // eglGetProcAddress() is not guaranteed to support the querying of non-extension EGL functions.
#define M(X)                           \
  if (0 == strcmp(#X, name)) {         \
    return reinterpret_cast<void*>(X); \
  }
  M(eglGetCurrentDisplay)
  M(eglQueryString)
  M(glActiveTexture)
  M(glAttachShader)
  M(glBindAttribLocation)
  M(glBindBuffer)
  M(glBindFramebuffer)
  M(glBindRenderbuffer)
  M(glBindTexture)
  M(glBlendColor)
  M(glBlendEquation)
  M(glBlendFunc)
  M(glBufferData)
  M(glBufferSubData)
  M(glCheckFramebufferStatus)
  M(glClear)
  M(glClearColor)
  M(glClearStencil)
  M(glColorMask)
  M(glCompileShader)
  M(glCompressedTexImage2D)
  M(glCompressedTexSubImage2D)
  M(glCopyTexSubImage2D)
  M(glCreateProgram)
  M(glCreateShader)
  M(glCullFace)
  M(glDeleteBuffers)
  M(glDeleteFramebuffers)
  M(glDeleteProgram)
  M(glDeleteRenderbuffers)
  M(glDeleteShader)
  M(glDeleteTextures)
  M(glDepthMask)
  M(glDisable)
  M(glDisableVertexAttribArray)
  M(glDrawArrays)
  M(glDrawElements)
  M(glEnable)
  M(glEnableVertexAttribArray)
  M(glFinish)
  M(glFlush)
  M(glFramebufferRenderbuffer)
  M(glFramebufferTexture2D)
  M(glFrontFace)
  M(glGenBuffers)
  M(glGenFramebuffers)
  M(glGenRenderbuffers)
  M(glGenTextures)
  M(glGenerateMipmap)
  M(glGetBufferParameteriv)
  M(glGetError)
  M(glGetFramebufferAttachmentParameteriv)
  M(glGetIntegerv)
  M(glGetProgramInfoLog)
  M(glGetProgramiv)
  M(glGetRenderbufferParameteriv)
  M(glGetShaderInfoLog)
  M(glGetShaderPrecisionFormat)
  M(glGetShaderiv)
  M(glGetString)
  M(glGetUniformLocation)
  M(glIsTexture)
  M(glLineWidth)
  M(glLinkProgram)
  M(glPixelStorei)
  M(glReadPixels)
  M(glRenderbufferStorage)
  M(glScissor)
  M(glShaderSource)
  M(glStencilFunc)
  M(glStencilFuncSeparate)
  M(glStencilMask)
  M(glStencilMaskSeparate)
  M(glStencilOp)
  M(glStencilOpSeparate)
  M(glTexImage2D)
  M(glTexParameterf)
  M(glTexParameterfv)
  M(glTexParameteri)
  M(glTexParameteriv)
  M(glTexSubImage2D)
  M(glUniform1f)
  M(glUniform1fv)
  M(glUniform1i)
  M(glUniform1iv)
  M(glUniform2f)
  M(glUniform2fv)
  M(glUniform2i)
  M(glUniform2iv)
  M(glUniform3f)
  M(glUniform3fv)
  M(glUniform3i)
  M(glUniform3iv)
  M(glUniform4f)
  M(glUniform4fv)
  M(glUniform4i)
  M(glUniform4iv)
  M(glUniformMatrix2fv)
  M(glUniformMatrix3fv)
  M(glUniformMatrix4fv)
  M(glUseProgram)
  M(glVertexAttrib1f)
  M(glVertexAttrib2fv)
  M(glVertexAttrib3fv)
  M(glVertexAttrib4fv)
  M(glVertexAttribPointer)
  M(glViewport)
#undef M
  auto fun = eglGetProcAddress(name);
  return reinterpret_cast<void*>(fun);
}

void* EGLProcGetter::getProcAddress(const char* name) const {
  return egl_get_gl_proc(nullptr, name);
}
}  // namespace pag