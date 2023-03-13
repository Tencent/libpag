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

#include "GLInterface.h"
#include <mutex>
#include <unordered_map>
#include "GLAssembledGLESInterface.h"
#include "GLAssembledGLInterface.h"
#include "GLAssembledWebGLInterface.h"
#include "GLUtil.h"

namespace tgfx {
static std::mutex interfaceLocker = {};
static std::unordered_map<int, std::unique_ptr<const GLInterface>> glInterfaceMap = {};

static int GetGLVersion(const GLProcGetter* getter) {
  if (getter == nullptr) {
    return -1;
  }
  auto glGetString = reinterpret_cast<GLGetString*>(getter->getProcAddress("glGetString"));
  if (glGetString == nullptr) {
    return -1;
  }
  auto versionString = (const char*)glGetString(GL_VERSION);
  return GetGLVersion(versionString).majorVersion;
}

const GLInterface* GLInterface::Get(const Context* context) {
  return context ? static_cast<const GLContext*>(context)->glInterface : nullptr;
}

const GLInterface* GLInterface::GetNative() {
  auto getter = GLProcGetter::Make();
  if (getter == nullptr) {
    return nullptr;
  }
  auto version = GetGLVersion(getter.get());
  if (version <= 0) {
    return nullptr;
  }
  std::lock_guard<std::mutex> autoLock(interfaceLocker);
  auto result = glInterfaceMap.find(version);
  if (result != glInterfaceMap.end()) {
    return result->second.get();
  }
  glInterfaceMap[version] = MakeNativeInterface(getter.get());
  return glInterfaceMap[version].get();
}

std::unique_ptr<const GLInterface> GLInterface::MakeNativeInterface(const GLProcGetter* getter) {
  if (getter == nullptr) {
    return nullptr;
  }
  auto getString = reinterpret_cast<GLGetString*>(getter->getProcAddress("glGetString"));
  if (getString == nullptr) {
    return nullptr;
  }
  auto getIntegerv = reinterpret_cast<GLGetIntegerv*>(getter->getProcAddress("glGetIntegerv"));
  if (getIntegerv == nullptr) {
    return nullptr;
  }
  auto getShaderPrecisionFormat = reinterpret_cast<GLGetShaderPrecisionFormat*>(
      getter->getProcAddress("glGetShaderPrecisionFormat"));
  auto getStringi = reinterpret_cast<GLGetStringi*>(getter->getProcAddress("glGetStringi"));
  auto getInternalformativ =
      reinterpret_cast<GLGetInternalformativ*>(getter->getProcAddress("glGetInternalformativ"));
  GLInfo info(getString, getStringi, getIntegerv, getInternalformativ, getShaderPrecisionFormat);
  auto interface = new GLInterface();
  auto functions = std::make_shared<GLFunctions>();
  interface->functions = functions;
  functions->activeTexture =
      reinterpret_cast<GLActiveTexture*>(getter->getProcAddress("glActiveTexture"));
  functions->attachShader =
      reinterpret_cast<GLAttachShader*>(getter->getProcAddress("glAttachShader"));
  functions->bindAttribLocation =
      reinterpret_cast<GLBindAttribLocation*>(getter->getProcAddress("glBindAttribLocation"));
  functions->bindBuffer = reinterpret_cast<GLBindBuffer*>(getter->getProcAddress("glBindBuffer"));
  functions->bindFramebuffer =
      reinterpret_cast<GLBindFramebuffer*>(getter->getProcAddress("glBindFramebuffer"));
  functions->bindRenderbuffer =
      reinterpret_cast<GLBindRenderbuffer*>(getter->getProcAddress("glBindRenderbuffer"));
  functions->bindTexture =
      reinterpret_cast<GLBindTexture*>(getter->getProcAddress("glBindTexture"));
  functions->blendColor = reinterpret_cast<GLBlendColor*>(getter->getProcAddress("glBlendColor"));
  functions->blendEquation =
      reinterpret_cast<GLBlendEquation*>(getter->getProcAddress("glBlendEquation"));
  functions->blendEquationSeparate =
      reinterpret_cast<GLBlendEquationSeparate*>(getter->getProcAddress("glBlendEquationSeparate"));
  functions->blendFunc = reinterpret_cast<GLBlendFunc*>(getter->getProcAddress("glBlendFunc"));
  functions->blendFuncSeparate =
      reinterpret_cast<GLBlendFuncSeparate*>(getter->getProcAddress("glBlendFuncSeparate"));
  functions->bufferData = reinterpret_cast<GLBufferData*>(getter->getProcAddress("glBufferData"));
  functions->bufferSubData =
      reinterpret_cast<GLBufferSubData*>(getter->getProcAddress("glBufferSubData"));
  functions->checkFramebufferStatus = reinterpret_cast<GLCheckFramebufferStatus*>(
      getter->getProcAddress("glCheckFramebufferStatus"));
  functions->clear = reinterpret_cast<GLClear*>(getter->getProcAddress("glClear"));
  functions->clearColor = reinterpret_cast<GLClearColor*>(getter->getProcAddress("glClearColor"));
  functions->clearStencil =
      reinterpret_cast<GLClearStencil*>(getter->getProcAddress("glClearStencil"));
  functions->colorMask = reinterpret_cast<GLColorMask*>(getter->getProcAddress("glColorMask"));
  functions->compileShader =
      reinterpret_cast<GLCompileShader*>(getter->getProcAddress("glCompileShader"));
  functions->compressedTexImage2D =
      reinterpret_cast<GLCompressedTexImage2D*>(getter->getProcAddress("glCompressedTexImage2D"));
  functions->compressedTexSubImage2D = reinterpret_cast<GLCompressedTexSubImage2D*>(
      getter->getProcAddress("glCompressedTexSubImage2D"));
  functions->copyTexSubImage2D =
      reinterpret_cast<GLCopyTexSubImage2D*>(getter->getProcAddress("glCopyTexSubImage2D"));
  functions->createProgram =
      reinterpret_cast<GLCreateProgram*>(getter->getProcAddress("glCreateProgram"));
  functions->createShader =
      reinterpret_cast<GLCreateShader*>(getter->getProcAddress("glCreateShader"));
  functions->cullFace = reinterpret_cast<GLCullFace*>(getter->getProcAddress("glCullFace"));
  functions->deleteBuffers =
      reinterpret_cast<GLDeleteBuffers*>(getter->getProcAddress("glDeleteBuffers"));
  functions->deleteFramebuffers =
      reinterpret_cast<GLDeleteFramebuffers*>(getter->getProcAddress("glDeleteFramebuffers"));
  functions->deleteProgram =
      reinterpret_cast<GLDeleteProgram*>(getter->getProcAddress("glDeleteProgram"));
  functions->deleteRenderbuffers =
      reinterpret_cast<GLDeleteRenderbuffers*>(getter->getProcAddress("glDeleteRenderbuffers"));
  functions->deleteShader =
      reinterpret_cast<GLDeleteShader*>(getter->getProcAddress("glDeleteShader"));
  functions->deleteSync = reinterpret_cast<GLDeleteSync*>(getter->getProcAddress("glDeleteSync"));
  functions->deleteTextures =
      reinterpret_cast<GLDeleteTextures*>(getter->getProcAddress("glDeleteTextures"));
  functions->depthFunc = reinterpret_cast<GLDepthFunc*>(getter->getProcAddress("glDepthFunc"));
  functions->depthMask = reinterpret_cast<GLDepthMask*>(getter->getProcAddress("glDepthMask"));
  functions->disable = reinterpret_cast<GLDisable*>(getter->getProcAddress("glDisable"));
  functions->disableVertexAttribArray = reinterpret_cast<GLDisableVertexAttribArray*>(
      getter->getProcAddress("glDisableVertexAttribArray"));
  functions->drawArrays = reinterpret_cast<GLDrawArrays*>(getter->getProcAddress("glDrawArrays"));
  functions->drawElements =
      reinterpret_cast<GLDrawElements*>(getter->getProcAddress("glDrawElements"));
  functions->enable = reinterpret_cast<GLEnable*>(getter->getProcAddress("glEnable"));
  functions->isEnabled = reinterpret_cast<GLIsEnabled*>(getter->getProcAddress("glIsEnabled"));
  functions->enableVertexAttribArray = reinterpret_cast<GLEnableVertexAttribArray*>(
      getter->getProcAddress("glEnableVertexAttribArray"));
  functions->fenceSync = reinterpret_cast<GLFenceSync*>(getter->getProcAddress("glFenceSync"));
  functions->finish = reinterpret_cast<GLFinish*>(getter->getProcAddress("glFinish"));
  functions->flush = reinterpret_cast<GLFlush*>(getter->getProcAddress("glFlush"));
  functions->framebufferRenderbuffer = reinterpret_cast<GLFramebufferRenderbuffer*>(
      getter->getProcAddress("glFramebufferRenderbuffer"));
  functions->framebufferTexture2D =
      reinterpret_cast<GLFramebufferTexture2D*>(getter->getProcAddress("glFramebufferTexture2D"));
  functions->frontFace = reinterpret_cast<GLFrontFace*>(getter->getProcAddress("glFrontFace"));
  functions->genBuffers = reinterpret_cast<GLGenBuffers*>(getter->getProcAddress("glGenBuffers"));
  functions->genFramebuffers =
      reinterpret_cast<GLGenFramebuffers*>(getter->getProcAddress("glGenFramebuffers"));
  functions->generateMipmap =
      reinterpret_cast<GLGenerateMipmap*>(getter->getProcAddress("glGenerateMipmap"));
  functions->genRenderbuffers =
      reinterpret_cast<GLGenRenderbuffers*>(getter->getProcAddress("glGenRenderbuffers"));
  functions->genTextures =
      reinterpret_cast<GLGenTextures*>(getter->getProcAddress("glGenTextures"));
  functions->getBufferParameteriv =
      reinterpret_cast<GLGetBufferParameteriv*>(getter->getProcAddress("glGetBufferParameteriv"));
  functions->getError = reinterpret_cast<GLGetError*>(getter->getProcAddress("glGetError"));
  functions->getFramebufferAttachmentParameteriv =
      reinterpret_cast<GLGetFramebufferAttachmentParameteriv*>(
          getter->getProcAddress("glGetFramebufferAttachmentParameteriv"));
  functions->getIntegerv =
      reinterpret_cast<GLGetIntegerv*>(getter->getProcAddress("glGetIntegerv"));
  functions->getInternalformativ =
      reinterpret_cast<GLGetInternalformativ*>(getter->getProcAddress("glGetInternalformativ"));
  functions->getBooleanv =
      reinterpret_cast<GLGetBooleanv*>(getter->getProcAddress("glGetBooleanv"));
  functions->getProgramInfoLog =
      reinterpret_cast<GLGetProgramInfoLog*>(getter->getProcAddress("glGetProgramInfoLog"));
  functions->getProgramiv =
      reinterpret_cast<GLGetProgramiv*>(getter->getProcAddress("glGetProgramiv"));
  functions->getRenderbufferParameteriv = reinterpret_cast<GLGetRenderbufferParameteriv*>(
      getter->getProcAddress("glGetRenderbufferParameteriv"));
  functions->getShaderInfoLog =
      reinterpret_cast<GLGetShaderInfoLog*>(getter->getProcAddress("glGetShaderInfoLog"));
  functions->getShaderiv =
      reinterpret_cast<GLGetShaderiv*>(getter->getProcAddress("glGetShaderiv"));
  functions->getShaderPrecisionFormat = reinterpret_cast<GLGetShaderPrecisionFormat*>(
      getter->getProcAddress("glGetShaderPrecisionFormat"));
  functions->getString = reinterpret_cast<GLGetString*>(getter->getProcAddress("glGetString"));
  functions->getStringi = reinterpret_cast<GLGetStringi*>(getter->getProcAddress("glGetStringi"));
  functions->getVertexAttribiv =
      reinterpret_cast<GLGetVertexAttribiv*>(getter->getProcAddress("glGetVertexAttribiv"));
  functions->getVertexAttribPointerv = reinterpret_cast<GLGetVertexAttribPointerv*>(
      getter->getProcAddress("glGetVertexAttribPointerv"));
  functions->getAttribLocation =
      reinterpret_cast<GLGetAttribLocation*>(getter->getProcAddress("glGetAttribLocation"));
  functions->getUniformLocation =
      reinterpret_cast<GLGetUniformLocation*>(getter->getProcAddress("glGetUniformLocation"));
  functions->isTexture = reinterpret_cast<GLIsTexture*>(getter->getProcAddress("glIsTexture"));
  functions->lineWidth = reinterpret_cast<GLLineWidth*>(getter->getProcAddress("glLineWidth"));
  functions->linkProgram =
      reinterpret_cast<GLLinkProgram*>(getter->getProcAddress("glLinkProgram"));
  functions->pixelStorei =
      reinterpret_cast<GLPixelStorei*>(getter->getProcAddress("glPixelStorei"));
  functions->readPixels = reinterpret_cast<GLReadPixels*>(getter->getProcAddress("glReadPixels"));
  functions->renderbufferStorage =
      reinterpret_cast<GLRenderbufferStorage*>(getter->getProcAddress("glRenderbufferStorage"));
  functions->resolveMultisampleFramebuffer = reinterpret_cast<GLResolveMultisampleFramebuffer*>(
      getter->getProcAddress("glResolveMultisampleFramebufferAPPLE"));
  functions->scissor = reinterpret_cast<GLScissor*>(getter->getProcAddress("glScissor"));
  functions->shaderSource =
      reinterpret_cast<GLShaderSource*>(getter->getProcAddress("glShaderSource"));
  functions->stencilFunc =
      reinterpret_cast<GLStencilFunc*>(getter->getProcAddress("glStencilFunc"));
  functions->stencilFuncSeparate =
      reinterpret_cast<GLStencilFuncSeparate*>(getter->getProcAddress("glStencilFuncSeparate"));
  functions->stencilMask =
      reinterpret_cast<GLStencilMask*>(getter->getProcAddress("glStencilMask"));
  functions->stencilMaskSeparate =
      reinterpret_cast<GLStencilMaskSeparate*>(getter->getProcAddress("glStencilMaskSeparate"));
  functions->stencilOp = reinterpret_cast<GLStencilOp*>(getter->getProcAddress("glStencilOp"));
  functions->stencilOpSeparate =
      reinterpret_cast<GLStencilOpSeparate*>(getter->getProcAddress("glStencilOpSeparate"));
  functions->texImage2D = reinterpret_cast<GLTexImage2D*>(getter->getProcAddress("glTexImage2D"));
  functions->texParameterf =
      reinterpret_cast<GLTexParameterf*>(getter->getProcAddress("glTexParameterf"));
  functions->texParameterfv =
      reinterpret_cast<GLTexParameterfv*>(getter->getProcAddress("glTexParameterfv"));
  functions->texParameteri =
      reinterpret_cast<GLTexParameteri*>(getter->getProcAddress("glTexParameteri"));
  functions->texParameteriv =
      reinterpret_cast<GLTexParameteriv*>(getter->getProcAddress("glTexParameteriv"));
  functions->texSubImage2D =
      reinterpret_cast<GLTexSubImage2D*>(getter->getProcAddress("glTexSubImage2D"));
  functions->uniform1f = reinterpret_cast<GLUniform1f*>(getter->getProcAddress("glUniform1f"));
  functions->uniform1i = reinterpret_cast<GLUniform1i*>(getter->getProcAddress("glUniform1i"));
  functions->uniform1fv = reinterpret_cast<GLUniform1fv*>(getter->getProcAddress("glUniform1fv"));
  functions->uniform1iv = reinterpret_cast<GLUniform1iv*>(getter->getProcAddress("glUniform1iv"));
  functions->uniform2f = reinterpret_cast<GLUniform2f*>(getter->getProcAddress("glUniform2f"));
  functions->uniform2i = reinterpret_cast<GLUniform2i*>(getter->getProcAddress("glUniform2i"));
  functions->uniform2fv = reinterpret_cast<GLUniform2fv*>(getter->getProcAddress("glUniform2fv"));
  functions->uniform2iv = reinterpret_cast<GLUniform2iv*>(getter->getProcAddress("glUniform2iv"));
  functions->uniform3f = reinterpret_cast<GLUniform3f*>(getter->getProcAddress("glUniform3f"));
  functions->uniform3i = reinterpret_cast<GLUniform3i*>(getter->getProcAddress("glUniform3i"));
  functions->uniform3fv = reinterpret_cast<GLUniform3fv*>(getter->getProcAddress("glUniform3fv"));
  functions->uniform3iv = reinterpret_cast<GLUniform3iv*>(getter->getProcAddress("glUniform3iv"));
  functions->uniform4f = reinterpret_cast<GLUniform4f*>(getter->getProcAddress("glUniform4f"));
  functions->uniform4i = reinterpret_cast<GLUniform4i*>(getter->getProcAddress("glUniform4i"));
  functions->uniform4fv = reinterpret_cast<GLUniform4fv*>(getter->getProcAddress("glUniform4fv"));
  functions->uniform4iv = reinterpret_cast<GLUniform4iv*>(getter->getProcAddress("glUniform4iv"));
  functions->uniformMatrix2fv =
      reinterpret_cast<GLUniformMatrix2fv*>(getter->getProcAddress("glUniformMatrix2fv"));
  functions->uniformMatrix3fv =
      reinterpret_cast<GLUniformMatrix3fv*>(getter->getProcAddress("glUniformMatrix3fv"));
  functions->uniformMatrix4fv =
      reinterpret_cast<GLUniformMatrix4fv*>(getter->getProcAddress("glUniformMatrix4fv"));
  functions->useProgram = reinterpret_cast<GLUseProgram*>(getter->getProcAddress("glUseProgram"));
  functions->vertexAttrib1f =
      reinterpret_cast<GLVertexAttrib1f*>(getter->getProcAddress("glVertexAttrib1f"));
  functions->vertexAttrib2fv =
      reinterpret_cast<GLVertexAttrib2fv*>(getter->getProcAddress("glVertexAttrib2fv"));
  functions->vertexAttrib3fv =
      reinterpret_cast<GLVertexAttrib3fv*>(getter->getProcAddress("glVertexAttrib3fv"));
  functions->vertexAttrib4fv =
      reinterpret_cast<GLVertexAttrib4fv*>(getter->getProcAddress("glVertexAttrib4fv"));
  functions->vertexAttribPointer =
      reinterpret_cast<GLVertexAttribPointer*>(getter->getProcAddress("glVertexAttribPointer"));
  functions->viewport = reinterpret_cast<GLViewport*>(getter->getProcAddress("glViewport"));
  functions->waitSync = reinterpret_cast<GLWaitSync*>(getter->getProcAddress("glWaitSync"));

  switch (info.standard) {
    case GLStandard::None:
      break;
    case GLStandard::GL:
      GLAssembleGLInterface(getter, functions.get(), info);
      break;
    case GLStandard::GLES:
      GLAssembleGLESInterface(getter, functions.get(), info);
      break;
    case GLStandard::WebGL:
      GLAssembleWebGLInterface(getter, functions.get(), info);
      break;
  }
  interface->caps = std::shared_ptr<const GLCaps>(new GLCaps(info));
  return std::unique_ptr<const GLInterface>(interface);
}
}  // namespace tgfx
