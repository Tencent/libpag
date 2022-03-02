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
#include "gpu/opengl/GLSurface.h"
#include "gpu/opengl/GLTexture.h"

namespace pag {
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
  // convertMatrix.postScale(1.0f, -1.0f);
  convertMatrix.setAll(2.0f / static_cast<float>(target->width), 0.0f, -1.0f, 0.0f,
                       -2.0f / static_cast<float>(target->height), 1.0f, 0.0f, 0.0f, 1.0f);
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
          (static_cast<float>(source->height) - texturePoint.y * source->scale.y) /
              static_cast<float>(source->height)};
}

tgfx::Point ToGLVertexPoint(const FilterTarget* target, const FilterSource* source,
                            const tgfx::Rect& contentBounds, const tgfx::Point& contentPoint) {
  tgfx::Point vertexPoint = {(contentPoint.x - contentBounds.left) * source->scale.x,
                             (contentPoint.y - contentBounds.top) * source->scale.y};
  return {2.0f * vertexPoint.x / static_cast<float>(target->width) - 1.0f,
          1.0f - 2.0f * vertexPoint.y / static_cast<float>(target->height)};
}

void PreConcatMatrix(FilterTarget* target, const tgfx::Matrix& matrix) {
  auto vertexMatrix = ToMatrix(target);
  vertexMatrix.preConcat(matrix);
  target->vertexMatrix = tgfx::ToGLVertexMatrix(vertexMatrix, target->width, target->height,
                                                tgfx::ImageOrigin::BottomLeft);
}
}  // namespace pag
