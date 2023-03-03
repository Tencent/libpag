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
#include "gpu/TextureEffect.h"
#include "gpu/TiledTextureEffect.h"
#include "images/ImageSource.h"
#include "images/MatrixImage.h"
#include "images/RGBAAAImage.h"
#include "images/RasterBuffer.h"
#include "images/RasterGenerator.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"

namespace tgfx {
std::shared_ptr<Image> Image::MakeFromEncoded(const std::string& filePath, bool mipMapped) {
  auto codec = ImageCodec::MakeFrom(filePath);
  if (codec == nullptr) {
    return nullptr;
  }
  return MakeFromGenerator(codec, codec->origin(), mipMapped);
}

std::shared_ptr<Image> Image::MakeFromEncoded(std::shared_ptr<Data> encodedData, bool mipMapped) {
  auto codec = ImageCodec::MakeFrom(std::move(encodedData));
  if (codec == nullptr) {
    return nullptr;
  }
  return MakeFromGenerator(codec, codec->origin(), mipMapped);
}

std::shared_ptr<Image> Image::MakeFromNativeImage(NativeImageRef nativeImage, ImageOrigin origin,
                                                  bool mipMapped) {
  auto codec = ImageCodec::MakeFrom(nativeImage);
  return MakeFromGenerator(std::move(codec), origin, mipMapped);
}

std::shared_ptr<Image> Image::MakeFromGenerator(std::shared_ptr<ImageGenerator> generator,
                                                ImageOrigin origin, bool mipMapped) {
  auto source = ImageSource::MakeFromGenerator(generator, mipMapped);
  return MakeFromSource(std::move(source), origin);
}

std::shared_ptr<Image> Image::MakeRasterCopy(const Pixmap& pixmap, ImageOrigin origin,
                                             bool mipMapped) {
  if (pixmap.isEmpty()) {
    return nullptr;
  }
  Bitmap bitmap(pixmap.width(), pixmap.height(), pixmap.isAlphaOnly());
  bitmap.writePixels(pixmap.info(), pixmap.pixels());
  return MakeFromBitmap(bitmap, origin, mipMapped);
}

std::shared_ptr<Image> Image::MakeFromRaster(const Pixmap& pixmap, ImageOrigin origin,
                                             bool mipMapped) {
  auto data = Data::MakeWithoutCopy(pixmap.pixels(), pixmap.byteSize());
  return MakeRasterData(pixmap.info(), std::move(data), origin, mipMapped);
}

std::shared_ptr<Image> Image::MakeRasterData(const ImageInfo& info, std::shared_ptr<Data> pixels,
                                             ImageOrigin origin, bool mipMapped) {
  auto imageBuffer = RasterBuffer::MakeFrom(info, pixels);
  if (imageBuffer != nullptr) {
    return MakeFromImageBuffer(std::move(imageBuffer), origin, mipMapped);
  }
  auto imageGenerator = RasterGenerator::MakeFrom(info, pixels);
  return MakeFromGenerator(std::move(imageGenerator), origin, mipMapped);
}

std::shared_ptr<Image> Image::MakeFromBitmap(const Bitmap& bitmap, ImageOrigin origin,
                                             bool mipMapped) {
  return MakeFromImageBuffer(bitmap.makeBuffer(), origin, mipMapped);
}

std::shared_ptr<Image> Image::MakeFromHardwareBuffer(HardwareBufferRef hardwareBuffer,
                                                     ImageOrigin origin, bool mipMapped) {
  auto buffer = ImageBuffer::MakeFrom(hardwareBuffer);
  return MakeFromImageBuffer(std::move(buffer), origin, mipMapped);
}

std::shared_ptr<Image> Image::MakeFromImageBuffer(std::shared_ptr<ImageBuffer> imageBuffer,
                                                  ImageOrigin origin, bool mipMapped) {
  auto source = ImageSource::MakeFromBuffer(std::move(imageBuffer), mipMapped);
  return MakeFromSource(std::move(source), origin);
}

std::shared_ptr<Image> Image::MakeFromTexture(std::shared_ptr<Texture> texture,
                                              ImageOrigin origin) {
  auto source = ImageSource::MakeFromTexture(std::move(texture));
  return MakeFromSource(std::move(source), origin);
}

std::shared_ptr<Image> Image::MakeFromSource(std::shared_ptr<ImageSource> source,
                                             ImageOrigin origin) {
  if (source == nullptr) {
    return nullptr;
  }
  std::shared_ptr<Image> image = nullptr;
  if (origin != ImageOrigin::TopLeft) {
    auto matrix = ImageOriginToMatrix(origin, source->width(), source->height());
    matrix.invert(&matrix);
    auto width = source->width();
    auto height = source->height();
    ApplyImageOrigin(origin, &width, &height);
    image = std::shared_ptr<MatrixImage>(new MatrixImage(std::move(source), width, height, matrix));
  } else {
    image = std::shared_ptr<Image>(new Image(std::move(source)));
  }
  image->weakThis = image;
  return image;
}

Image::Image(std::shared_ptr<ImageSource> source) : source(std::move(source)) {
}

int Image::width() const {
  return source->width();
}

int Image::height() const {
  return source->height();
}

bool Image::hasMipmaps() const {
  return source->hasMipmaps();
}

bool Image::isAlphaOnly() const {
  return source->isAlphaOnly();
}

bool Image::isRGBAAA() const {
  return false;
}

bool Image::isLazyGenerated() const {
  return source->isLazyGenerated();
}

bool Image::isTextureBacked() const {
  return source->isTextureBacked();
}

std::shared_ptr<Texture> Image::getTexture() const {
  return source->getTexture();
}

std::shared_ptr<Image> Image::makeTextureImage(Context* context) const {
  auto textureSource = source->makeTextureSource(context);
  if (textureSource == source) {
    return weakThis.lock();
  }
  if (textureSource == nullptr) {
    return nullptr;
  }
  return cloneWithSource(std::move(textureSource));
}

std::shared_ptr<Image> Image::onCloneWithSource(std::shared_ptr<ImageSource> newSource) const {
  return std::shared_ptr<Image>(new Image(std::move(newSource)));
}

std::shared_ptr<Image> Image::makeSubset(const Rect& subset) const {
  auto rect = subset;
  rect.round();
  auto bounds = Rect::MakeWH(width(), height());
  if (bounds == rect) {
    return weakThis.lock();
  }
  if (!bounds.contains(rect)) {
    return nullptr;
  }
  auto subsetImage = onMakeSubset(rect);
  subsetImage->weakThis = subsetImage;
  return subsetImage;
}

std::shared_ptr<Image> Image::onMakeSubset(const Rect& subset) const {
  auto localMatrix = Matrix::MakeTrans(subset.x(), subset.y());
  return std::shared_ptr<MatrixImage>(new MatrixImage(
      source, static_cast<int>(subset.width()), static_cast<int>(subset.height()), localMatrix));
}

std::shared_ptr<Image> Image::makeDecoded(Context* context) const {
  auto decodedSource = source->makeDecoded(context);
  if (decodedSource == source) {
    return weakThis.lock();
  }
  return cloneWithSource(std::move(decodedSource));
}

std::shared_ptr<Image> Image::makeMipMapped() const {
  auto mipMappedSource = source->makeMipMapped();
  if (mipMappedSource == source) {
    return weakThis.lock();
  }
  return cloneWithSource(std::move(mipMappedSource));
}

std::shared_ptr<Image> Image::makeRGBAAA(int displayWidth, int displayHeight, int alphaStartX,
                                         int alphaStartY) {
  if (alphaStartX == 0 && alphaStartY == 0) {
    return makeSubset(Rect::MakeWH(displayWidth, displayHeight));
  }
  auto image = onMakeRGBAAA(displayWidth, displayHeight, alphaStartX, alphaStartY);
  if (image != nullptr) {
    image->weakThis = image;
  }
  return image;
}

std::shared_ptr<Image> Image::onMakeRGBAAA(int displayWidth, int displayHeight, int alphaStartX,
                                           int alphaStartY) const {
  if (alphaStartX + displayWidth > source->width() ||
      alphaStartY + displayHeight > source->height()) {
    return nullptr;
  }
  return std::shared_ptr<Image>(
      new RGBAAAImage(source, displayWidth, displayHeight, alphaStartX, alphaStartY));
}

std::unique_ptr<FragmentProcessor> Image::asFragmentProcessor(Context* context,
                                                              const SamplingOptions& sampling,
                                                              const Matrix* localMatrix,
                                                              uint32_t surfaceFlags) {
  return asFragmentProcessor(context, TileMode::Clamp, TileMode::Clamp, sampling, localMatrix,
                             surfaceFlags);
}

std::unique_ptr<FragmentProcessor> Image::asFragmentProcessor(Context* context, TileMode tileModeX,
                                                              TileMode tileModeY,
                                                              const SamplingOptions& sampling,
                                                              const Matrix* localMatrix,
                                                              uint32_t surfaceFlags) {
  return TiledTextureEffect::Make(source->lockTextureProxy(context, surfaceFlags), tileModeX,
                                  tileModeY, sampling, localMatrix);
}

std::shared_ptr<Image> Image::cloneWithSource(std::shared_ptr<ImageSource> newSource) const {
  auto decodedImage = onCloneWithSource(std::move(newSource));
  decodedImage->weakThis = decodedImage;
  return decodedImage;
}
}  // namespace tgfx
