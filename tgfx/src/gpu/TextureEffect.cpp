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

#include "TextureEffect.h"
#include "YUVTextureEffect.h"
#include "opengl/GLRGBAAATextureEffect.h"

namespace tgfx {
std::unique_ptr<FragmentProcessor> TextureEffect::Make(std::shared_ptr<Texture> texture,
                                                       const SamplingOptions& sampling,
                                                       const Matrix* localMatrix) {
  return MakeRGBAAA(std::move(texture), sampling, Point::Zero(), localMatrix);
}

std::unique_ptr<FragmentProcessor> TextureEffect::MakeRGBAAA(std::shared_ptr<Texture> texture,
                                                             const SamplingOptions& sampling,
                                                             const Point& alphaStart,
                                                             const Matrix* localMatrix) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto matrix = localMatrix ? *localMatrix : Matrix::I();
  if (texture->isYUV()) {
    return std::unique_ptr<YUVTextureEffect>(new YUVTextureEffect(
        std::static_pointer_cast<YUVTexture>(texture), sampling, alphaStart, matrix));
  }
  return std::unique_ptr<TextureEffect>(
      new TextureEffect(std::move(texture), sampling, alphaStart, matrix));
}

TextureEffect::TextureEffect(std::shared_ptr<Texture> texture, SamplingOptions sampling,
                             const Point& alphaStart, const Matrix& localMatrix)
    : FragmentProcessor(ClassID()),
      texture(std::move(texture)),
      samplerState(sampling),
      alphaStart(alphaStart),
      coordTransform(localMatrix, this->texture.get()) {
  setTextureSamplerCnt(1);
  addCoordTransform(&coordTransform);
}

bool TextureEffect::onIsEqual(const FragmentProcessor& processor) const {
  const auto& that = static_cast<const TextureEffect&>(processor);
  return texture == that.texture && alphaStart == that.alphaStart &&
         coordTransform.matrix == that.coordTransform.matrix && samplerState == that.samplerState;
}

void TextureEffect::onComputeProcessorKey(BytesKey* bytesKey) const {
  uint32_t flags = alphaStart == Point::Zero() ? 1 : 0;
  bytesKey->write(flags);
}

std::unique_ptr<GLFragmentProcessor> TextureEffect::onCreateGLInstance() const {
  return std::make_unique<GLRGBAAATextureEffect>();
}
}  // namespace tgfx
