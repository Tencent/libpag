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

#include "SamplerState.h"
#include "gpu/FragmentProcessor.h"
#include "gpu/TextureProxy.h"

namespace tgfx {
class TextureEffect : public FragmentProcessor {
 public:
  static std::unique_ptr<FragmentProcessor> Make(std::shared_ptr<TextureProxy> textureProxy,
                                                 const SamplingOptions& sampling,
                                                 const Matrix* localMatrix = nullptr);

  static std::unique_ptr<FragmentProcessor> MakeRGBAAA(std::shared_ptr<TextureProxy> textureProxy,
                                                       const SamplingOptions& sampling,
                                                       const Point& alphaStart,
                                                       const Matrix* localMatrix = nullptr);

  static std::unique_ptr<FragmentProcessor> Make(std::shared_ptr<Texture> texture,
                                                 const SamplingOptions& sampling,
                                                 const Matrix* localMatrix = nullptr);

  static std::unique_ptr<FragmentProcessor> MakeRGBAAA(std::shared_ptr<Texture> texture,
                                                       const SamplingOptions& sampling,
                                                       const Point& alphaStart,
                                                       const Matrix* localMatrix = nullptr);

  std::string name() const override {
    return "TextureEffect";
  }

 protected:
  DEFINE_PROCESSOR_CLASS_ID

  TextureEffect(std::shared_ptr<Texture> texture, SamplingOptions sampling, const Point& alphaStart,
                const Matrix& localMatrix);

  bool onIsEqual(const FragmentProcessor& processor) const override;

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  const TextureSampler* onTextureSampler(size_t) const override {
    return texture->getSampler();
  }

  SamplerState onSamplerState(size_t) const override {
    return samplerState;
  }

  std::shared_ptr<Texture> texture;
  SamplerState samplerState;
  Point alphaStart = Point::Zero();
  CoordTransform coordTransform;
};
}  // namespace tgfx
