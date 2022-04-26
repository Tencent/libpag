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

#include "tgfx/core/Image.h"
#include "core/utils/USE.h"
#include "platform/NativeCodec.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/PixelBuffer.h"
#include "tgfx/core/Stream.h"

#if defined(TGFX_USE_WEBP_DECODE) || defined(TGFX_USE_WEBP_ENCODE)
#include "core/images/webp/WebpImage.h"
#endif

#if defined(TGFX_USE_PNG_DECODE) || defined(TGFX_USE_PNG_ENCODE)

#include "core/images/png/PngImage.h"

#endif

#if defined(TGFX_USE_JPEG_DECODE) || defined(TGFX_USE_JPEG_ENCODE)
#include "core/images/jpeg/JpegImage.h"
#endif

namespace tgfx {
std::shared_ptr<Image> Image::MakeFrom(const std::string& filePath) {
  std::shared_ptr<Image> image = nullptr;
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
  if (WebpImage::IsWebp(data)) {
    image = WebpImage::MakeFrom(filePath);
  }
#endif

#ifdef TGFX_USE_PNG_DECODE
  if (PngImage::IsPng(data)) {
    image = PngImage::MakeFrom(filePath);
  }
#endif

#ifdef TGFX_USE_JPEG_DECODE
  if (JpegImage::IsJpeg(data)) {
    image = JpegImage::MakeFrom(filePath);
  }
#endif
  if (image == nullptr) {
    image = NativeCodec::MakeImage(filePath);
  }
  if (image && !ImageInfo::IsValidSize(image->width(), image->height())) {
    image = nullptr;
  }
  return image;
}

std::shared_ptr<Image> Image::MakeFrom(std::shared_ptr<Data> imageBytes) {
  if (imageBytes == nullptr || imageBytes->size() == 0) {
    return nullptr;
  }
  std::shared_ptr<Image> image = nullptr;
#ifdef TGFX_USE_WEBP_DECODE
  if (WebpImage::IsWebp(imageBytes)) {
    image = WebpImage::MakeFrom(imageBytes);
  }
#endif
#ifdef TGFX_USE_PNG_DECODE
  if (PngImage::IsPng(imageBytes)) {
    image = PngImage::MakeFrom(imageBytes);
  }
#endif
#ifdef TGFX_USE_JPEG_DECODE
  if (JpegImage::IsJpeg(imageBytes)) {
    image = JpegImage::MakeFrom(imageBytes);
  }
#endif
  if (image == nullptr) {
    image = NativeCodec::MakeImage(imageBytes);
  }
  if (image && !ImageInfo::IsValidSize(image->width(), image->height())) {
    image = nullptr;
  }
  return image;
}

std::shared_ptr<Image> Image::MakeFrom(void* nativeImage) {
  if (nativeImage == nullptr) {
    return nullptr;
  }
  return NativeCodec::MakeFrom(nativeImage);
}

std::shared_ptr<Data> Image::Encode(const ImageInfo& info, const void* pixels, EncodedFormat format,
                                    int quality) {
  if (info.isEmpty() || pixels == nullptr) {
    return nullptr;
  }
  USE(format);
  if (quality > 100) quality = 100;
  if (quality < 0) quality = 0;
#ifdef TGFX_USE_JPEG_ENCODE
  if (format == EncodedFormat::JPEG) {
    return JpegImage::Encode(info, pixels, format, quality);
  }
#endif
#ifdef TGFX_USE_WEBP_ENCODE
  if (format == EncodedFormat::WEBP) {
    return WebpImage::Encode(info, pixels, format, quality);
  }
#endif
#ifdef TGFX_USE_PNG_ENCODE
  if (format == EncodedFormat::PNG) {
    return PngImage::Encode(info, pixels, format, quality);
  }
#endif
  return nullptr;
}

std::shared_ptr<TextureBuffer> Image::makeBuffer() const {
  auto pixelBuffer = PixelBuffer::Make(width(), height(), false);
  if (pixelBuffer == nullptr) {
    return nullptr;
  }
  Bitmap bitmap(pixelBuffer);
  auto result = readPixels(pixelBuffer->info(), bitmap.writablePixels());
  return result ? pixelBuffer : nullptr;
}
}  // namespace tgfx