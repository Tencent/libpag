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
#include "gpu/Texture.h"
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
std::shared_ptr<Image> Image::MakeFromFile(const std::string& filePath) {
  auto codec = ImageCodec::MakeFrom(filePath);
  if (codec == nullptr) {
    return nullptr;
  }
  return MakeFrom(codec, codec->origin());
}

std::shared_ptr<Image> Image::MakeFromEncoded(std::shared_ptr<Data> encodedData) {
  auto codec = ImageCodec::MakeFrom(std::move(encodedData));
  if (codec == nullptr) {
    return nullptr;
  }
  return MakeFrom(codec, codec->origin());
}

std::shared_ptr<Image> Image::MakeFrom(NativeImageRef nativeImage, ImageOrigin origin) {
  auto codec = ImageCodec::MakeFrom(nativeImage);
  return MakeFrom(std::move(codec), origin);
}

std::shared_ptr<Image> Image::MakeFrom(std::shared_ptr<ImageGenerator> generator,
                                       ImageOrigin origin) {
  auto source = ImageSource::MakeFrom(std::move(generator));
  return MakeFromSource(std::move(source), origin);
}

std::shared_ptr<Image> Image::MakeFrom(const ImageInfo& info, std::shared_ptr<Data> pixels,
                                       ImageOrigin origin) {
  auto imageBuffer = RasterBuffer::MakeFrom(info, pixels);
  if (imageBuffer != nullptr) {
    return MakeFrom(std::move(imageBuffer), origin);
  }
  auto imageGenerator = RasterGenerator::MakeFrom(info, std::move(pixels));
  return MakeFrom(std::move(imageGenerator), origin);
}

std::shared_ptr<Image> Image::MakeFrom(const Bitmap& bitmap, ImageOrigin origin) {
  return MakeFrom(bitmap.makeBuffer(), origin);
}

std::shared_ptr<Image> Image::MakeFrom(HardwareBufferRef hardwareBuffer, YUVColorSpace colorSpace,
                                       ImageOrigin origin) {
  auto buffer = ImageBuffer::MakeFrom(hardwareBuffer, colorSpace);
  return MakeFrom(std::move(buffer), origin);
}

std::shared_ptr<Image> Image::MakeI420(std::shared_ptr<YUVData> yuvData, YUVColorSpace colorSpace,
                                       ImageOrigin origin) {
  auto buffer = ImageBuffer::MakeI420(std::move(yuvData), colorSpace);
  return MakeFrom(std::move(buffer), origin);
}

std::shared_ptr<Image> Image::MakeNV12(std::shared_ptr<YUVData> yuvData, YUVColorSpace colorSpace,
                                       ImageOrigin origin) {
  auto buffer = ImageBuffer::MakeNV12(std::move(yuvData), colorSpace);
  return MakeFrom(std::move(buffer), origin);
}

std::shared_ptr<Image> Image::MakeFrom(std::shared_ptr<ImageBuffer> imageBuffer,
                                       ImageOrigin origin) {
  auto source = ImageSource::MakeFrom(std::move(imageBuffer));
  return MakeFromSource(std::move(source), origin);
}

std::shared_ptr<Image> Image::MakeFrom(Context* context, const BackendTexture& backendTexture,
                                       ImageOrigin origin) {
  SurfaceOrigin textureOrigin = SurfaceOrigin::TopLeft;
  if (origin == ImageOrigin::BottomLeft) {
    textureOrigin = SurfaceOrigin::BottomLeft;
    origin = ImageOrigin::TopLeft;
  }
  auto texture = Texture::MakeFrom(context, backendTexture, textureOrigin);
  return MakeFrom(std::move(texture), origin);
}

std::shared_ptr<Image> Image::MakeAdopted(Context* context, const BackendTexture& backendTexture,
                                          ImageOrigin origin) {
  SurfaceOrigin textureOrigin = SurfaceOrigin::TopLeft;
  if (origin == ImageOrigin::BottomLeft) {
    textureOrigin = SurfaceOrigin::BottomLeft;
    origin = ImageOrigin::TopLeft;
  }
  auto texture = Texture::MakeAdopted(context, backendTexture, textureOrigin);
  return MakeFrom(std::move(texture), origin);
}

std::shared_ptr<Image> Image::MakeFrom(std::shared_ptr<Texture> texture, ImageOrigin origin) {
  auto source = ImageSource::MakeFrom(std::move(texture));
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

BackendTexture Image::getBackendTexture() const {
  return source->getBackendTexture();
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

std::unique_ptr<FragmentProcessor> Image::asFragmentProcessor(
    Context* context, uint32_t surfaceFlags, TileMode tileModeX, TileMode tileModeY,
    const SamplingOptions& sampling, const Matrix* localMatrix) {
  return TiledTextureEffect::Make(source->lockTextureProxy(context, surfaceFlags), tileModeX,
                                  tileModeY, sampling, localMatrix);
}

std::unique_ptr<FragmentProcessor> Image::asFragmentProcessor(Context* context,
                                                              uint32_t surfaceFlags,
                                                              const SamplingOptions& sampling) {
  return asFragmentProcessor(context, surfaceFlags, TileMode::Clamp, TileMode::Clamp, sampling);
}

std::shared_ptr<Image> Image::cloneWithSource(std::shared_ptr<ImageSource> newSource) const {
  auto decodedImage = onCloneWithSource(std::move(newSource));
  decodedImage->weakThis = decodedImage;
  return decodedImage;
}
}  // namespace tgfx
