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
#include "gpu/processors/FragmentProcessor.h"

namespace tgfx {
class TiledTextureEffect : public FragmentProcessor {
 public:
  static std::unique_ptr<FragmentProcessor> Make(std::shared_ptr<TextureProxy> textureProxy,
                                                 TileMode tileModeX, TileMode tileModeY,
                                                 const SamplingOptions& sampling,
                                                 const Matrix* localMatrix = nullptr);

  static std::unique_ptr<FragmentProcessor> Make(std::shared_ptr<Texture> texture,
                                                 SamplerState samplerState = {},
                                                 const Matrix* localMatrix = nullptr);

  std::string name() const override {
    return "TiledTextureEffect";
  }

 protected:
  DEFINE_PROCESSOR_CLASS_ID

  enum class ShaderMode {
    None,                 // Using HW mode
    Clamp,                // Shader based clamp, no filter specialization
    RepeatNearestNone,    // Simple repeat for nearest sampling, no mipmapping.
    RepeatLinearNone,     // Filter the subset boundary for kRepeat mode, no mip mapping
    RepeatLinearMipmap,   // Logic for linear filtering and LOD selection with kRepeat mode.
    RepeatNearestMipmap,  // Logic for nearest filtering and LOD selection with kRepeat mode.
    MirrorRepeat,         // Mirror repeat (doesn't depend on filter))
    ClampToBorderNearest,
    ClampToBorderLinear
  };

  struct Sampling {
    Sampling(const Texture* texture, SamplerState samplerState, const Rect& subset,
             const Caps* caps);

    SamplerState hwSampler;
    ShaderMode shaderModeX = ShaderMode::None;
    ShaderMode shaderModeY = ShaderMode::None;
    Rect shaderSubset = Rect::MakeEmpty();
    Rect shaderClamp = Rect::MakeEmpty();
  };

  TiledTextureEffect(std::shared_ptr<Texture> texture, const Sampling& sampling,
                     const Matrix& localMatrix);

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  bool onIsEqual(const FragmentProcessor& processor) const override;

  const TextureSampler* onTextureSampler(size_t) const override {
    return texture->getSampler();
  }

  SamplerState onSamplerState(size_t) const override {
    return samplerState;
  }

  static ShaderMode GetShaderMode(SamplerState::WrapMode mode, FilterMode filter, MipMapMode mm);

  std::shared_ptr<Texture> texture;
  SamplerState samplerState;
  Rect subset;
  Rect clamp;
  ShaderMode shaderModeX;
  ShaderMode shaderModeY;
  CoordTransform coordTransform;
};
}  // namespace tgfx
