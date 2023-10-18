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

#include "gpu/SamplerState.h"
#include "gpu/TextureProxy.h"
#include "gpu/YUVTexture.h"
#include "gpu/processors/FragmentProcessor.h"

namespace tgfx {
class TextureEffect : public FragmentProcessor {
 public:
  static std::unique_ptr<FragmentProcessor> Make(std::shared_ptr<TextureProxy> proxy,
                                                 const SamplingOptions& sampling,
                                                 const Matrix* localMatrix = nullptr);

  static std::unique_ptr<FragmentProcessor> MakeRGBAAA(std::shared_ptr<TextureProxy> proxy,
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

  TextureEffect(std::shared_ptr<TextureProxy> proxy, SamplingOptions sampling,
                const Point& alphaStart, const Matrix& localMatrix);

  bool onIsEqual(const FragmentProcessor& processor) const override;

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  size_t onCountTextureSamplers() const override;

  const TextureSampler* onTextureSampler(size_t index) const override;

  SamplerState onSamplerState(size_t) const override {
    return samplerState;
  }

  Texture* getTexture() const;

  YUVTexture* getYUVTexture() const;

  void onVisitProxies(const std::function<void(TextureProxy*)>& func) const override {
    func(textureProxy.get());
  }

  std::shared_ptr<TextureProxy> textureProxy;
  SamplerState samplerState;
  Point alphaStart = Point::Zero();
  CoordTransform coordTransform;
};
}  // namespace tgfx
