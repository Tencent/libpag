/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/core/ImageReader.h"
#include "core/ImageStream.h"
#include "core/PixelRef.h"
#include "gpu/Texture.h"
#include "utils/Log.h"

namespace tgfx {
class ImageReaderBuffer : public ImageBuffer {
 public:
  explicit ImageReaderBuffer(std::shared_ptr<ImageReader> imageReader, uint64_t bufferVersion)
      : imageReader(std::move(imageReader)), contentVersion(bufferVersion) {
  }

  int width() const override {
    return imageReader->width();
  }

  int height() const override {
    return imageReader->height();
  }

  bool isAlphaOnly() const override {
    return imageReader->stream->isAlphaOnly();
  }

  bool expired() const override {
    return imageReader->checkExpired(contentVersion);
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const override {
    return imageReader->readTexture(contentVersion, context, mipMapped);
  }

 private:
  std::shared_ptr<ImageReader> imageReader = nullptr;
  uint64_t contentVersion = 0;
};

std::shared_ptr<ImageReader> ImageReader::MakeFrom(const Bitmap& bitmap) {
  return ImageReader::MakeFrom(bitmap.pixelRef);
}

std::shared_ptr<ImageReader> ImageReader::MakeFrom(std::shared_ptr<Mask> mask) {
  return ImageReader::MakeFrom(mask->getImageStream());
}

std::shared_ptr<ImageReader> ImageReader::MakeFrom(std::shared_ptr<ImageStream> imageStream) {
  if (imageStream == nullptr) {
    return nullptr;
  }
  auto imageReader = std::shared_ptr<ImageReader>(new ImageReader(std::move(imageStream)));
  imageReader->weakThis = imageReader;
  return imageReader;
}

ImageReader::ImageReader(std::shared_ptr<ImageStream> imageStream)
    : stream(std::move(imageStream)) {
  stream->attachToStream(this);
  dirtyBounds = Rect::MakeWH(stream->width(), stream->height());
}

ImageReader::~ImageReader() {
  stream->detachFromStream(this);
}

int ImageReader::width() const {
  return stream->width();
}

int ImageReader::height() const {
  return stream->height();
}

std::shared_ptr<ImageBuffer> ImageReader::acquireNextBuffer() {
  std::lock_guard<std::mutex> autoLock(locker);
  DEBUG_ASSERT(!weakThis.expired());
  if (!hasPendingChanges) {
    return nullptr;
  }
  hasPendingChanges = false;
  bufferVersion++;
  return std::make_shared<ImageReaderBuffer>(weakThis.lock(), bufferVersion);
}

bool ImageReader::checkExpired(uint64_t contentVersion) {
  std::lock_guard<std::mutex> autoLock(locker);
  return contentVersion < textureVersion;
}

std::shared_ptr<Texture> ImageReader::readTexture(uint64_t contentVersion, Context* context,
                                                  bool mipMapped) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (contentVersion == textureVersion) {
    return texture;
  }
  if (contentVersion < bufferVersion) {
    LOGE(
        "ImageReader::readTexture(): Failed to read texture, the target ImageBuffer is already "
        "expired!");
    return nullptr;
  }
  bool success = true;
  if (texture == nullptr) {
    texture = stream->onMakeTexture(context, mipMapped);
    success = texture != nullptr;
  } else if (!stream->isHardwareBacked()) {
    success = stream->onUpdateTexture(texture, dirtyBounds);
  }
  if (success) {
    dirtyBounds.setEmpty();
    texture->markUniqueKeyExpired();
    textureVersion = contentVersion;
  }
  return texture;
}

void ImageReader::onContentDirty(const Rect& bounds) {
  std::lock_guard<std::mutex> autoLock(locker);
  hasPendingChanges = true;
  dirtyBounds.join(bounds);
  if (stream->isHardwareBacked() && texture != nullptr) {
    texture->markUniqueKeyExpired();
    textureVersion = 0;
  }
}
}  // namespace tgfx