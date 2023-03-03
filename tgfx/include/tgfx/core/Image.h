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
#include "tgfx/core/ImageOrigin.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/core/SamplingOptions.h"
#include "tgfx/core/TileMode.h"
#include "tgfx/platform/HardwareBuffer.h"
#include "tgfx/platform/NativeImage.h"

namespace tgfx {
class ImageSource;
class Context;
class TextureProxy;
class FragmentProcessor;

/**
 * Image describes a two-dimensional array of pixels to draw. The pixels may be decoded in an
 * ImageBuffer, encoded in a Picture or compressed data stream, or located in GPU memory as a GPU
 * texture. The Image class is safe across threads and cannot be modified after it is created. The
 * width and height of an Image are always greater than zero. Creating an Image with zero width or
 * height returns nullptr. The corresponding GPU cache is immediately marked as expired if all
 * Images with the same ImageSource are released, which becomes recyclable and will be purged at
 * some point in the future.
 */
class Image {
 public:
  /**
   * Creates Image from encoded file. Image is returned if the format of the encoded file is
   * recognized and supported. Recognized formats vary by platform. Note that the mipMapped
   * parameter may be ignored if it is not supported by the GPU or the associated image source.
   */
  static std::shared_ptr<Image> MakeFromEncoded(const std::string& filePath,
                                                bool mipMapped = false);

  /**
   * Creates Image from encoded data. An Image is returned if the format of the encoded data is
   * recognized and supported. Recognized formats vary by platform. Note that the mipMapped
   * parameter may be ignored if it is not supported by the GPU or the associated image source.
   */
  static std::shared_ptr<Image> MakeFromEncoded(std::shared_ptr<Data> encodedData,
                                                bool mipMapped = false);

  /**
   * Creates a new Image object from a platform-specific image in the CPU. The nativeImage could be
   * a jobject that represents a java Bitmap on the android platform or a CGImageRef on the apple
   * platform. The returned Image object takes a reference to the nativeImage. Returns nullptr if
   * the nativeImage is nullptr or the current platform has no native image support.
   */
  static std::shared_ptr<Image> MakeFromNativeImage(NativeImageRef nativeImage,
                                                    ImageOrigin origin = ImageOrigin::TopLeft,
                                                    bool mipMapped = false);

  /**
   * Creates Image from an image generator. An Image is returned if the generator is not nullptr.
   * The image generator may wrap codec data or custom data. Note that the mipMapped parameter may
   * be ignored if it is not supported by the GPU or the associated image source.
   */
  static std::shared_ptr<Image> MakeFromGenerator(std::shared_ptr<ImageGenerator> generator,
                                                  ImageOrigin origin = ImageOrigin::TopLeft,
                                                  bool mipMapped = false);

  /**
   * Creates Image from Pixmap and a copy of pixels. Since pixels are copied, Pixmap pixels may be
   * modified or deleted without affecting Image. The returned Image may convert the copied pixels
   * to a new format which is more efficient for texture uploading on the GPU. Note that the
   * mipMapped parameter may be ignored if it is not supported by the GPU or the associated image
   * source.
   */
  static std::shared_ptr<Image> MakeRasterCopy(const Pixmap& pixmap,
                                               ImageOrigin origin = ImageOrigin::TopLeft,
                                               bool mipMapped = false);

  /**
   * Creates Image from Pixmap, sharing pixels. The caller must ensure the pixmap will always be the
   * same and alive for the lifetime of the returned Image. Note that the mipMapped parameter may be
   * ignored if it is not supported by the GPU or the associated image source.
   */
  static std::shared_ptr<Image> MakeFromRaster(const Pixmap& pixmap,
                                               ImageOrigin origin = ImageOrigin::TopLeft,
                                               bool mipMapped = false);

  /**
   * Creates Image from ImageInfo, sharing pixels from the immutable Data object. The returned Image
   * takes a reference to the pixels. The caller must ensure the pixels will always be the same for
   * the lifetime of the returned Image. Returns nullptr if the ImageInfo is empty or the pixels are
   * nullptr. Note that the mipMapped parameter may be ignored if it is not supported by the GPU or
   * the associated image source.
   */
  static std::shared_ptr<Image> MakeRasterData(const ImageInfo& info, std::shared_ptr<Data> pixels,
                                               ImageOrigin origin = ImageOrigin::TopLeft,
                                               bool mipMapped = false);

  /**
   * Creates Image from Bitmap, sharing bitmap pixels. The Bitmap will allocate a new PixelBuffer
   * and copy the original pixels into it if there is a subsequent call of pixel writing to the
   * Bitmap. Therefore, the content of the returned Image always remains the same. Note that the
   * mipMapped parameter may be ignored if it is not supported by the GPU or the associated image
   * source.
   */
  static std::shared_ptr<Image> MakeFromBitmap(const Bitmap& bitmap,
                                               ImageOrigin origin = ImageOrigin::TopLeft,
                                               bool mipMapped = false);

  /**
   * Creates Image from a platform-specific hardware buffer. The hardwareBuffer could be an
   * AHardwareBuffer on the android platform or a CVPixelBufferRef on the apple platform. The
   * returned Image takes a reference to the hardwareBuffer. The caller must ensure that pixels are
   * unchanged for the lifetime of the returned Image. Returns nullptr if the hardwareBuffer is
   * nullptr or the hardwareBuffer is not single-plane. Note that the mipMapped parameter may be
   * ignored if it is not supported by the GPU or the associated image source.
   */
  static std::shared_ptr<Image> MakeFromHardwareBuffer(HardwareBufferRef hardwareBuffer,
                                                       ImageOrigin origin = ImageOrigin::TopLeft,
                                                       bool mipMapped = false);

  /**
   * Creates Image from ImageBuffer, Image is returned if imageBuffer is not nullptr and its
   * dimensions are greater than zero. Note that the mipMapped parameter may be ignored if it is not
   * supported by the GPU or the associated image source.
   */
  static std::shared_ptr<Image> MakeFromImageBuffer(std::shared_ptr<ImageBuffer> imageBuffer,
                                                    ImageOrigin origin = ImageOrigin::TopLeft,
                                                    bool mipMapped = false);

  /**
   * Creates Image from Texture, Image is returned if texture is not nullptr.
   */
  static std::shared_ptr<Image> MakeFromTexture(std::shared_ptr<Texture> texture,
                                                ImageOrigin origin = ImageOrigin::TopLeft);

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
   * Returns true if the Image is an RGBAAA image, which takes half of the original image as its RGB
   * channels and the other half as its alpha channel.
   */
  virtual bool isRGBAAA() const;

  /**
   * Returns true if Image is backed by an image generator or other services that create their
   * pixels on-demand.
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
   * Retrieves the backend texture. Returns nullptr if the Image is not backed by texture.
   */
  std::shared_ptr<Texture> getTexture() const;

  /**
   * Returns an Image backed by GPU texture associated with the specified context. If there is a
   * corresponding texture cache in the context, returns an Image wraps that texture. Otherwise,
   * creates one immediately, which may block the calling thread. Returns the original Image if the
   * Image is texture backed and the context is compatible with the backing GPU texture. Otherwise,
   * returns nullptr.
   */
  std::shared_ptr<Image> makeTextureImage(Context* context) const;

  /**
   * Returns subset of Image. The subset must be fully contained by Image dimensions. The
   * implementation always shares pixels and caches with the original Image. Returns nullptr if the
   * subset is empty, or the subset is not contained by bounds.
   */
  std::shared_ptr<Image> makeSubset(const Rect& subset) const;

  /**
   * Returns a decoded Image from the lazy Image. The returned Image shares the same texture cache
   * with the original Image and immediately schedules an asynchronous decoding task, which will not
   * block the calling thread. Returns the original Image if the Image is not lazy or has a
   * corresponding texture cache in the specified context.
   */
  std::shared_ptr<Image> makeDecoded(Context* context = nullptr) const;

  /**
   * Returns an Image with mipmaps enabled. Returns the original Image if the Image has mipmaps
   * enabled already or fails to enable mipmaps.
   */
  std::shared_ptr<Image> makeMipMapped() const;

  /**
   * Returns an Image with the RGBAAA layout that takes half of the original Image as its RGB
   * channels and the other half as its alpha channel. Returns a subset Image if both alphaStartX
   * and alphaStartY are zero. Returns nullptr if the original Image has an extra matrix or an
   * RGBAAA layout, or the specified layout is invalid.
   * @param displayWidth The display width of the RGBAAA image.
   * @param displayHeight The display height of the RGBAAA image.
   * @param alphaStartX The x position of where alpha area begins in the original image.
   * @param alphaStartY The y position of where alpha area begins in the original image.
   */
  std::shared_ptr<Image> makeRGBAAA(int displayWidth, int displayHeight, int alphaStartX,
                                    int alphaStartY);

 protected:
  std::weak_ptr<Image> weakThis;
  std::shared_ptr<ImageSource> source = nullptr;

  explicit Image(std::shared_ptr<ImageSource> source);

  virtual std::shared_ptr<Image> onCloneWithSource(std::shared_ptr<ImageSource> newSource) const;

  virtual std::shared_ptr<Image> onMakeSubset(const Rect& subset) const;

  virtual std::shared_ptr<Image> onMakeRGBAAA(int displayWidth, int displayHeight, int alphaStartX,
                                              int alphaStartY) const;

  virtual std::unique_ptr<FragmentProcessor> asFragmentProcessor(
      Context* context, TileMode tileModeX, TileMode tileModeY, const SamplingOptions& sampling,
      const Matrix* localMatrix = nullptr, uint32_t surfaceFlags = 0);

  std::unique_ptr<FragmentProcessor> asFragmentProcessor(Context* context,
                                                         const SamplingOptions& sampling,
                                                         const Matrix* localMatrix = nullptr,
                                                         uint32_t surfaceFlags = 0);

 private:
  static std::shared_ptr<Image> MakeFromSource(std::shared_ptr<ImageSource> source,
                                               ImageOrigin origin = ImageOrigin::TopLeft);

  std::shared_ptr<Image> cloneWithSource(std::shared_ptr<ImageSource> newSource) const;

  friend class Canvas;
};
}  // namespace tgfx
