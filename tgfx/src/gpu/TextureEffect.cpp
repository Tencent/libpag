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
#include "core/utils/Log.h"
#include "core/utils/UniqueID.h"
#include "gpu/YUVTextureEffect.h"
#include "opengl/GLTextureEffect.h"

namespace tgfx {
static bool CheckParameter(const Texture* texture, const RGBAAALayout* layout) {
  if (texture == nullptr) {
    return false;
  }
  if (layout != nullptr) {
    if (layout->width <= 0 || layout->height <= 0 ||
        (layout->alphaStartX <= 0 && layout->alphaStartY <= 0) ||
        layout->width + layout->alphaStartX > texture->width() ||
        layout->height + layout->alphaStartY > texture->height()) {
      LOGE("TextureFragmentProcessor::Make(): Invalid RGBAAALayout specified!");
      return false;
    }
  }
  return true;
}

std::unique_ptr<FragmentProcessor> TextureEffect::Make(const Texture* texture,
                                                       const Matrix& localMatrix,
                                                       const RGBAAALayout* layout) {
  if (!CheckParameter(texture, layout)) {
    return nullptr;
  }
  // normalize
  auto scale = texture->getTextureCoord(1, 1) - texture->getTextureCoord(0, 0);
  auto translate = texture->getTextureCoord(0, 0);
  auto matrix = localMatrix;
  matrix.postScale(scale.x, scale.y);
  matrix.postTranslate(translate.x, translate.y);
  if (texture->origin() == ImageOrigin::BottomLeft) {
    matrix.postScale(1, -1);
    translate = texture->getTextureCoord(0, static_cast<float>(texture->height()));
    matrix.postTranslate(translate.x, translate.y);
  }
  if (texture->isYUV()) {
    return std::unique_ptr<YUVTextureEffect>(
        new YUVTextureEffect(static_cast<const YUVTexture*>(texture), layout, matrix));
  }
  return std::unique_ptr<TextureEffect>(new TextureEffect(texture, layout, matrix));
}

TextureEffect::TextureEffect(const Texture* texture, const RGBAAALayout* layout,
                             const Matrix& localMatrix)
    : texture(texture), layout(layout), coordTransform(localMatrix) {
  setTextureSamplerCnt(1);
  addCoordTransform(&coordTransform);
}

void TextureEffect::onComputeProcessorKey(BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
  uint32_t flags = layout == nullptr ? 1 : 0;
  bytesKey->write(flags);
}

std::unique_ptr<GLFragmentProcessor> TextureEffect::onCreateGLInstance() const {
  return std::make_unique<GLTextureEffect>();
}
}  // namespace tgfx
