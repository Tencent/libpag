/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "FilterHelper.h"
#include "base/utils/Log.h"
#include "base/utils/USE.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/opengl/GLFunctions.h"

namespace pag {
std::array<float, 9> ToGLMatrix(const tgfx::Matrix& matrix) {
  float values[6];
  matrix.get6(values);
  return {values[0], values[3], 0, values[1], values[4], 0, values[2], values[5], 1};
}

std::array<float, 9> ToGLVertexMatrix(const tgfx::Matrix& matrix, int width, int height,
                                      tgfx::ImageOrigin origin) {
  auto result = matrix;
  auto w = static_cast<float>(width);
  auto h = static_cast<float>(height);
  tgfx::Matrix convertMatrix = {};
  // The following is equivalent：
  // convertMatrix.setTranslate(1.0f, 1.0f);
  // convertMatrix.postScale(width/2.0f, height/2.0f);
  convertMatrix.setAll(w * 0.5f, 0.0f, w * 0.5f, 0.0f, h * 0.5f, h * 0.5f);
  result.preConcat(convertMatrix);
  if (convertMatrix.invert(&convertMatrix)) {
    result.postConcat(convertMatrix);
  }
  if (origin == tgfx::ImageOrigin::BottomLeft) {
    result.postScale(1.0f, -1.0f);
  }
  return ToGLMatrix(result);
}

std::array<float, 9> ToGLTextureMatrix(const tgfx::Matrix& matrix, int width, int height,
                                       tgfx::ImageOrigin origin) {
  if (matrix.isIdentity() && origin == tgfx::ImageOrigin::TopLeft) {
    return ToGLMatrix(matrix);
  }
  auto result = matrix;
  tgfx::Matrix convertMatrix = {};
  convertMatrix.setScale(static_cast<float>(width), static_cast<float>(height));
  result.preConcat(convertMatrix);
  if (convertMatrix.invert(&convertMatrix)) {
    result.postConcat(convertMatrix);
  }
  if (origin == tgfx::ImageOrigin::BottomLeft) {
    result.postScale(1.0f, -1.0f);
    result.postTranslate(0.0f, 1.0f);
  }
  return ToGLMatrix(result);
}

tgfx::Matrix ToMatrix(const std::array<float, 9>& values) {
  tgfx::Matrix result = {};
  result.setAll(values[0], values[3], values[6], values[1], values[4], values[7]);
  return result;
}

tgfx::Point ToGLTexturePoint(const tgfx::BackendTexture* source, const tgfx::Point& texturePoint) {
  return {texturePoint.x / static_cast<float>(source->width()),
          texturePoint.y / static_cast<float>(source->height())};
}

tgfx::Point ToGLVertexPoint(const tgfx::BackendRenderTarget& target, const tgfx::Point& point) {
  return {2.0f * point.x / static_cast<float>(target.width()) - 1.0f,
          2.0f * point.y / static_cast<float>(target.height()) - 1.0f};
}

static unsigned LoadGLShader(tgfx::Context* context, unsigned shaderType,
                             const std::string& source) {
  auto gl = tgfx::GLFunctions::Get(context);
  auto shader = gl->createShader(shaderType);
  const char* files[] = {source.c_str()};
  gl->shaderSource(shader, 1, files, nullptr);
  gl->compileShader(shader);
  int success;
  gl->getShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    gl->getShaderInfoLog(shader, 512, nullptr, infoLog);
    LOGE("Could not compile shader: %d %s", shaderType, infoLog);
    gl->deleteShader(shader);
    shader = 0;
  }
  return shader;
}

unsigned CreateGLProgram(tgfx::Context* context, const std::string& vertex,
                         const std::string& fragment) {
  auto vertexShader = LoadGLShader(context, GL_VERTEX_SHADER, vertex);
  if (vertexShader == 0) {
    return 0;
  }
  auto fragmentShader = LoadGLShader(context, GL_FRAGMENT_SHADER, fragment);
  if (fragmentShader == 0) {
    return 0;
  }
  auto gl = tgfx::GLFunctions::Get(context);
  auto programHandle = gl->createProgram();
  gl->attachShader(programHandle, vertexShader);
  gl->attachShader(programHandle, fragmentShader);
  gl->linkProgram(programHandle);
  int success;
  gl->getProgramiv(programHandle, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    gl->getProgramInfoLog(programHandle, 512, nullptr, infoLog);
    gl->deleteProgram(programHandle);
  }
  gl->deleteShader(vertexShader);
  gl->deleteShader(fragmentShader);
  return programHandle;
}

void ActiveGLTexture(tgfx::Context* context, int unitIndex, const tgfx::GLTextureInfo* sampler) {
  if (sampler == nullptr) {
    return;
  }
  auto gl = tgfx::GLFunctions::Get(context);
  gl->activeTexture(GL_TEXTURE0 + unitIndex);
  gl->bindTexture(sampler->target, sampler->id);
  gl->texParameteri(sampler->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->texParameteri(sampler->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->texParameteri(sampler->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->texParameteri(sampler->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

bool CheckGLError(tgfx::Context* context) {
#ifdef PAG_BUILD_FOR_WEB
  USE(context);
  return true;
#else
  auto gl = tgfx::GLFunctions::Get(context);
  bool success = true;
  unsigned errorCode;
  while ((errorCode = gl->getError()) != GL_NO_ERROR) {
    success = false;
    LOGE("glCheckError: %d", errorCode);
  }
  return success;
#endif
}
}  // namespace pag
