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

#include "YUVTextureFragmentProcessor.h"
#include "opengl/GLYUVTextureFragmentProcessor.h"

namespace pag {
std::unique_ptr<YUVTextureFragmentProcessor> YUVTextureFragmentProcessor::Make(
    const YUVTexture* texture, const RGBAAALayout* layout, const Matrix& localMatrix) {
  return std::unique_ptr<YUVTextureFragmentProcessor>(
      new YUVTextureFragmentProcessor(texture, layout, localMatrix));
}

YUVTextureFragmentProcessor::YUVTextureFragmentProcessor(const YUVTexture* texture,
                                                         const RGBAAALayout* layout,
                                                         const Matrix& localMatrix)
    : texture(texture), layout(layout), coordTransform(localMatrix) {
  setTextureSamplerCnt(texture->samplerCount());
  addCoordTransform(&coordTransform);
}

void YUVTextureFragmentProcessor::onComputeProcessorKey(BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
  uint32_t flags = texture->pixelFormat() == YUVPixelFormat::I420 ? 0 : 1;
  flags |= layout == nullptr ? 0 : 2;
  flags |= texture->colorRange() == YUVColorRange::MPEG ? 0 : 4;
  bytesKey->write(flags);
}

std::unique_ptr<GLFragmentProcessor> YUVTextureFragmentProcessor::onCreateGLInstance() const {
  return std::make_unique<GLYUVTextureFragmentProcessor>();
}
}  // namespace pag
