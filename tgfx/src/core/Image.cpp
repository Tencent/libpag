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
#include "core/images/ImageSource.h"
#include "core/images/MatrixImage.h"
#include "core/images/RGBAAAImage.h"
#include "gpu/TextureEffect.h"
#include "gpu/TiledTextureEffect.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/PixelBuffer.h"

namespace tgfx {
std::shared_ptr<Image> Image::MakeFromEncoded(const std::string& filePath, bool mipMapped) {
  auto codec = ImageCodec::MakeFrom(filePath);
  if (codec == nullptr) {
    return nullptr;
  }
  return MakeFromGenerator(codec, codec->orientation(), mipMapped);
}

std::shared_ptr<Image> Image::MakeFromEncoded(std::shared_ptr<Data> encodedData, bool mipMapped) {
  auto codec = ImageCodec::MakeFrom(std::move(encodedData));
  if (codec == nullptr) {
    return nullptr;
  }
  return MakeFromGenerator(codec, codec->orientation(), mipMapped);
}

std::shared_ptr<Image> Image::MakeFromGenerator(std::shared_ptr<ImageGenerator> generator,
                                                Orientation orientation, bool mipMapped) {
  auto source = ImageSource::MakeFromGenerator(generator, mipMapped);
  return MakeFromSource(std::move(source), orientation);
}

std::shared_ptr<Image> Image::MakeFromPixels(const ImageInfo& info, const void* pixels,
                                             Orientation orientation, bool mipMapped) {
  if (info.isEmpty() || pixels == nullptr) {
    return nullptr;
  }
  auto buffer = PixelBuffer::Make(info.width(), info.height(), info.isAlphaOnly());
  if (buffer == nullptr) {
    return nullptr;
  }
  {  // make sure buffer is unlocked after writing pixels.
    Bitmap bitmap(buffer);
    bitmap.writePixels(info, pixels);
  }
  return MakeFromBuffer(std::move(buffer), orientation, mipMapped);
}

std::shared_ptr<Image> Image::MakeFromBuffer(std::shared_ptr<ImageBuffer> imageBuffer,
                                             Orientation orientation, bool mipMapped) {
  auto source = ImageSource::MakeFromBuffer(std::move(imageBuffer), mipMapped);
  return MakeFromSource(std::move(source), orientation);
}

std::shared_ptr<Image> Image::MakeFromTexture(std::shared_ptr<Texture> texture,
                                              Orientation orientation) {
  auto source = ImageSource::MakeFromTexture(std::move(texture));
  return MakeFromSource(std::move(source), orientation);
}

std::shared_ptr<Image> Image::MakeRGBAAA(std::shared_ptr<ImageGenerator> generator,
                                         int displayWidth, int displayHeight, int alphaStartX,
                                         int alphaStartY, bool mipMapped) {
  auto source = ImageSource::MakeFromGenerator(std::move(generator), mipMapped);
  return MakeRGBAAA(std::move(source), displayWidth, displayHeight, alphaStartX, alphaStartY);
}

std::shared_ptr<Image> Image::MakeRGBAAA(std::shared_ptr<ImageBuffer> imageBuffer, int displayWidth,
                                         int displayHeight, int alphaStartX, int alphaStartY,
                                         bool mipMapped) {
  auto source = ImageSource::MakeFromBuffer(std::move(imageBuffer), mipMapped);
  return MakeRGBAAA(std::move(source), displayWidth, displayHeight, alphaStartX, alphaStartY);
}

std::shared_ptr<Image> Image::MakeRGBAAA(std::shared_ptr<Texture> texture, int displayWidth,
                                         int displayHeight, int alphaStartX, int alphaStartY) {
  auto source = ImageSource::MakeFromTexture(std::move(texture));
  return MakeRGBAAA(std::move(source), displayWidth, displayHeight, alphaStartX, alphaStartY);
}

std::shared_ptr<Image> Image::MakeFromSource(std::shared_ptr<ImageSource> source,
                                             Orientation orientation) {
  if (source == nullptr) {
    return nullptr;
  }
  std::shared_ptr<Image> image = nullptr;
  if (orientation != Orientation::TopLeft) {
    auto matrix = OrientationToMatrix(orientation, source->width(), source->height());
    matrix.invert(&matrix);
    auto width = source->width();
    auto height = source->height();
    ApplyOrientation(orientation, &width, &height);
    image = std::shared_ptr<MatrixImage>(new MatrixImage(std::move(source), width, height, matrix));
  } else {
    image = std::shared_ptr<Image>(new Image(std::move(source)));
  }
  image->weakThis = image;
  return image;
}

std::shared_ptr<Image> Image::MakeRGBAAA(std::shared_ptr<ImageSource> source, int displayWidth,
                                         int displayHeight, int alphaStartX, int alphaStartY) {
  if (source == nullptr) {
    return nullptr;
  }
  if (alphaStartX == 0 && alphaStartY == 0) {
    auto image = MakeFromSource(std::move(source));
    if (displayWidth != image->width() || displayHeight != image->height()) {
      image = image->makeSubset(Rect::MakeWH(displayWidth, displayHeight));
    }
    return image;
  }
  if (alphaStartX + displayWidth > source->width() ||
      alphaStartY + displayHeight > source->height()) {
    return nullptr;
  }
  auto image = std::shared_ptr<Image>(
      new RGBAAAImage(std::move(source), displayWidth, displayHeight, alphaStartX, alphaStartY));
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

std::shared_ptr<Image> Image::makeTextureImage(Context* context, bool wrapCacheOnly) const {
  auto textureSource = source->makeTextureSource(context, wrapCacheOnly);
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

void Image::markCacheExpired(Context* context) {
  auto proxy = source->getTextureProxy(context);
  if (proxy) {
    proxy->removeCacheOwner();
  }
}

std::shared_ptr<Image> Image::onMakeSubset(const Rect& subset) const {
  auto localMatrix = Matrix::MakeTrans(subset.x(), subset.y());
  return std::shared_ptr<MatrixImage>(new MatrixImage(
      source, static_cast<int>(subset.width()), static_cast<int>(subset.height()), localMatrix));
}

std::unique_ptr<FragmentProcessor> Image::asFragmentProcessor(Context* context,
                                                              const SamplingOptions& sampling,
                                                              const Matrix* localMatrix) {
  return asFragmentProcessor(context, TileMode::Clamp, TileMode::Clamp, sampling, localMatrix);
}

std::unique_ptr<FragmentProcessor> Image::asFragmentProcessor(Context* context, TileMode tileModeX,
                                                              TileMode tileModeY,
                                                              const SamplingOptions& sampling,
                                                              const Matrix* localMatrix) {
  auto proxy = source->lockTextureProxy(context);
  if (proxy == nullptr) {
    return nullptr;
  }
  if (!proxy->isInstantiated()) {
    proxy->instantiate();
  }
  auto texture = proxy->getTexture();
  if ((tileModeX != TileMode::Clamp || tileModeY != TileMode::Clamp) && !texture->isYUV()) {
    return TiledTextureEffect::Make(std::move(texture),
                                    SamplerState(tileModeX, tileModeY, sampling), localMatrix);
  }
  return TextureEffect::Make(std::move(texture), sampling, localMatrix);
}

std::shared_ptr<Image> Image::cloneWithSource(std::shared_ptr<ImageSource> newSource) const {
  auto decodedImage = onCloneWithSource(std::move(newSource));
  decodedImage->weakThis = decodedImage;
  return decodedImage;
}
}  // namespace tgfx
