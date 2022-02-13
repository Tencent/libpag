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

#include "TextureFragmentProcessor.h"
#include "base/utils/Log.h"
#include "base/utils/UniqueID.h"
#include "gpu/YUVTextureFragmentProcessor.h"
#include "opengl/GLTextureFragmentProcessor.h"

namespace pag {
std::unique_ptr<FragmentProcessor> TextureFragmentProcessor::Make(const Texture* texture,
                                                                  const RGBAAALayout* layout,
                                                                  const Matrix& localMatrix) {
  if (texture == nullptr) {
    return nullptr;
  }
  if (layout != nullptr) {
    if (layout->width <= 0 || layout->height <= 0 ||
        (layout->alphaStartX <= 0 && layout->alphaStartY <= 0) ||
        layout->width + layout->alphaStartX > texture->width() ||
        layout->height + layout->alphaStartY > texture->height()) {
      LOGE("TextureFragmentProcessor::Make(): Invalid RGBAAALayout specified!");
      return nullptr;
    }
  }
  if (texture->isYUV()) {
    return std::unique_ptr<YUVTextureFragmentProcessor>(new YUVTextureFragmentProcessor(
        static_cast<const YUVTexture*>(texture), layout, localMatrix));
  }
  return std::unique_ptr<TextureFragmentProcessor>(
      new TextureFragmentProcessor(texture, layout, localMatrix));
}

TextureFragmentProcessor::TextureFragmentProcessor(const Texture* texture,
                                                   const RGBAAALayout* layout,
                                                   const Matrix& localMatrix)
    : texture(texture), layout(layout), coordTransform(localMatrix) {
  setTextureSamplerCnt(1);
  if (texture->origin() == ImageOrigin::BottomLeft) {
    coordTransform.matrix.postScale(1, -1);
    coordTransform.matrix.postTranslate(0, 1);
  }
  addCoordTransform(&coordTransform);
}

void TextureFragmentProcessor::onComputeProcessorKey(BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
  uint32_t flags = layout == nullptr ? 1 : 0;
  bytesKey->write(flags);
}

std::unique_ptr<GLFragmentProcessor> TextureFragmentProcessor::onCreateGLInstance() const {
  return std::make_unique<GLTextureFragmentProcessor>();
}
}  // namespace pag
