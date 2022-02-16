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

#include "core/BlendMode.h"
#include "core/Color.h"
#include "core/ImageInfo.h"
#include "core/ImageOrigin.h"
#include "core/Matrix.h"
#include "core/Stroke.h"
#include "gpu/Backend.h"
#include "pag/file.h"
#include "pag/gpu.h"

namespace pag {

tgfx::BlendMode ToTGFXBlend(Enum blendMode);

tgfx::LineCap ToTGFXCap(Enum cap);

tgfx::LineJoin ToTGFXJoin(Enum join);

tgfx::Color ToTGFX(Color color, Opacity opacity = Opaque);

float ToAlpha(Opacity opacity);

tgfx::ImageOrigin ToTGFX(ImageOrigin origin);

ImageOrigin ToPAG(tgfx::ImageOrigin origin);

tgfx::AlphaType ToTGFX(AlphaType alphaType);

AlphaType ToPAG(tgfx::AlphaType alphaType);

tgfx::ColorType ToTGFX(ColorType colorType);

ColorType ToPAG(tgfx::ColorType colorType);

static inline tgfx::BackendTexture ToTGFX(const BackendTexture& texture) {
  return *reinterpret_cast<const tgfx::BackendTexture*>(&texture);
}

static inline BackendTexture ToPAG(const tgfx::BackendTexture& texture) {
  return *reinterpret_cast<const BackendTexture*>(&texture);
}

static inline tgfx::BackendRenderTarget ToTGFX(const BackendRenderTarget& renderTarget) {
  return *reinterpret_cast<const tgfx::BackendRenderTarget*>(&renderTarget);
}

static inline BackendRenderTarget ToPAG(const tgfx::BackendRenderTarget& renderTarget) {
  return *reinterpret_cast<const BackendRenderTarget*>(&renderTarget);
}

static inline tgfx::BackendSemaphore* ToTGFX(BackendSemaphore* semaphore) {
  return reinterpret_cast<tgfx::BackendSemaphore*>(semaphore);
}

static inline tgfx::BackendSemaphore ToTGFX(const BackendSemaphore& semaphore) {
  return *reinterpret_cast<const tgfx::BackendSemaphore*>(&semaphore);
}

static inline const tgfx::Matrix* ToTGFX(const Matrix* matrix) {
  return reinterpret_cast<const tgfx::Matrix*>(matrix);
}

static inline tgfx::Matrix* ToTGFX(Matrix* matrix) {
  return reinterpret_cast<tgfx::Matrix*>(matrix);
}

static inline tgfx::Matrix ToTGFX(const Matrix& matrix) {
  return *ToTGFX(&matrix);
}

static inline Matrix ToPAG(const tgfx::Matrix& matrix) {
  return *reinterpret_cast<const Matrix*>(&matrix);
}

static inline Matrix* ToPAG(tgfx::Matrix* matrix) {
  return reinterpret_cast<Matrix*>(matrix);
}

static inline const tgfx::Rect* ToTGFX(const Rect* rect) {
  return reinterpret_cast<const tgfx::Rect*>(rect);
}

static inline tgfx::Rect* ToTGFX(Rect* rect) {
  return reinterpret_cast<tgfx::Rect*>(rect);
}

static inline tgfx::Rect ToTGFX(const Rect& rect) {
  return *ToTGFX(&rect);
}

static inline const tgfx::Point* ToTGFX(const Point* point) {
  return reinterpret_cast<const tgfx::Point*>(point);
}

static inline Rect ToPAG(const tgfx::Rect& rect) {
  return *reinterpret_cast<const Rect*>(&rect);
}

static inline Rect* ToPAG(tgfx::Rect* rect) {
  return reinterpret_cast<Rect*>(rect);
}

static inline tgfx::Point* ToTGFX(Point* point) {
  return reinterpret_cast<tgfx::Point*>(point);
}

static inline tgfx::Point ToTGFX(const Point& point) {
  return {point.x, point.y};
}

static inline Point ToPAG(const tgfx::Point& point) {
  return {point.x, point.y};
}

static inline Point* ToPAG(tgfx::Point* point) {
  return reinterpret_cast<Point*>(point);
}

}  // namespace pag
