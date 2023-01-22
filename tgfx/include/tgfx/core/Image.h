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

#include "tgfx/core/Cacheable.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/ImageGenerator.h"
#include "tgfx/core/ImageInfo.h"

namespace tgfx {
class TextureProxy;

class Context;

/**
 * Image describes a two dimensional array of pixels to draw. The pixels may be decoded in a
 * ImageBuffer, encoded in a Picture or compressed data stream, or located in GPU memory as a GPU
 * texture. Image is safe across threads and cannot be modified after it is created. The width and
 * height of Image are always greater than zero. Creating an Image with zero width or height returns
 * nullptr.
 */
class Image : public Cacheable {
 public:
  /**
   * Creates Image from encoded file. Image is returned if format of the encoded file is recognized
   * and supported. Recognized formats vary by platform.
   */
  static std::shared_ptr<Image> MakeFromEncoded(const std::string& filePath);

  /**
   * Creates Image from encoded data. Image is returned if format of the encoded data is recognized
   * and supported. Recognized formats vary by platform.
   */
  static std::shared_ptr<Image> MakeFromEncoded(std::shared_ptr<Data> encodedData);

  /**
   * Creates Image from an image generator. Image is returned if generator is not nullptr. Image
   * generator may wrap Picture data, codec data, or custom data.
   */
  static std::shared_ptr<Image> MakeFromGenerator(std::unique_ptr<ImageGenerator> imageGenerator);

  /**
   * Creates Image from ImageInfo, copying pixels. Image is returned if ImageInfo is not empty
   * and pixels is not nullptr. The returned Image may convert the copied pixels to a new format
   * which is more efficient for texture uploading on the GPU.
   */
  static std::shared_ptr<Image> MakeFromPixels(const ImageInfo& info, const void* pixels);

  /**
   * Creates Image from ImageBuffer, Image is returned if imageBuffer is not nullptr and its
   * dimensions are greater than zero.
   */
  static std::shared_ptr<Image> MakeFromBuffer(std::shared_ptr<ImageBuffer> imageBuffer);

  /**
   * Creates Image from Texture, Image is returned if texture is not nullptr.
   */
  static std::shared_ptr<Image> MakeFromTexture(std::shared_ptr<Texture> texture);

  virtual ~Image() = default;

  /**
   * Returns true if Image is backed by an image generator or other service that creates its pixels
   * on-demand.
   */
  virtual bool isLazyGenerated() const;

  /**
   * Returns true if the Image was created from a GPU texture.
   */
  virtual bool isTextureBacked() const;

  /**
   * Returns an Image backed by GPU texture associated with context. Returns original Image if
   * context is compatible with backing GPU texture. Returns nullptr if context is nullptr, or if
   * Image was created with another context.
   */
  std::shared_ptr<Image> makeTextureImage(Context* context) const;

 protected:
  virtual std::shared_ptr<Image> onMakeTextureImage(Context* context) const = 0;

  virtual std::shared_ptr<TextureProxy> onMakeTextureProxy(Context* context) const = 0;

 private:
  std::shared_ptr<TextureProxy> lockTextureProxy(Context* context) const;

  friend class Canvas;
};
}  // namespace tgfx
