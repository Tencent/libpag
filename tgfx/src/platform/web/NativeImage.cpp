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

#include "NativeImage.h"
#include "NativeTextureBuffer.h"
#include "platform/NativeCodec.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/Stream.h"

using namespace emscripten;

namespace tgfx {
std::shared_ptr<Image> NativeCodec::MakeImage(const std::string& filePath) {
  if (filePath.find("http://") == 0 || filePath.find("https://") == 0) {
    auto nativeImageClass = val::module_property("NativeImage");
    if (!nativeImageClass.as<bool>()) {
      return nullptr;
    }
    auto nativeImage = nativeImageClass.call<val>("createFromPath", filePath).await();
    return NativeImage::MakeFrom(nativeImage);
  }
  auto imageStream = Stream::MakeFromFile(filePath);
  if (imageStream == nullptr || imageStream->size() <= 14) {
    return nullptr;
  }
  Buffer imageBuffer(imageStream->size());
  imageStream->read(imageBuffer.data(), imageStream->size());
  auto imageData = imageBuffer.release();
  return NativeCodec::MakeImage(imageData);
}

std::shared_ptr<Image> NativeCodec::MakeImage(std::shared_ptr<Data> imageBytes) {
  auto nativeImageClass = val::module_property("NativeImage");
  if (!nativeImageClass.as<bool>()) {
    return nullptr;
  }
  auto bytes =
      val(typed_memory_view(imageBytes->size(), static_cast<const uint8_t*>(imageBytes->data())));
  auto nativeImage = nativeImageClass.call<val>("createFromBytes", bytes).await();
  return NativeImage::MakeFrom(nativeImage);
}

std::shared_ptr<Image> NativeCodec::MakeFrom(void* /*nativeImage*/) {
  return nullptr;
}

std::shared_ptr<NativeImage> NativeImage::MakeFrom(emscripten::val nativeImage) {
  if (!nativeImage.as<bool>()) {
    return nullptr;
  }
  auto width = nativeImage.call<int>("width");
  auto height = nativeImage.call<int>("height");
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  auto image = std::shared_ptr<NativeImage>(new NativeImage(width, height));
  image->nativeImage = std::move(nativeImage);
  return image;
}

std::shared_ptr<TextureBuffer> NativeImage::makeBuffer() const {
  return NativeTextureBuffer::Make(width(), height(), nativeImage);
}
}  // namespace tgfx
