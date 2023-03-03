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

#include "tgfx/core/ImageCodec.h"
#include "core/PixelBuffer.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/core/Stream.h"
#include "utils/USE.h"

#if defined(TGFX_USE_WEBP_DECODE) || defined(TGFX_USE_WEBP_ENCODE)
#include "codecs/webp/WebpCodec.h"
#endif

#if defined(TGFX_USE_PNG_DECODE) || defined(TGFX_USE_PNG_ENCODE)

#include "codecs/png/PngCodec.h"

#endif

#if defined(TGFX_USE_JPEG_DECODE) || defined(TGFX_USE_JPEG_ENCODE)
#include "codecs/jpeg/JpegCodec.h"
#endif

namespace tgfx {
std::shared_ptr<ImageCodec> ImageCodec::MakeFrom(const std::string& filePath) {
  std::shared_ptr<ImageCodec> codec = nullptr;
  auto stream = Stream::MakeFromFile(filePath);
  if (stream == nullptr || stream->size() <= 14) {
    return nullptr;
  }
  Buffer buffer(14);
  if (stream->read(buffer.data(), 14) < 14) {
    return nullptr;
  }
  auto data = buffer.release();
#ifdef TGFX_USE_WEBP_DECODE
  if (WebpCodec::IsWebp(data)) {
    codec = WebpCodec::MakeFrom(filePath);
  }
#endif

#ifdef TGFX_USE_PNG_DECODE
  if (PngCodec::IsPng(data)) {
    codec = PngCodec::MakeFrom(filePath);
  }
#endif

#ifdef TGFX_USE_JPEG_DECODE
  if (JpegCodec::IsJpeg(data)) {
    codec = JpegCodec::MakeFrom(filePath);
  }
#endif
  if (codec == nullptr) {
    codec = MakeNativeCodec(filePath);
  }
  if (codec && !ImageInfo::IsValidSize(codec->width(), codec->height())) {
    codec = nullptr;
  }
  return codec;
}

std::shared_ptr<ImageCodec> ImageCodec::MakeFrom(std::shared_ptr<Data> imageBytes) {
  if (imageBytes == nullptr || imageBytes->size() == 0) {
    return nullptr;
  }
  std::shared_ptr<ImageCodec> codec = nullptr;
#ifdef TGFX_USE_WEBP_DECODE
  if (WebpCodec::IsWebp(imageBytes)) {
    codec = WebpCodec::MakeFrom(imageBytes);
  }
#endif
#ifdef TGFX_USE_PNG_DECODE
  if (PngCodec::IsPng(imageBytes)) {
    codec = PngCodec::MakeFrom(imageBytes);
  }
#endif
#ifdef TGFX_USE_JPEG_DECODE
  if (JpegCodec::IsJpeg(imageBytes)) {
    codec = JpegCodec::MakeFrom(imageBytes);
  }
#endif
  if (codec == nullptr) {
    codec = MakeNativeCodec(imageBytes);
  }
  if (codec && !ImageInfo::IsValidSize(codec->width(), codec->height())) {
    codec = nullptr;
  }
  return codec;
}

std::shared_ptr<Data> ImageCodec::Encode(const Pixmap& pixmap, EncodedFormat format, int quality) {
  if (pixmap.isEmpty()) {
    return nullptr;
  }
  USE(format);
  if (quality > 100) {
    quality = 100;
  }
  if (quality < 0) {
    quality = 0;
  }
#ifdef TGFX_USE_JPEG_ENCODE
  if (format == EncodedFormat::JPEG) {
    return JpegCodec::Encode(pixmap, quality);
  }
#endif
#ifdef TGFX_USE_WEBP_ENCODE
  if (format == EncodedFormat::WEBP) {
    return WebpCodec::Encode(pixmap, quality);
  }
#endif
#ifdef TGFX_USE_PNG_ENCODE
  if (format == EncodedFormat::PNG) {
    return PngCodec::Encode(pixmap, quality);
  }
#endif
  return nullptr;
}

std::shared_ptr<ImageBuffer> ImageCodec::onMakeBuffer(bool tryHardware) const {
  auto pixelBuffer = PixelBuffer::Make(width(), height(), isAlphaOnly(), tryHardware);
  if (pixelBuffer == nullptr) {
    return nullptr;
  }
  auto pixels = pixelBuffer->lockPixels();
  auto result = readPixels(pixelBuffer->info(), pixels);
  pixelBuffer->unlockPixels();
  return result ? pixelBuffer : nullptr;
}
}  // namespace tgfx