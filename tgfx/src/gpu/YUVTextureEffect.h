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

#pragma once

#include "FragmentProcessor.h"
#include "tgfx/gpu/YUVTexture.h"

namespace tgfx {
class YUVTextureEffect : public FragmentProcessor {
 public:
  std::string name() const override {
    return "YUVTextureEffect";
  }

 private:
  DEFINE_PROCESSOR_CLASS_ID

  YUVTextureEffect(std::shared_ptr<YUVTexture> texture, SamplingOptions sampling,
                   const Point& alphaStart, const Matrix& localMatrix);

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  std::unique_ptr<GLFragmentProcessor> onCreateGLInstance() const override;

  const TextureSampler* onTextureSampler(size_t index) const override {
    return texture->getSamplerAt(index);
  }

  SamplerState onSamplerState(size_t) const override {
    return samplerState;
  }

  bool onIsEqual(const FragmentProcessor& processor) const override;

  std::shared_ptr<YUVTexture> texture;
  SamplerState samplerState;
  Point alphaStart = Point::Zero();
  CoordTransform coordTransform;

  friend class TextureEffect;

  friend class GLYUVTextureEffect;
};
}  // namespace tgfx
