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

#include "FilterHelper.h"
#include "base/utils/USE.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/gpu/opengl/GLRenderTarget.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace pag {
static std::array<float, 9> ToGLMatrix(const tgfx::Matrix& matrix) {
  float values[9];
  matrix.get9(values);
  return {values[0], values[3], values[6], values[1], values[4],
          values[7], values[2], values[5], values[8]};
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
  convertMatrix.setAll(w * 0.5f, 0.0f, w * 0.5f, 0.0f, h * 0.5f, h * 0.5f, 0.0f, 0.0f, 1.0f);
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

tgfx::Matrix ToMatrix(const FilterTarget* target, bool flipY) {
  tgfx::Matrix matrix = {};
  auto values = target->vertexMatrix;
  matrix.setAll(values[0], values[3], values[6], values[1], values[4], values[7], values[2],
                values[5], values[8]);
  if (flipY) {
    matrix.postScale(1.0f, -1.0f);
  }
  tgfx::Matrix convertMatrix = {};
  // 以下等价于：
  // convertMatrix.setScale(2f/width, 2f/height);
  // convertMatrix.postTranslate(-1.0f, -1.0f);
  convertMatrix.setAll(2.0f / static_cast<float>(target->width), 0.0f, -1.0f, 0.0f,
                       2.0f / static_cast<float>(target->height), -1.0f, 0.0f, 0.0f, 1.0f);
  matrix.preConcat(convertMatrix);
  if (convertMatrix.invert(&convertMatrix)) {
    matrix.postConcat(convertMatrix);
  }
  return matrix;
}

std::unique_ptr<FilterSource> ToFilterSource(const tgfx::Texture* texture,
                                             const tgfx::Point& scale) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto filterSource = new FilterSource();
  filterSource->sampler = *static_cast<const tgfx::GLSampler*>(texture->getSampler());
  filterSource->width = texture->width();
  filterSource->height = texture->height();
  filterSource->scale = scale;
  filterSource->textureMatrix =
      ToGLTextureMatrix(tgfx::Matrix::I(), texture->width(), texture->height(), texture->origin());
  return std::unique_ptr<FilterSource>(filterSource);
}

std::unique_ptr<FilterTarget> ToFilterTarget(const tgfx::Surface* surface,
                                             const tgfx::Matrix& drawingMatrix) {
  if (surface == nullptr) {
    return nullptr;
  }
  auto renderTarget = std::static_pointer_cast<tgfx::GLRenderTarget>(surface->getRenderTarget());
  auto filterTarget = new FilterTarget();
  filterTarget->frameBuffer = renderTarget->glFrameBuffer();
  filterTarget->width = surface->width();
  filterTarget->height = surface->height();
  filterTarget->vertexMatrix =
      ToGLVertexMatrix(drawingMatrix, surface->width(), surface->height(), surface->origin());
  return std::unique_ptr<FilterTarget>(filterTarget);
}

tgfx::Point ToGLTexturePoint(const FilterSource* source, const tgfx::Point& texturePoint) {
  return {texturePoint.x * source->scale.x / static_cast<float>(source->width),
          texturePoint.y * source->scale.y / static_cast<float>(source->height)};
}

tgfx::Point ToGLVertexPoint(const FilterTarget* target, const FilterSource* source,
                            const tgfx::Rect& contentBounds, const tgfx::Point& contentPoint) {
  tgfx::Point vertexPoint = {(contentPoint.x - contentBounds.left) * source->scale.x,
                             (contentPoint.y - contentBounds.top) * source->scale.y};
  return {2.0f * vertexPoint.x / static_cast<float>(target->width) - 1.0f,
          2.0f * vertexPoint.y / static_cast<float>(target->height) - 1.0f};
}

void PreConcatMatrix(FilterTarget* target, const tgfx::Matrix& matrix) {
  auto vertexMatrix = ToMatrix(target);
  vertexMatrix.preConcat(matrix);
  target->vertexMatrix =
      ToGLVertexMatrix(vertexMatrix, target->width, target->height, tgfx::ImageOrigin::TopLeft);
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

void ActiveGLTexture(tgfx::Context* context, int unitIndex, const tgfx::TextureSampler* sampler) {
  if (sampler == nullptr) {
    return;
  }
  auto glSampler = static_cast<const tgfx::GLSampler*>(sampler);
  auto gl = tgfx::GLFunctions::Get(context);
  gl->activeTexture(GL_TEXTURE0 + unitIndex);
  gl->bindTexture(glSampler->target, glSampler->id);
  gl->texParameteri(glSampler->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->texParameteri(glSampler->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->texParameteri(glSampler->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->texParameteri(glSampler->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
