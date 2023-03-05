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

#include "tgfx/core/Data.h"
#include "tgfx/core/EncodedFormat.h"
#include "tgfx/core/ImageGenerator.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/ImageOrigin.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/platform/NativeImage.h"

namespace tgfx {

class ImageBuffer;

/**
 * Abstraction layer directly on top of an image codec.
 */
class ImageCodec : public ImageGenerator {
 public:
  /**
   * If this file path represents an encoded image that we know how to decode, return an ImageCodec
   * that can decode it. Otherwise, return nullptr.
   */
  static std::shared_ptr<ImageCodec> MakeFrom(const std::string& filePath);

  /**
   * If the file bytes represent an encoded image that we know how to decode, return an ImageCodec
   * that can decode it. Otherwise, return nullptr.
   */
  static std::shared_ptr<ImageCodec> MakeFrom(std::shared_ptr<Data> imageBytes);

  /**
   * Creates a new ImageCodec object from a platform-specific NativeImage. For example, the
   * NativeImage could be a jobject that represents a java Bitmap on the android platform or a
   * CGImageRef on the apple platform.The returned ImageCodec object takes a reference on the
   * nativeImage. Returns nullptr if the nativeImage is nullptr or the current platform has no
   * NativeImage support.
   */
  static std::shared_ptr<ImageCodec> MakeFrom(NativeImageRef nativeImage);

  /**
   * Encodes the specified Pixmap into a binary image format. Returns nullptr if encoding fails.
   */
  static std::shared_ptr<Data> Encode(const Pixmap& pixmap, EncodedFormat format, int quality);

  /**
   * Returns the origin of target image.
   */
  ImageOrigin origin() const {
    return _origin;
  }

  bool isAlphaOnly() const override {
    return false;
  }

  /**
   * Decodes the image with the specified image info into the given pixels. Returns true if the
   * decoding was successful. Note that we do not recommend calling this method due to performance
   * reasons, especially on the web platform. Use the makeBuffer() method for better performance if
   * your final goal is to draw the image.
   */
  virtual bool readPixels(const ImageInfo& dstInfo, void* dstPixels) const = 0;

 protected:
  ImageCodec(int width, int height, ImageOrigin origin)
      : ImageGenerator(width, height), _origin(origin) {
  }

  std::shared_ptr<ImageBuffer> onMakeBuffer(bool tryHardware) const override;

 private:
  ImageOrigin _origin = ImageOrigin::TopLeft;

  /**
   * If the file path represents an encoded image that the current platform knows how to decode,
   * returns an ImageCodec that can decode it. Otherwise returns nullptr.
   */
  static std::shared_ptr<ImageCodec> MakeNativeCodec(const std::string& filePath);

  /**
   * If the file bytes represents an encoded image that the current platform knows how to decode,
   * returns an ImageCodec that can decode it. Otherwise returns nullptr.
   */
  static std::shared_ptr<ImageCodec> MakeNativeCodec(std::shared_ptr<Data> imageBytes);

  friend class Pixmap;
};
}  // namespace tgfx
