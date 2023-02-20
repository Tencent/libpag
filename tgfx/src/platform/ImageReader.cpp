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

#include "tgfx/platform/ImageReader.h"
#include "core/utils/Log.h"

namespace tgfx {
class ImageReaderBuffer : public ImageBuffer {
 public:
  explicit ImageReaderBuffer(std::shared_ptr<ImageReader> imageReader)
      : contentVersion(imageReader->textureVersion + 1), imageReader(std::move(imageReader)) {
  }

  int width() const override {
    return imageReader->width();
  }

  int height() const override {
    return imageReader->height();
  }

  bool isAlphaOnly() const override {
    return false;
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const override {
    return imageReader->readTexture(contentVersion, context, mipMapped);
  }

 private:
  int contentVersion = 0;
  std::shared_ptr<ImageReader> imageReader = nullptr;
};

std::shared_ptr<ImageBuffer> ImageReader::makeNextBuffer() {
  DEBUG_ASSERT(!weakThis.expired());
  return std::make_shared<ImageReaderBuffer>(weakThis.lock());
}

std::shared_ptr<Texture> ImageReader::readTexture(int bufferVersion, Context* context,
                                                  bool mipMapped) {
  if (bufferVersion < textureVersion) {
    return nullptr;
  }
  if (bufferVersion > textureVersion) {
    auto success = onUpdateTexture(context, mipMapped);
    if (success) {
      textureVersion = bufferVersion;
    }
  }
  return texture;
}
}  // namespace tgfx