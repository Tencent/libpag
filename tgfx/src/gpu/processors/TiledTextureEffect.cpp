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

#include "TiledTextureEffect.h"
#include "ConstColorProcessor.h"
#include "TextureEffect.h"
#include "gpu/ProxyProvider.h"
#include "utils/MathExtra.h"

namespace tgfx {
using Wrap = SamplerState::WrapMode;

TiledTextureEffect::ShaderMode TiledTextureEffect::GetShaderMode(Wrap wrap, FilterMode filter,
                                                                 MipMapMode mm) {
  switch (wrap) {
    case Wrap::MirrorRepeat:
      return ShaderMode::MirrorRepeat;
    case Wrap::Clamp:
      return ShaderMode::Clamp;
    case Wrap::Repeat:
      switch (mm) {
        case MipMapMode::None:
          switch (filter) {
            case FilterMode::Nearest:
              return ShaderMode::RepeatNearestNone;
            case FilterMode::Linear:
              return ShaderMode::RepeatLinearNone;
          }
        case MipMapMode::Nearest:
        case MipMapMode::Linear:
          switch (filter) {
            case FilterMode::Nearest:
              return ShaderMode::RepeatNearestMipmap;
            case FilterMode::Linear:
              return ShaderMode::RepeatLinearMipmap;
          }
      }
    case Wrap::ClampToBorder:
      switch (filter) {
        case FilterMode::Nearest:
          return ShaderMode::ClampToBorderNearest;
        case FilterMode::Linear:
          return ShaderMode::ClampToBorderLinear;
      }
  }
}

std::unique_ptr<FragmentProcessor> TiledTextureEffect::Make(std::shared_ptr<Texture> texture,
                                                            TileMode tileModeX, TileMode tileModeY,
                                                            const SamplingOptions& sampling,
                                                            const Matrix* localMatrix) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto context = texture->getContext();
  if (context == nullptr) {
    return nullptr;
  }
  auto proxy = context->proxyProvider()->wrapTexture(std::move(texture));
  return Make(std::move(proxy), tileModeX, tileModeY, sampling, localMatrix);
}

TiledTextureEffect::Sampling::Sampling(const Texture* texture, SamplerState sampler,
                                       const Rect& subset) {
  struct Span {
    float a = 0.f;
    float b = 0.f;

    Span makeInset(float o) const {
      Span r = {a + o, b - o};
      if (r.a > r.b) {
        r.a = r.b = (r.a + r.b) / 2;
      }
      return r;
    }
  };
  struct Result1D {
    ShaderMode shaderMode = ShaderMode::None;
    Span shaderSubset;
    Span shaderClamp;
    Wrap hwWrap = Wrap::Clamp;
  };
  auto caps = texture->getContext()->caps();
  auto canDoWrapInHW = [&](int size, Wrap wrap) {
    if (wrap == Wrap::ClampToBorder && !caps->clampToBorderSupport) {
      return false;
    }
    if (wrap != Wrap::Clamp && !caps->npotTextureTileSupport && !IsPow2(size)) {
      return false;
    }
    if (texture->getSampler()->type() != TextureType::TwoD &&
        !(wrap == Wrap::Clamp || wrap == Wrap::ClampToBorder)) {
      return false;
    }
    return true;
  };
  auto resolve = [&](int size, Wrap wrap, Span subset) {
    Result1D r;
    bool canDoModeInHW = canDoWrapInHW(size, wrap);
    if (canDoModeInHW && subset.a <= 0 && subset.b >= static_cast<float>(size)) {
      r.hwWrap = wrap;
      return r;
    }
    r.shaderSubset = subset;
    r.shaderClamp = subset.makeInset(0.5f);
    r.shaderMode = GetShaderMode(wrap, sampler.filterMode, sampler.mipMapMode);
    return r;
  };

  Span subsetX{subset.left, subset.right};
  auto x = resolve(texture->width(), sampler.wrapModeX, subsetX);
  Span subsetY{subset.top, subset.bottom};
  auto y = resolve(texture->height(), sampler.wrapModeY, subsetY);
  hwSampler = SamplerState(x.hwWrap, y.hwWrap, sampler.filterMode, sampler.mipMapMode);
  shaderModeX = x.shaderMode;
  shaderModeY = y.shaderMode;
  shaderSubset = {x.shaderSubset.a, y.shaderSubset.a, x.shaderSubset.b, y.shaderSubset.b};
  shaderClamp = {x.shaderClamp.a, y.shaderClamp.a, x.shaderClamp.b, y.shaderClamp.b};
}

TiledTextureEffect::TiledTextureEffect(std::shared_ptr<TextureProxy> proxy,
                                       const SamplerState& samplerState, const Matrix& localMatrix)
    : FragmentProcessor(ClassID()),
      textureProxy(std::move(proxy)),
      samplerState(samplerState),
      coordTransform(localMatrix, textureProxy.get()) {
  addCoordTransform(&coordTransform);
}

void TiledTextureEffect::onComputeProcessorKey(BytesKey* bytesKey) const {
  auto texture = getTexture();
  if (texture == nullptr) {
    return;
  }
  auto subset = Rect::MakeWH(texture->width(), texture->height());
  Sampling sampling(texture, samplerState, subset);
  auto flags = static_cast<uint32_t>(sampling.shaderModeX);
  flags |= static_cast<uint32_t>(sampling.shaderModeY) << 4;
  bytesKey->write(flags);
}

bool TiledTextureEffect::onIsEqual(const FragmentProcessor& processor) const {
  const auto& that = static_cast<const TiledTextureEffect&>(processor);
  return textureProxy == that.textureProxy && samplerState == that.samplerState &&
         coordTransform.matrix == that.coordTransform.matrix;
}

size_t TiledTextureEffect::onCountTextureSamplers() const {
  auto texture = getTexture();
  return texture ? 1 : 0;
}

const TextureSampler* TiledTextureEffect::onTextureSampler(size_t) const {
  auto texture = getTexture();
  if (texture == nullptr) {
    return nullptr;
  }
  return texture->getSampler();
}

SamplerState TiledTextureEffect::onSamplerState(size_t) const {
  auto texture = getTexture();
  if (texture == nullptr) {
    return {};
  }
  auto subset = Rect::MakeWH(texture->width(), texture->height());
  Sampling sampling(texture, samplerState, subset);
  return sampling.hwSampler;
}

const Texture* TiledTextureEffect::getTexture() const {
  auto texture = textureProxy->getTexture().get();
  if (texture && !texture->isYUV()) {
    return texture;
  }
  return nullptr;
}
}  // namespace tgfx
