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
  // Runtime dispatch on the backend tag so a Metal / GL binary can each accept the type it
  // supports without pretending to translate one into the other. tgfx's BackendTexture holds
  // per-backend info in a union and its constructors are backend-inline in Backend.h, so we can
  // safely emit any variant here — the actual GPU-facing consumers (tgfx::Image, tgfx::Surface,
  // etc.) only accept the info matching the compiled backend.
  switch (texture.backend()) {
    case Backend::OPENGL: {
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
    case Backend::METAL: {
      MtlTextureInfo mtlInfo = {};
      if (!texture.getMtlTextureInfo(&mtlInfo)) {
        return {};
      }
      tgfx::MetalTextureInfo sampler = {};
      sampler.texture = mtlInfo.texture;
      sampler.format = mtlInfo.format;
      return tgfx::BackendTexture{sampler, texture.width(), texture.height()};
    }
    case Backend::VULKAN:
    case Backend::MOCK:
    default:
      return {};
  }
}

BackendTexture ToPAG(const tgfx::BackendTexture& texture) {
  switch (texture.backend()) {
    case tgfx::Backend::OpenGL: {
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
    case tgfx::Backend::Metal: {
      tgfx::MetalTextureInfo mtlInfo = {};
      if (!texture.getMetalTextureInfo(&mtlInfo)) {
        return {};
      }
      MtlTextureInfo sampler = {};
      // tgfx::MetalTextureInfo::texture is const void* (immutable view of id<MTLTexture>);
      // pag::MtlTextureInfo predates that and uses void*. The pointer is treated as an opaque
      // handle by libpag — the const_cast is safe because no writer path exists downstream.
      sampler.texture = const_cast<void*>(mtlInfo.texture);
      sampler.format = mtlInfo.format;
      return {sampler, texture.width(), texture.height()};
    }
    default:
      return {};
  }
}

tgfx::BackendRenderTarget ToTGFX(const BackendRenderTarget& renderTarget) {
  switch (renderTarget.backend()) {
    case Backend::OPENGL: {
      GLFrameBufferInfo glInfo = {};
      if (!renderTarget.getGLFramebufferInfo(&glInfo)) {
        return {};
      }
      tgfx::GLFrameBufferInfo frameBuffer = {};
      frameBuffer.id = glInfo.id;
      frameBuffer.format = glInfo.format;
      return tgfx::BackendRenderTarget(frameBuffer, renderTarget.width(), renderTarget.height());
    }
    case Backend::METAL: {
      MtlTextureInfo mtlInfo = {};
      if (!renderTarget.getMtlTextureInfo(&mtlInfo)) {
        return {};
      }
      tgfx::MetalTextureInfo sampler = {};
      sampler.texture = mtlInfo.texture;
      sampler.format = mtlInfo.format;
      return tgfx::BackendRenderTarget(sampler, renderTarget.width(), renderTarget.height());
    }
    case Backend::VULKAN:
    case Backend::MOCK:
    default:
      return {};
  }
}

tgfx::BackendSemaphore ToTGFX(const BackendSemaphore& semaphore) {
  // Dispatch on the pag-side backend tag so the returned tgfx::BackendSemaphore carries the
  // right variant. Non-initialized semaphores return a default-constructed tgfx one, which is
  // a no-op at the tgfx layer. Vulkan / D3D12 / WebGPU sync ABI extensions are deferred to
  // the specific backend PRs that introduce user-facing sync APIs.
  if (!semaphore.isInitialized()) {
    return {};
  }
  if (auto glSync = semaphore.glSync()) {
    tgfx::GLSyncInfo syncInfo = {glSync};
    return tgfx::BackendSemaphore(syncInfo);
  }
  if (auto mtlEvent = semaphore.mtlEvent()) {
    tgfx::MetalSyncInfo syncInfo = {mtlEvent, semaphore.mtlValue()};
    return tgfx::BackendSemaphore(syncInfo);
  }
  return {};
}
}  // namespace pag
