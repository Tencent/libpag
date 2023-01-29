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
#include "tgfx/core/ImageGenerator.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/Orientation.h"
#include "tgfx/core/SamplingOptions.h"
#include "tgfx/core/TileMode.h"

namespace tgfx {
class ImageSource;
class Context;
class TextureProxy;
class FragmentProcessor;

/**
 * Image describes a two dimensional array of pixels to draw. The pixels may be decoded in a
 * ImageBuffer, encoded in a Picture or compressed data stream, or located in GPU memory as a GPU
 * texture. Image is safe across threads and cannot be modified after it is created. The width and
 * height of Image are always greater than zero. Creating an Image with zero width or height returns
 * nullptr.
 */
class Image {
 public:
  /**
   * Creates Image from encoded file. Image is returned if format of the encoded file is recognized
   * and supported. Recognized formats vary by platform. The mipMapped parameter may be ignored if
   * it is not supported by the GPU or the associated image source.
   */
  static std::shared_ptr<Image> MakeFromEncoded(const std::string& filePath,
                                                bool mipMapped = false);

  /**
   * Creates Image from encoded data. Image is returned if format of the encoded data is recognized
   * and supported. Recognized formats vary by platform. The mipMapped parameter may be ignored if
   * it is not supported by the GPU or the associated image source.
   */
  static std::shared_ptr<Image> MakeFromEncoded(std::shared_ptr<Data> encodedData,
                                                bool mipMapped = false);

  /**
   * Creates Image from an image generator. Image is returned if generator is not nullptr. Image
   * generator may wrap codec data or custom data. The mipMapped parameter may be ignored if it is
   * not supported by the GPU or the associated image source.
   */
  static std::shared_ptr<Image> MakeFromGenerator(std::shared_ptr<ImageGenerator> generator,
                                                  Orientation orientation = Orientation::TopLeft,
                                                  bool mipMapped = false);

  /**
   * Creates Image from ImageInfo, copying pixels. Image is returned if ImageInfo is not empty
   * and pixels is not nullptr. The returned Image may convert the copied pixels to a new format
   * which is more efficient for texture uploading on the GPU. The mipMapped parameter may be
   * ignored if it is not supported by the GPU or the associated image source.
   */
  static std::shared_ptr<Image> MakeFromPixels(const ImageInfo& info, const void* pixels,
                                               Orientation orientation = Orientation::TopLeft,
                                               bool mipMapped = false);

  /**
   * Creates Image from ImageBuffer, Image is returned if imageBuffer is not nullptr and its
   * dimensions are greater than zero. The mipMapped parameter may be ignored if it is not supported
   * by the GPU or the associated image source.
   */
  static std::shared_ptr<Image> MakeFromBuffer(std::shared_ptr<ImageBuffer> imageBuffer,
                                               Orientation orientation = Orientation::TopLeft,
                                               bool mipMapped = false);

  /**
   * Creates Image from Texture, Image is returned if texture is not nullptr.
   */
  static std::shared_ptr<Image> MakeFromTexture(std::shared_ptr<Texture> texture,
                                                Orientation orientation = Orientation::TopLeft);

  /**
   * Creates an Image with RGBAAA layout that takes half of the original image from imageGenerator
   * as its RGB channels and the other half as its alpha channel. Returns a non-RGBAAA Image if both
   * alphaStartX and alphaStartY are zero. Returns nullptr if imageGenerator is nullptr or the
   * RGBAAA layout is not contained by the bounds of the original image.
   * @param imageGenerator An ImageGenerator generates the original image.
   * @param displayWidth The display width of the RGBAAA image.
   * @param displayHeight The display height of the RGBAAA image.
   * @param alphaStartX The x position of where alpha area begins in the original image.
   * @param alphaStartY The y position of where alpha area begins in the original image.
   * @param mipMapped Whether created Image texture must allocate mip map levels.
   */
  static std::shared_ptr<Image> MakeRGBAAA(std::shared_ptr<ImageGenerator> imageGenerator,
                                           int displayWidth, int displayHeight, int alphaStartX,
                                           int alphaStartY, bool mipMapped = false);

  /**
   * Creates an Image with RGBAAA layout that takes half of the original image from imageBuffer as
   * its RGB channels and the other half as its alpha channel. Returns a non-RGBAAA Image if
   * both alphaStartX and alphaStartY are zero. Returns nullptr if imageBuffer is nullptr or the
   * RGBAAA layout is not contained by the bounds of the original image.
   * @param imageBuffer An ImageBuffer represents the original image.
   * @param displayWidth The display width of the RGBAAA image.
   * @param displayHeight The display height of the RGBAAA image.
   * @param alphaStartX The x position of where alpha area begins in the original image.
   * @param alphaStartY The y position of where alpha area begins in the original image.
   * @param mipMapped whether created Image texture must allocate mip map levels.
   */
  static std::shared_ptr<Image> MakeRGBAAA(std::shared_ptr<ImageBuffer> imageBuffer,
                                           int displayWidth, int displayHeight, int alphaStartX,
                                           int alphaStartY, bool mipMapped = false);

  /**
   * Creates an Image with RGBAAA layout that takes half of the pixels in texture as its RGB
   * channels and the other half as its alpha channel. Returns a non-RGBAAA Image if both
   * alphaStartX and alphaStartY are zero. Returns nullptr if imageBuffer is nullptr or the
   * RGBAAA layout is not contained by the bounds of the original image.
   * @param texture A Texture represents the original image.
   * @param displayWidth The display width of the RGBAAA image.
   * @param displayHeight The display height of the RGBAAA image.
   * @param alphaStartX The x position of where alpha area begins in the original image.
   * @param alphaStartY The y position of where alpha area begins in the original image.
   */
  static std::shared_ptr<Image> MakeRGBAAA(std::shared_ptr<Texture> texture, int displayWidth,
                                           int displayHeight, int alphaStartX, int alphaStartY);

  virtual ~Image() = default;

  /**
   * Returns the width of the Image.
   */
  virtual int width() const;

  /**
   * Returns pixel row count.
   */
  virtual int height() const;

  /**
   * Returns true if pixels represent transparency only. If true, each pixel is packed in 8 bits as
   * defined by ColorType::ALPHA_8.
   */
  bool isAlphaOnly() const;

  /**
   * Returns true if Image is backed by an image generator or other service that creates its pixels
   * on-demand.
   */
  bool isLazyGenerated() const;

  /**
   * Returns true if the Image was created from a GPU texture.
   */
  bool isTextureBacked() const;

  /**
   * Returns true if the Image has mipmap levels. The value was passed in when creating the Image.
   * It may be ignored if mipmaps are not supported by the GPU or the associated image source.
   */
  bool hasMipmaps() const;

  /**
   * Retrieves the backend texture. Returns nullptr if Image is not texture backed.
   */
  std::shared_ptr<Texture> getTexture() const;

  /**
   * Returns a decoded Image from the lazy Image. The returned Image shares the same texture cache
   * with the original Image and immediately schedules an asynchronous decoding task, which will not
   * block the calling thread. Returns nullptr if the Image is not lazy or has a corresponding
   * texture cache in the specified context.
   */
  std::shared_ptr<Image> makeDecodedImage(Context* context = nullptr) const;

  /**
   * Returns an Image backed by GPU texture associated with the specified context. If there is a
   * corresponding texture cache in the context, returns an Image wraps that texture. Otherwise,
   * creates one immediately if wrapCacheOnly is false, which may block the calling thread. Returns
   * original Image if the Image is texture backed and the context is compatible with the backing
   * GPU texture. Otherwise, returns nullptr.
   */
  std::shared_ptr<Image> makeTextureImage(Context* context, bool wrapCacheOnly = false) const;

  /**
   * Returns subset of Image. subset must be fully contained by Image dimensions. The implementation
   * may share pixels, or may copy them. Returns nullptr if subset is empty, or subset is not
   * contained by bounds.
   */
  std::shared_ptr<Image> makeSubset(const Rect& subset) const;

 protected:
  std::weak_ptr<Image> weakThis;
  std::shared_ptr<ImageSource> source = nullptr;

  explicit Image(std::shared_ptr<ImageSource> source);

  virtual std::shared_ptr<Image> onCloneWithSource(std::shared_ptr<ImageSource> newSource) const;

  virtual std::shared_ptr<Image> onMakeSubset(const Rect& subset) const;

  virtual std::unique_ptr<FragmentProcessor> asFragmentProcessor(
      Context* context, TileMode tileModeX, TileMode tileModeY, const SamplingOptions& sampling,
      const Matrix* localMatrix = nullptr);

  std::unique_ptr<FragmentProcessor> asFragmentProcessor(Context* context,
                                                         const SamplingOptions& sampling,
                                                         const Matrix* localMatrix = nullptr);

 private:
  static std::shared_ptr<Image> MakeFromSource(std::shared_ptr<ImageSource> source,
                                               Orientation orientation = Orientation::TopLeft);

  static std::shared_ptr<Image> MakeRGBAAA(std::shared_ptr<ImageSource> source, int displayWidth,
                                           int displayHeight, int alphaStartX, int alphaStartY);

  friend class Canvas;
};
}  // namespace tgfx
