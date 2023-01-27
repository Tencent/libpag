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
#include "tgfx/core/Orientation.h"

namespace tgfx {

class ImageBuffer;

/**
 * Abstraction layer directly on top of an image codec.
 */
class ImageCodec : public ImageGenerator {
 public:
  /**
   * If this file path represents an encoded image that we know how to decode, return an ImageCodec
   * that can decode it. Otherwise return nullptr.
   */
  static std::shared_ptr<ImageCodec> MakeFrom(const std::string& filePath);

  /**
   * If this file bytes represents an encoded image that we know how to decode, return an ImageCodec
   * that can decode it. Otherwise return nullptr.
   */
  static std::shared_ptr<ImageCodec> MakeFrom(std::shared_ptr<Data> imageBytes);

  /**
   * Creates a new ImageCodec object from a native image. The type of nativeImage should be either
   * a jobject that represents a java Bitmap on android platform or a CGImageRef on the apple
   * platform. Returns nullptr if current platform has no native image support. The returned
   * ImageCodec object takes a reference on the nativeImage.
   */
  static std::shared_ptr<ImageCodec> MakeFrom(void* nativeImage);

  /**
   * Returns the orientation of target image.
   */
  Orientation orientation() const {
    return _orientation;
  }

  bool isAlphaOnly() const override {
    // TODO: implements alpha-only checking for image codecs.
    return false;
  }

  /**
   * Decodes the image with the specified image info into the given pixels. Returns true if the
   * decoding was successful.
   * Note: This method is not implemented on the web platform, use makeBuffer() instead if your
   * final goal is to make a texture out of the image.
   */
  virtual bool readPixels(const ImageInfo& dstInfo, void* dstPixels) const = 0;

 protected:
  ImageCodec(int width, int height, Orientation orientation)
      : ImageGenerator(width, height), _orientation(orientation) {
  }

  std::shared_ptr<ImageBuffer> onMakeBuffer(bool tryHardware) const override;

 private:
  Orientation _orientation = Orientation::TopLeft;

  /**
   * Encodes the specified pixels into a binary image format. Returns nullptr if encoding fails.
   */
  static std::shared_ptr<Data> Encode(const ImageInfo& info, const void* pixels,
                                      EncodedFormat format, int quality);

  friend class Bitmap;
};
}  // namespace tgfx
