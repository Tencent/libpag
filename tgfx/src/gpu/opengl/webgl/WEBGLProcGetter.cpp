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

#include "WEBGLProcGetter.h"

#include <emscripten/html5_webgl.h>
#include <GLES2/gl2.h>
#include <GLES3/gl3.h>
#include <cstring>

namespace pag {
void* WEBGLProcGetter::getProcAddress(const char* name) const {
#define N(X)                   \
  if (0 == strcmp(#X, name)) { \
    return reinterpret_cast<void*>(X);           \
  }
  N(glActiveTexture)
  N(glAttachShader)
  N(glIsEnabled)
  N(glBindAttribLocation)
  N(glBindBuffer)
  N(glBindFramebuffer)
  N(glBindRenderbuffer)
  N(glBindTexture)
  N(glBlendColor)
  N(glBlendEquation)
  N(glBlendFunc)
  N(glBufferData)
  N(glBufferSubData)
  N(glCheckFramebufferStatus)
  N(glClear)
  N(glClearColor)
  N(glClearStencil)
  N(glColorMask)
  N(glCompileShader)
  N(glCompressedTexImage2D)
  N(glCompressedTexSubImage2D)
  N(glCopyTexSubImage2D)
  N(glCreateProgram)
  N(glCreateShader)
  N(glCullFace)
  N(glDeleteBuffers)
  N(glDeleteFramebuffers)
  N(glDeleteProgram)
  N(glDeleteRenderbuffers)
  N(glDeleteShader)
  N(glDeleteTextures)
  N(glDepthMask)
  N(glDisable)
  N(glDisableVertexAttribArray)
  N(glDrawArrays)
  N(glDrawElements)
  N(glEnable)
  N(glEnableVertexAttribArray)
  N(glFinish)
  N(glFlush)
  N(glFramebufferRenderbuffer)
  N(glFramebufferTexture2D)
  N(glFrontFace)
  N(glGenBuffers)
  N(glGenFramebuffers)
  N(glGenRenderbuffers)
  N(glGenTextures)
  N(glGenerateMipmap)
  N(glGetBufferParameteriv)
  N(glGetError)
  N(glGetFramebufferAttachmentParameteriv)
  N(glGetIntegerv)
  N(glGetProgramInfoLog)
  N(glGetProgramiv)
  N(glGetRenderbufferParameteriv)
  N(glGetShaderInfoLog)
  N(glGetShaderPrecisionFormat)
  N(glGetShaderiv)
  N(glGetString)
  N(glGetUniformLocation)
  N(glIsTexture)
  N(glLineWidth)
  N(glLinkProgram)
  N(glPixelStorei)
  N(glReadPixels)
  N(glRenderbufferStorage)
  N(glScissor)
  N(glShaderSource)
  N(glStencilFunc)
  N(glStencilFuncSeparate)
  N(glStencilMask)
  N(glStencilMaskSeparate)
  N(glStencilOp)
  N(glStencilOpSeparate)
  N(glTexImage2D)
  N(glTexParameterf)
  N(glTexParameterfv)
  N(glTexParameteri)
  N(glTexParameteriv)
  N(glTexSubImage2D)
  N(glUniform1f)
  N(glUniform1fv)
  N(glUniform1i)
  N(glUniform1iv)
  N(glUniform2f)
  N(glUniform2fv)
  N(glUniform2i)
  N(glUniform2iv)
  N(glUniform3f)
  N(glUniform3fv)
  N(glUniform3i)
  N(glUniform3iv)
  N(glUniform4f)
  N(glUniform4fv)
  N(glUniform4i)
  N(glUniform4iv)
  N(glUniformMatrix2fv)
  N(glUniformMatrix3fv)
  N(glUniformMatrix4fv)
  N(glUseProgram)
  N(glVertexAttrib1f)
  N(glVertexAttrib2fv)
  N(glVertexAttrib3fv)
  N(glVertexAttrib4fv)
  N(glVertexAttribPointer)
  N(glViewport)

  N(glGetAttribLocation)

  N(glBlendEquationSeparate)
  N(glDrawBuffers)
  N(glStencilOpSeparate)
  N(glStencilFuncSeparate)
  N(glStencilMaskSeparate)

  N(glBindVertexArray)
  N(glDeleteVertexArrays)
  N(glGenVertexArrays)

#undef N

  auto fun = emscripten_webgl_get_proc_address(name);
  return reinterpret_cast<void*>(fun);
}
}  // namespace pag
