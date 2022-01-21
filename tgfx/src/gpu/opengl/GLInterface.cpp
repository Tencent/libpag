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
#include "GLState.h"
#include "GLUtil.h"

namespace pag {
static int GetGLVersion(const GLProcGetter* getter) {
  if (getter == nullptr) {
    return -1;
  }
  auto glGetString = reinterpret_cast<GLGetString*>(getter->getProcAddress("glGetString"));
  if (glGetString == nullptr) {
    return -1;
  }
  auto versionString = (const char*)glGetString(GL::VERSION);
  return GetGLVersion(versionString).majorVersion;
}

const GLInterface* GLInterface::GetNative(const GLProcGetter* getter, GLInterfaceCache* cache) {
  auto version = GetGLVersion(getter);
  if (version <= 0) {
    return nullptr;
  }
  std::lock_guard<std::mutex> autoLock(cache->locker);
  auto result = cache->glInterfaceMap.find(version);
  if (result != cache->glInterfaceMap.end()) {
    return result->second.get();
  }
  cache->glInterfaceMap[version] = MakeNativeInterface(getter);
  return cache->glInterfaceMap[version].get();
}

namespace {
template <typename R, typename... Args>
GLFunction<R GL_FUNCTION_TYPE(Args...)> Bind(GLState* interface, R (GLState::*member)(Args...)) {
  return [interface, member](Args... a) -> R { return (interface->*member)(a...); };
}
}  // anonymous namespace

#ifdef TGFX_BUILD_FOR_WEB
#define Hook(X) state
#else
#define Hook(X) interface->X = Bind(state, &GLState::X)
#endif

std::unique_ptr<const GLInterface> GLInterface::HookWithState(const GLInterface* gl,
                                                              GLState* state) {
  auto interface = new GLInterface();
  *interface = *gl;
  Hook(activeTexture);
  Hook(blendEquation);
  Hook(blendFunc);
  Hook(bindFramebuffer);
  Hook(bindRenderbuffer);
  Hook(bindBuffer);
  Hook(bindTexture);
  Hook(disable);
  Hook(disableVertexAttribArray);
  Hook(enable);
  Hook(enableVertexAttribArray);
  Hook(pixelStorei);
  Hook(scissor);
  Hook(viewport);
  Hook(useProgram);
  Hook(vertexAttribPointer);
  Hook(depthMask);
  if (gl->caps->vertexArrayObjectSupport) {
    Hook(bindVertexArray);
  }
  return std::unique_ptr<const GLInterface>(interface);
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
  interface->activeTexture =
      reinterpret_cast<GLActiveTexture*>(getter->getProcAddress("glActiveTexture"));
  interface->attachShader =
      reinterpret_cast<GLAttachShader*>(getter->getProcAddress("glAttachShader"));
  interface->bindBuffer = reinterpret_cast<GLBindBuffer*>(getter->getProcAddress("glBindBuffer"));
  interface->bindFramebuffer =
      reinterpret_cast<GLBindFramebuffer*>(getter->getProcAddress("glBindFramebuffer"));
  interface->bindRenderbuffer =
      reinterpret_cast<GLBindRenderbuffer*>(getter->getProcAddress("glBindRenderbuffer"));
  interface->bindTexture =
      reinterpret_cast<GLBindTexture*>(getter->getProcAddress("glBindTexture"));
  interface->blendEquation =
      reinterpret_cast<GLBlendEquation*>(getter->getProcAddress("glBlendEquation"));
  interface->blendEquationSeparate =
      reinterpret_cast<GLBlendEquationSeparate*>(getter->getProcAddress("glBlendEquationSeparate"));
  interface->blendFunc = reinterpret_cast<GLBlendFunc*>(getter->getProcAddress("glBlendFunc"));
  interface->blendFuncSeparate =
      reinterpret_cast<GLBlendFuncSeparate*>(getter->getProcAddress("glBlendFuncSeparate"));
  interface->bufferData = reinterpret_cast<GLBufferData*>(getter->getProcAddress("glBufferData"));
  interface->clear = reinterpret_cast<GLClear*>(getter->getProcAddress("glClear"));
  interface->clearColor = reinterpret_cast<GLClearColor*>(getter->getProcAddress("glClearColor"));
  interface->compileShader =
      reinterpret_cast<GLCompileShader*>(getter->getProcAddress("glCompileShader"));
  interface->copyTexSubImage2D =
      reinterpret_cast<GLCopyTexSubImage2D*>(getter->getProcAddress("glCopyTexSubImage2D"));
  interface->createProgram =
      reinterpret_cast<GLCreateProgram*>(getter->getProcAddress("glCreateProgram"));
  interface->createShader =
      reinterpret_cast<GLCreateShader*>(getter->getProcAddress("glCreateShader"));
  interface->deleteBuffers =
      reinterpret_cast<GLDeleteBuffers*>(getter->getProcAddress("glDeleteBuffers"));
  interface->deleteFramebuffers =
      reinterpret_cast<GLDeleteFramebuffers*>(getter->getProcAddress("glDeleteFramebuffers"));
  interface->deleteProgram =
      reinterpret_cast<GLDeleteProgram*>(getter->getProcAddress("glDeleteProgram"));
  interface->deleteRenderbuffers =
      reinterpret_cast<GLDeleteRenderbuffers*>(getter->getProcAddress("glDeleteRenderbuffers"));
  interface->deleteShader =
      reinterpret_cast<GLDeleteShader*>(getter->getProcAddress("glDeleteShader"));
  interface->deleteTextures =
      reinterpret_cast<GLDeleteTextures*>(getter->getProcAddress("glDeleteTextures"));
  interface->depthMask = reinterpret_cast<GLDepthMask*>(getter->getProcAddress("glDepthMask"));
  interface->disable = reinterpret_cast<GLDisable*>(getter->getProcAddress("glDisable"));
  interface->disableVertexAttribArray = reinterpret_cast<GLDisableVertexAttribArray*>(
      getter->getProcAddress("glDisableVertexAttribArray"));
  interface->drawArrays = reinterpret_cast<GLDrawArrays*>(getter->getProcAddress("glDrawArrays"));
  interface->drawElements =
      reinterpret_cast<GLDrawElements*>(getter->getProcAddress("glDrawElements"));
  interface->enable = reinterpret_cast<GLEnable*>(getter->getProcAddress("glEnable"));
  interface->isEnabled = reinterpret_cast<GLIsEnabled*>(getter->getProcAddress("glIsEnabled"));
  interface->enableVertexAttribArray = reinterpret_cast<GLEnableVertexAttribArray*>(
      getter->getProcAddress("glEnableVertexAttribArray"));
  interface->finish = reinterpret_cast<GLFinish*>(getter->getProcAddress("glFinish"));
  interface->flush = reinterpret_cast<GLFlush*>(getter->getProcAddress("glFlush"));
  interface->framebufferRenderbuffer = reinterpret_cast<GLFramebufferRenderbuffer*>(
      getter->getProcAddress("glFramebufferRenderbuffer"));
  interface->framebufferTexture2D =
      reinterpret_cast<GLFramebufferTexture2D*>(getter->getProcAddress("glFramebufferTexture2D"));
  interface->genBuffers = reinterpret_cast<GLGenBuffers*>(getter->getProcAddress("glGenBuffers"));
  interface->genFramebuffers =
      reinterpret_cast<GLGenFramebuffers*>(getter->getProcAddress("glGenFramebuffers"));
  interface->genRenderbuffers =
      reinterpret_cast<GLGenRenderbuffers*>(getter->getProcAddress("glGenRenderbuffers"));
  interface->genTextures =
      reinterpret_cast<GLGenTextures*>(getter->getProcAddress("glGenTextures"));
  interface->getIntegerv =
      reinterpret_cast<GLGetIntegerv*>(getter->getProcAddress("glGetIntegerv"));
  interface->getBooleanv =
      reinterpret_cast<GLGetBooleanv*>(getter->getProcAddress("glGetBooleanv"));
  interface->getProgramInfoLog =
      reinterpret_cast<GLGetProgramInfoLog*>(getter->getProcAddress("glGetProgramInfoLog"));
  interface->getProgramiv =
      reinterpret_cast<GLGetProgramiv*>(getter->getProcAddress("glGetProgramiv"));
  interface->getRenderbufferParameteriv = reinterpret_cast<GLGetRenderbufferParameteriv*>(
      getter->getProcAddress("glGetRenderbufferParameteriv"));
  interface->getShaderInfoLog =
      reinterpret_cast<GLGetShaderInfoLog*>(getter->getProcAddress("glGetShaderInfoLog"));
  interface->getShaderiv =
      reinterpret_cast<GLGetShaderiv*>(getter->getProcAddress("glGetShaderiv"));
  interface->getString = reinterpret_cast<GLGetString*>(getter->getProcAddress("glGetString"));
  interface->getVertexAttribiv =
      reinterpret_cast<GLGetVertexAttribiv*>(getter->getProcAddress("glGetVertexAttribiv"));
  interface->getVertexAttribPointerv = reinterpret_cast<GLGetVertexAttribPointerv*>(
      getter->getProcAddress("glGetVertexAttribPointerv"));
  interface->getAttribLocation =
      reinterpret_cast<GLGetAttribLocation*>(getter->getProcAddress("glGetAttribLocation"));
  interface->getUniformLocation =
      reinterpret_cast<GLGetUniformLocation*>(getter->getProcAddress("glGetUniformLocation"));
  interface->linkProgram =
      reinterpret_cast<GLLinkProgram*>(getter->getProcAddress("glLinkProgram"));
  interface->pixelStorei =
      reinterpret_cast<GLPixelStorei*>(getter->getProcAddress("glPixelStorei"));
  interface->readPixels = reinterpret_cast<GLReadPixels*>(getter->getProcAddress("glReadPixels"));
  interface->renderbufferStorage =
      reinterpret_cast<GLRenderbufferStorage*>(getter->getProcAddress("glRenderbufferStorage"));
  interface->resolveMultisampleFramebuffer = reinterpret_cast<GLResolveMultisampleFramebuffer*>(
      getter->getProcAddress("glResolveMultisampleFramebufferAPPLE"));
  interface->scissor = reinterpret_cast<GLScissor*>(getter->getProcAddress("glScissor"));
  interface->shaderSource =
      reinterpret_cast<GLShaderSource*>(getter->getProcAddress("glShaderSource"));
  interface->texImage2D = reinterpret_cast<GLTexImage2D*>(getter->getProcAddress("glTexImage2D"));
  interface->texParameteri =
      reinterpret_cast<GLTexParameteri*>(getter->getProcAddress("glTexParameteri"));
  interface->texParameteriv =
      reinterpret_cast<GLTexParameteriv*>(getter->getProcAddress("glTexParameteriv"));
  interface->texSubImage2D =
      reinterpret_cast<GLTexSubImage2D*>(getter->getProcAddress("glTexSubImage2D"));
  interface->uniform1f = reinterpret_cast<GLUniform1f*>(getter->getProcAddress("glUniform1f"));
  interface->uniform1i = reinterpret_cast<GLUniform1i*>(getter->getProcAddress("glUniform1i"));
  interface->uniform2f = reinterpret_cast<GLUniform2f*>(getter->getProcAddress("glUniform2f"));
  interface->uniform3f = reinterpret_cast<GLUniform3f*>(getter->getProcAddress("glUniform3f"));
  interface->uniform4fv = reinterpret_cast<GLUniform4fv*>(getter->getProcAddress("glUniform4fv"));
  interface->uniformMatrix3fv =
      reinterpret_cast<GLUniformMatrix3fv*>(getter->getProcAddress("glUniformMatrix3fv"));
  interface->useProgram = reinterpret_cast<GLUseProgram*>(getter->getProcAddress("glUseProgram"));
  interface->vertexAttribPointer =
      reinterpret_cast<GLVertexAttribPointer*>(getter->getProcAddress("glVertexAttribPointer"));
  interface->viewport = reinterpret_cast<GLViewport*>(getter->getProcAddress("glViewport"));
  interface->fenceSync = reinterpret_cast<GLFenceSync*>(getter->getProcAddress("glFenceSync"));
  interface->waitSync = reinterpret_cast<GLWaitSync*>(getter->getProcAddress("glWaitSync"));
  interface->deleteSync = reinterpret_cast<GLDeleteSync*>(getter->getProcAddress("glDeleteSync"));
  switch (info.standard) {
    case GLStandard::None:
      break;
    case GLStandard::GL:
      GLAssembleGLInterface(getter, interface, info);
      break;
    case GLStandard::GLES:
      GLAssembleGLESInterface(getter, interface, info);
      break;
    case GLStandard::WebGL:
      GLAssembleWebGLInterface(getter, interface, info);
      break;
  }
  interface->caps = std::shared_ptr<const GLCaps>(new GLCaps(info));
  return std::unique_ptr<const GLInterface>(interface);
}
}  // namespace pag
