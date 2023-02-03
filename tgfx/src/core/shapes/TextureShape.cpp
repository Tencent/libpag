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

#include "TextureShape.h"
#include "gpu/TextureEffect.h"
#include "gpu/ops/FillRectOp.h"
#include "tgfx/core/Mask.h"

namespace tgfx {
TextureShape::TextureShape(std::unique_ptr<PathProxy> pathProxy, float resolutionScale)
    : PathShape(std::move(pathProxy), resolutionScale) {
}

std::unique_ptr<DrawOp> TextureShape::makeOp(GpuPaint* paint, const Matrix& viewMatrix,
                                             bool skipGeneratingCache) const {
  auto resourceCache = paint->context->resourceCache();
  auto texture = std::static_pointer_cast<Texture>(resourceCache->findResourceByOwner(this));
  if (texture != nullptr) {
    return makeTextureOp(texture, paint, viewMatrix);
  }
  auto path = getFillPath();
  auto width = ceilf(bounds.width());
  auto height = ceilf(bounds.height());
  auto mask = Mask::Make(static_cast<int>(width), static_cast<int>(height));
  if (!mask) {
    return nullptr;
  }
  auto matrix = Matrix::MakeTrans(-bounds.x(), -bounds.y());
  matrix.postScale(width / bounds.width(), height / bounds.height());
  mask->setMatrix(matrix);
  mask->fillPath(path);
  // TODO(pengweilv): mip map
  texture = mask->updateTexture(paint->context);
  if (texture == nullptr) {
    return nullptr;
  }
  if (!skipGeneratingCache) {
    texture->assignCacheOwner(this);
  }
  return makeTextureOp(texture, paint, viewMatrix);
}

std::unique_ptr<DrawOp> TextureShape::makeTextureOp(std::shared_ptr<Texture> texture,
                                                    GpuPaint* paint,
                                                    const Matrix& viewMatrix) const {
  auto maskLocalMatrix = Matrix::I();
  maskLocalMatrix.postTranslate(-bounds.x(), -bounds.y());
  maskLocalMatrix.postScale(static_cast<float>(texture->width()) / bounds.width(),
                            static_cast<float>(texture->height()) / bounds.height());
  paint->coverageFragmentProcessors.emplace_back(FragmentProcessor::MulInputByChildAlpha(
      TextureEffect::Make(std::move(texture), SamplingOptions(), &maskLocalMatrix)));
  return FillRectOp::Make(paint->color, bounds, viewMatrix);
}
}  // namespace tgfx
