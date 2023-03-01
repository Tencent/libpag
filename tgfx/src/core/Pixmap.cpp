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

#include "tgfx/core/Pixmap.h"
#include "core/PixelRef.h"
#include "skcms.h"

namespace tgfx {

static inline void* AddOffset(void* pixels, size_t offset) {
  return reinterpret_cast<uint8_t*>(pixels) + offset;
}

static inline const void* AddOffset(const void* pixels, size_t offset) {
  return reinterpret_cast<const uint8_t*>(pixels) + offset;
}

static void CopyRectMemory(const void* src, size_t srcRB, void* dst, size_t dstRB,
                           size_t trimRowBytes, int rowCount) {
  if (trimRowBytes == dstRB && trimRowBytes == srcRB) {
    memcpy(dst, src, trimRowBytes * rowCount);
    return;
  }
  for (int i = 0; i < rowCount; i++) {
    memcpy(dst, src, trimRowBytes);
    dst = AddOffset(dst, dstRB);
    src = AddOffset(src, srcRB);
  }
}

static const std::unordered_map<ColorType, gfx::skcms_PixelFormat> ColorMapper{
    {ColorType::RGBA_8888, gfx::skcms_PixelFormat::skcms_PixelFormat_RGBA_8888},
    {ColorType::BGRA_8888, gfx::skcms_PixelFormat::skcms_PixelFormat_BGRA_8888},
    {ColorType::ALPHA_8, gfx::skcms_PixelFormat::skcms_PixelFormat_A_8},
};

static const std::unordered_map<AlphaType, gfx::skcms_AlphaFormat> AlphaMapper{
    {AlphaType::Unpremultiplied, gfx::skcms_AlphaFormat::skcms_AlphaFormat_Unpremul},
    {AlphaType::Premultiplied, gfx::skcms_AlphaFormat::skcms_AlphaFormat_PremulAsEncoded},
    {AlphaType::Opaque, gfx::skcms_AlphaFormat::skcms_AlphaFormat_Opaque},
};

static void ConvertPixels(const ImageInfo& srcInfo, const void* srcPixels, const ImageInfo& dstInfo,
                          void* dstPixels) {
  if (srcInfo.colorType() == dstInfo.colorType() && srcInfo.alphaType() == dstInfo.alphaType()) {
    CopyRectMemory(srcPixels, srcInfo.rowBytes(), dstPixels, dstInfo.rowBytes(),
                   dstInfo.minRowBytes(), dstInfo.height());
    return;
  }
  auto srcFormat = ColorMapper.at(srcInfo.colorType());
  auto srcAlpha = AlphaMapper.at(srcInfo.alphaType());
  auto dstFormat = ColorMapper.at(dstInfo.colorType());
  auto dstAlpha = AlphaMapper.at(dstInfo.alphaType());
  auto width = dstInfo.width();
  auto height = dstInfo.height();
  for (int i = 0; i < height; i++) {
    gfx::skcmsTransform(srcPixels, srcFormat, srcAlpha, nullptr, dstPixels, dstFormat, dstAlpha,
                        nullptr, width);
    dstPixels = AddOffset(dstPixels, dstInfo.rowBytes());
    srcPixels = AddOffset(srcPixels, srcInfo.rowBytes());
  }
}

Pixmap::Pixmap(const ImageInfo& info, const void* pixels) : _info(info), _pixels(pixels) {
  if (_info.isEmpty() || _pixels == nullptr) {
    _info = {};
    _pixels = nullptr;
  }
}

Pixmap::Pixmap(const ImageInfo& info, void* pixels)
    : _info(info), _pixels(pixels), _writablePixels(pixels) {
  if (_info.isEmpty() || _pixels == nullptr) {
    _info = {};
    _pixels = nullptr;
    _writablePixels = nullptr;
  }
}

Pixmap::Pixmap(const Bitmap& bitmap) {
  _pixels = bitmap.lockPixels();
  if (_pixels == nullptr) {
    return;
  }
  pixelRef = bitmap.pixelRef;
  _info = pixelRef->info();
}

Pixmap::Pixmap(Bitmap& bitmap) {
  _writablePixels = bitmap.lockPixels();
  if (_writablePixels == nullptr) {
    return;
  }
  pixelRef = bitmap.pixelRef;
  _pixels = _writablePixels;
  _info = pixelRef->info();
}

Pixmap::~Pixmap() {
  reset();
}

void Pixmap::reset() {
  if (pixelRef != nullptr) {
    pixelRef->unlockPixels();
    pixelRef = nullptr;
  }
  _pixels = nullptr;
  _info = {};
}

void Pixmap::reset(const ImageInfo& info, const void* pixels) {
  reset();
  if (!info.isEmpty() && pixels != nullptr) {
    _info = info;
    _pixels = pixels;
  }
}

void Pixmap::reset(const ImageInfo& info, void* pixels) {
  reset();
  if (!info.isEmpty() && pixels != nullptr) {
    _info = info;
    _pixels = pixels;
    _writablePixels = pixels;
  }
}

Color Pixmap::getColor(int x, int y) const {
  auto dstInfo = ImageInfo::Make(1, 1, ColorType::RGBA_8888, AlphaType::Unpremultiplied, 4);
  uint8_t color[4];
  if (!readPixels(dstInfo, color, x, y)) {
    return Color::Transparent();
  }
  return Color::FromRGBA(color[0], color[1], color[2], color[3]);
}

Pixmap Pixmap::makeSubset(const Rect& subset) const {
  auto rect = subset;
  rect.round();
  auto bounds = Rect::MakeWH(width(), height());
  if (bounds == rect) {
    return *this;
  }
  if (!bounds.contains(rect)) {
    return {};
  }
  auto srcX = static_cast<int>(rect.x());
  auto srcY = static_cast<int>(rect.y());
  auto width = static_cast<int>(rect.width());
  auto height = static_cast<int>(rect.height());
  auto srcPixels = _info.computeOffset(_pixels, srcX, srcY);
  auto srcInfo = _info.makeWH(width, height);
  return {srcInfo, srcPixels};
}

bool Pixmap::readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX, int srcY) const {
  if (_pixels == nullptr || dstPixels == nullptr) {
    return false;
  }
  auto imageInfo = dstInfo.makeIntersect(-srcX, -srcY, _info.width(), _info.height());
  if (imageInfo.isEmpty()) {
    return false;
  }
  auto srcPixels = _info.computeOffset(_pixels, srcX, srcY);
  auto srcInfo = _info.makeWH(imageInfo.width(), imageInfo.height());
  dstPixels = imageInfo.computeOffset(dstPixels, -srcX, -srcY);
  ConvertPixels(srcInfo, srcPixels, imageInfo, dstPixels);
  return true;
}

bool Pixmap::writePixels(const ImageInfo& srcInfo, const void* srcPixels, int dstX, int dstY) {
  if (_writablePixels == nullptr || srcPixels == nullptr) {
    return false;
  }
  auto imageInfo = srcInfo.makeIntersect(-dstX, -dstY, _info.width(), _info.height());
  if (imageInfo.isEmpty()) {
    return false;
  }
  srcPixels = imageInfo.computeOffset(srcPixels, -dstX, -dstY);
  auto dstPixels = _info.computeOffset(_writablePixels, dstX, dstY);
  auto dstInfo = _info.makeWH(imageInfo.width(), imageInfo.height());
  ConvertPixels(imageInfo, srcPixels, dstInfo, dstPixels);
  return true;
}

bool Pixmap::eraseAll() {
  if (_writablePixels == nullptr) {
    return false;
  }
  if (_info.rowBytes() == _info.minRowBytes()) {
    memset(_writablePixels, 0, byteSize());
  } else {
    auto rowCount = _info.height();
    auto trimRowBytes = _info.width() * _info.bytesPerPixel();
    auto* pixels = static_cast<char*>(_writablePixels);
    for (int i = 0; i < rowCount; i++) {
      memset(pixels, 0, trimRowBytes);
      pixels += info().rowBytes();
    }
  }
  return true;
}

}  // namespace tgfx
