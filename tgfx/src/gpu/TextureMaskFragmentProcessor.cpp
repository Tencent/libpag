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

#include "TextureMaskFragmentProcessor.h"
#include "base/utils/UniqueID.h"
#include "opengl/GLTextureMaskFragmentProcessor.h"

namespace pag {
std::unique_ptr<TextureMaskFragmentProcessor> TextureMaskFragmentProcessor::MakeUseLocalCoord(
    const Texture* texture, const Matrix& localMatrix, bool inverted) {
  if (texture == nullptr) {
    return nullptr;
  }
  return std::unique_ptr<TextureMaskFragmentProcessor>(
      new TextureMaskFragmentProcessor(texture, localMatrix, inverted));
}

std::unique_ptr<TextureMaskFragmentProcessor> TextureMaskFragmentProcessor::MakeUseDeviceCoord(
    const Texture* texture, ImageOrigin deviceOrigin) {
  if (texture == nullptr) {
    return nullptr;
  }
  return std::unique_ptr<TextureMaskFragmentProcessor>(
      new TextureMaskFragmentProcessor(texture, deviceOrigin));
}

TextureMaskFragmentProcessor::TextureMaskFragmentProcessor(const Texture* texture,
                                                           ImageOrigin deviceOrigin)
    : useLocalCoord(false), texture(texture) {
  setTextureSamplerCnt(1);
  if (deviceOrigin == ImageOrigin::BottomLeft) {
    deviceCoordMatrix.postScale(1, -1);
    deviceCoordMatrix.postTranslate(0, 1);
  }
}

TextureMaskFragmentProcessor::TextureMaskFragmentProcessor(const Texture* texture,
                                                           const Matrix& localMatrix, bool inverted)
    : useLocalCoord(true), texture(texture), coordTransform(localMatrix), inverted(inverted) {
  setTextureSamplerCnt(1);
  if (texture->origin() == ImageOrigin::BottomLeft) {
    coordTransform.matrix.postScale(1, -1);
    coordTransform.matrix.postTranslate(0, 1);
  }
  addCoordTransform(&coordTransform);
}

void TextureMaskFragmentProcessor::onComputeProcessorKey(BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
  uint32_t flags = 0;
  flags |= (useLocalCoord ? 1 : 0) << 0;
  flags |= (inverted ? 1 : 0) << 1;
  bytesKey->write(flags);
}

std::unique_ptr<GLFragmentProcessor> TextureMaskFragmentProcessor::onCreateGLInstance() const {
  return std::make_unique<GLTextureMaskFragmentProcessor>();
}
}  // namespace pag
