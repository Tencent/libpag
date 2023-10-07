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

#include "YUVTextureEffect.h"

namespace tgfx {
YUVTextureEffect::YUVTextureEffect(std::shared_ptr<YUVTexture> texture, SamplingOptions sampling,
                                   const Point& alphaStart, const Matrix& localMatrix)
    : FragmentProcessor(ClassID()),
      texture(std::move(texture)),
      samplerState(sampling),
      alphaStart(alphaStart),
      coordTransform(localMatrix, this->texture.get(), alphaStart) {
  setTextureSamplerCnt(this->texture->samplerCount());
  addCoordTransform(&coordTransform);
}

void YUVTextureEffect::onComputeProcessorKey(BytesKey* bytesKey) const {
  uint32_t flags = texture->pixelFormat() == YUVPixelFormat::I420 ? 0 : 1;
  flags |= alphaStart == Point::Zero() ? 0 : 2;
  flags |= IsLimitedYUVColorRange(texture->colorSpace()) ? 0 : 4;
  bytesKey->write(flags);
}

bool YUVTextureEffect::onIsEqual(const FragmentProcessor& processor) const {
  const auto& that = static_cast<const YUVTextureEffect&>(processor);
  return texture == that.texture && alphaStart == that.alphaStart &&
         coordTransform.matrix == that.coordTransform.matrix && samplerState == that.samplerState;
}
}  // namespace tgfx
