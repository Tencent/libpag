/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "TGFXCast.h"

namespace pag {
static constexpr std::pair<BlendMode, tgfx::BlendMode> BlendModeMap[] = {
    {BlendMode::Normal, tgfx::BlendMode::SrcOver},
    {BlendMode::Multiply, tgfx::BlendMode::Multiply},
    {BlendMode::Screen, tgfx::BlendMode::Screen},
    {BlendMode::Overlay, tgfx::BlendMode::Overlay},
    {BlendMode::Darken, tgfx::BlendMode::Darken},
    {BlendMode::Lighten, tgfx::BlendMode::Lighten},
    {BlendMode::ColorDodge, tgfx::BlendMode::ColorDodge},
    {BlendMode::ColorBurn, tgfx::BlendMode::ColorBurn},
    {BlendMode::HardLight, tgfx::BlendMode::HardLight},
    {BlendMode::SoftLight, tgfx::BlendMode::SoftLight},
    {BlendMode::Difference, tgfx::BlendMode::Difference},
    {BlendMode::Exclusion, tgfx::BlendMode::Exclusion},
    {BlendMode::Hue, tgfx::BlendMode::Hue},
    {BlendMode::Saturation, tgfx::BlendMode::Saturation},
    {BlendMode::Color, tgfx::BlendMode::Color},
    {BlendMode::Luminosity, tgfx::BlendMode::Luminosity},
    {BlendMode::Add, tgfx::BlendMode::PlusLighter}};

tgfx::BlendMode ToTGFX(BlendMode blendMode) {
  for (const auto& pair : BlendModeMap) {
    if (pair.first == blendMode) {
      return pair.second;
    }
  }
  return tgfx::BlendMode::SrcOver;
}

tgfx::LineCap ToTGFX(LineCap cap) {
  switch (cap) {
    case LineCap::Round:
      return tgfx::LineCap::Round;
    case LineCap::Square:
      return tgfx::LineCap::Square;
    default:
      return tgfx::LineCap::Butt;
  }
}

tgfx::LineJoin ToTGFX(LineJoin join) {
  switch (join) {
    case LineJoin::Round:
      return tgfx::LineJoin::Round;
    case LineJoin::Bevel:
      return tgfx::LineJoin::Bevel;
    default:
      return tgfx::LineJoin::Miter;
  }
}

tgfx::Color ToTGFX(Color color, Opacity opacity) {
  return {static_cast<float>(color.red) / 255.0f, static_cast<float>(color.green) / 255.0f,
          static_cast<float>(color.blue) / 255.0f, ToAlpha(opacity)};
}

float ToAlpha(Opacity opacity) {
  if (opacity == 255) {
    return 1.0f;
  }
  return static_cast<float>(opacity) / 255.0f;
}

tgfx::ImageOrigin ToTGFX(ImageOrigin origin) {
  return origin == ImageOrigin::TopLeft ? tgfx::ImageOrigin::TopLeft
                                        : tgfx::ImageOrigin::BottomLeft;
}

ImageOrigin ToPAG(tgfx::ImageOrigin origin) {
  return origin == tgfx::ImageOrigin::TopLeft ? ImageOrigin::TopLeft : ImageOrigin::BottomLeft;
}

tgfx::AlphaType ToTGFX(AlphaType alphaType) {
  switch (alphaType) {
    case AlphaType::Opaque:
      return tgfx::AlphaType::Opaque;
    case AlphaType::Premultiplied:
      return tgfx::AlphaType::Premultiplied;
    case AlphaType::Unpremultiplied:
      return tgfx::AlphaType::Unpremultiplied;
    default:
      return tgfx::AlphaType::Unknown;
  }
}

AlphaType ToPAG(tgfx::AlphaType alphaType) {
  switch (alphaType) {
    case tgfx::AlphaType::Opaque:
      return AlphaType::Opaque;
    case tgfx::AlphaType::Premultiplied:
      return AlphaType::Premultiplied;
    case tgfx::AlphaType::Unpremultiplied:
      return AlphaType::Unpremultiplied;
    default:
      return AlphaType::Unknown;
  }
}

tgfx::ColorType ToTGFX(ColorType colorType) {
  switch (colorType) {
    case ColorType::ALPHA_8:
      return tgfx::ColorType::ALPHA_8;
    case ColorType::RGBA_8888:
      return tgfx::ColorType::RGBA_8888;
    case ColorType::BGRA_8888:
      return tgfx::ColorType::BGRA_8888;
    case ColorType::RGB_565:
      return tgfx::ColorType::RGB_565;
    case ColorType::Gray_8:
      return tgfx::ColorType::Gray_8;
    case ColorType::RGBA_F16:
      return tgfx::ColorType::RGBA_F16;
    case ColorType::RGBA_1010102:
      return tgfx::ColorType::RGBA_1010102;
    default:
      return tgfx::ColorType::Unknown;
  }
}

ColorType ToPAG(tgfx::ColorType colorType) {
  switch (colorType) {
    case tgfx::ColorType::ALPHA_8:
      return ColorType::ALPHA_8;
    case tgfx::ColorType::RGBA_8888:
      return ColorType::RGBA_8888;
    case tgfx::ColorType::BGRA_8888:
      return ColorType::BGRA_8888;
    default:
      return ColorType::Unknown;
  }
}

tgfx::BackendTexture ToTGFX(const BackendTexture& texture) {
  GLTextureInfo glInfo = {};
  if (!texture.getGLTextureInfo(&glInfo)) {
    return {};
  }
  tgfx::GLTextureInfo sampler = {};
  sampler.id = glInfo.id;
  sampler.target = glInfo.target;
  sampler.format = glInfo.format;
  return tgfx::BackendTexture{sampler, texture.width(), texture.height()};
}

BackendTexture ToPAG(const tgfx::BackendTexture& texture) {
  tgfx::GLTextureInfo glInfo = {};
  if (!texture.getGLTextureInfo(&glInfo)) {
    return {};
  }
  GLTextureInfo sampler = {};
  sampler.id = glInfo.id;
  sampler.target = glInfo.target;
  sampler.format = glInfo.format;
  return {sampler, texture.width(), texture.height()};
}

tgfx::BackendRenderTarget ToTGFX(const BackendRenderTarget& renderTarget) {
  GLFrameBufferInfo glInfo = {};
  if (!renderTarget.getGLFramebufferInfo(&glInfo)) {
    return {};
  }
  tgfx::GLFrameBufferInfo frameBuffer = {};
  frameBuffer.id = glInfo.id;
  frameBuffer.format = glInfo.format;
  return {frameBuffer, renderTarget.width(), renderTarget.height()};
}

tgfx::BackendSemaphore ToTGFX(const BackendSemaphore& semaphore) {
  tgfx::GLSyncInfo syncInfo = {semaphore.glSync()};
  tgfx::BackendSemaphore glSemaphore = {syncInfo};
  return glSemaphore;
}
}  // namespace pag
