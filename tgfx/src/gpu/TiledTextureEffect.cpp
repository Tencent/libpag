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
#include "TextureEffect.h"
#include "core/utils/Log.h"
#include "core/utils/MathExtra.h"
#include "opengl/GLTiledTextureEffect.h"

namespace tgfx {
class TiledTextureEffectProxy : public FragmentProcessorProxy {
 public:
  TiledTextureEffectProxy(std::shared_ptr<TextureProxy> textureProxy, TileMode tileModeX,
                          TileMode tileModeY, const SamplingOptions& sampling,
                          const Matrix* localMatrix)
      : FragmentProcessorProxy(ClassID()),
        textureProxy(std::move(textureProxy)),
        tileModeX(tileModeX),
        tileModeY(tileModeY),
        sampling(sampling),
        localMatrix(localMatrix ? *localMatrix : Matrix::I()) {
  }

  std::string name() const override {
    return "TiledTextureEffectProxy";
  }

  std::unique_ptr<FragmentProcessor> instantiate() override {
    auto texture = textureProxy->getTexture();
    DEBUG_ASSERT(texture != nullptr);
    if ((tileModeX != TileMode::Clamp || tileModeY != TileMode::Clamp) && !texture->isYUV()) {
      return TiledTextureEffect::Make(std::move(texture),
                                      SamplerState(tileModeX, tileModeY, sampling), &localMatrix);
    }
    return TextureEffect::Make(std::move(texture), sampling, &localMatrix);
  }

  void onVisitProxies(const std::function<void(TextureProxy*)>& func) const override {
    func(textureProxy.get());
  }

  bool onIsEqual(const FragmentProcessor& fp) const override {
    const auto& that = static_cast<const TiledTextureEffectProxy&>(fp);
    return textureProxy == that.textureProxy && tileModeX == that.tileModeX &&
           tileModeY == that.tileModeY && sampling.filterMode == that.sampling.filterMode &&
           sampling.mipMapMode == that.sampling.mipMapMode && localMatrix == that.localMatrix;
  }

 private:
  DEFINE_PROCESSOR_CLASS_ID

  std::shared_ptr<TextureProxy> textureProxy;
  TileMode tileModeX;
  TileMode tileModeY;
  SamplingOptions sampling;
  Matrix localMatrix;
};

std::unique_ptr<FragmentProcessor> TiledTextureEffect::Make(
    std::shared_ptr<TextureProxy> textureProxy, TileMode tileModeX, TileMode tileModeY,
    const SamplingOptions& sampling, const Matrix* localMatrix) {
  if (textureProxy == nullptr) {
    return nullptr;
  }
  return std::make_unique<TiledTextureEffectProxy>(std::move(textureProxy), tileModeX, tileModeY,
                                                   sampling, localMatrix);
}

using Wrap = SamplerState::WrapMode;

struct TiledTextureEffect::Sampling {
  Sampling(const Texture* texture, SamplerState samplerState, const Rect& subset, const Caps* caps);

  SamplerState hwSampler;
  ShaderMode shaderModeX = ShaderMode::None;
  ShaderMode shaderModeY = ShaderMode::None;
  Rect shaderSubset = Rect::MakeEmpty();
  Rect shaderClamp = Rect::MakeEmpty();
};

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

TiledTextureEffect::Sampling::Sampling(const Texture* texture, SamplerState sampler,
                                       const Rect& subset, const Caps* caps) {
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

std::unique_ptr<FragmentProcessor> TiledTextureEffect::Make(std::shared_ptr<Texture> texture,
                                                            SamplerState samplerState,
                                                            const Matrix* localMatrix) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto matrix = localMatrix ? *localMatrix : Matrix::I();
  auto subset = Rect::MakeWH(texture->width(), texture->height());
  Sampling sampling(texture.get(), samplerState, subset, texture->getContext()->caps());
  return std::unique_ptr<TiledTextureEffect>(
      new TiledTextureEffect(std::move(texture), sampling, matrix));
}

TiledTextureEffect::TiledTextureEffect(std::shared_ptr<Texture> texture, const Sampling& sampling,
                                       const Matrix& localMatrix)
    : FragmentProcessor(ClassID()),
      texture(std::move(texture)),
      samplerState(sampling.hwSampler),
      subset(sampling.shaderSubset),
      clamp(sampling.shaderClamp),
      shaderModeX(sampling.shaderModeX),
      shaderModeY(sampling.shaderModeY),
      coordTransform(localMatrix, this->texture.get()) {
  setTextureSamplerCnt(1);
  addCoordTransform(&coordTransform);
}

void TiledTextureEffect::onComputeProcessorKey(BytesKey* bytesKey) const {
  auto flags = static_cast<uint32_t>(shaderModeX);
  flags |= static_cast<uint32_t>(shaderModeY) << 4;
  bytesKey->write(flags);
}

bool TiledTextureEffect::onIsEqual(const FragmentProcessor& processor) const {
  const auto& that = static_cast<const TiledTextureEffect&>(processor);
  return texture == that.texture && samplerState == that.samplerState && subset == that.subset &&
         clamp == that.clamp && shaderModeX == that.shaderModeX &&
         shaderModeY == that.shaderModeY && coordTransform.matrix == that.coordTransform.matrix;
}

std::unique_ptr<GLFragmentProcessor> TiledTextureEffect::onCreateGLInstance() const {
  return std::make_unique<GLTiledTextureEffect>();
}
}  // namespace tgfx
