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

#include "NativeCodec.h"
#include <atomic>
#include "NativeImageBuffer.h"
#include "NativeImageInfo.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/Stream.h"
#include "tgfx/platform/web/WebImage.h"

using namespace emscripten;

namespace tgfx {
static std::atomic_bool AsyncSupport = false;

void WebImage::SetAsyncSupport(bool enabled) {
  AsyncSupport = enabled;
}

std::shared_ptr<ImageCodec> ImageCodec::MakeNativeCodec(const std::string& filePath) {
  auto imageStream = Stream::MakeFromFile(filePath);
  if (imageStream == nullptr || imageStream->size() <= 14) {
    return nullptr;
  }
  Buffer imageBuffer(imageStream->size());
  imageStream->read(imageBuffer.data(), imageStream->size());
  auto imageData = imageBuffer.release();
  return ImageCodec::MakeNativeCodec(imageData);
}

std::shared_ptr<ImageCodec> ImageCodec::MakeNativeCodec(std::shared_ptr<Data> imageBytes) {
  auto imageSize = NativeImageInfo::GetSize(imageBytes);
  if (imageSize.isEmpty()) {
    return nullptr;
  }
  return std::shared_ptr<ImageCodec>(
      new NativeCodec(imageSize.width, imageSize.height, std::move(imageBytes)));
}

NativeCodec::NativeCodec(int width, int height, std::shared_ptr<Data> imageBytes)
    : ImageCodec(width, height, Orientation::TopLeft), imageBytes(std::move(imageBytes)) {
}

bool NativeCodec::asyncSupport() const {
  return AsyncSupport;
}

bool NativeCodec::readPixels(const ImageInfo&, void*) const {
  return false;
}

std::shared_ptr<ImageBuffer> NativeCodec::onMakeBuffer(bool) const {
  auto nativeImageClass = val::module_property("NativeImage");
  if (!nativeImageClass.as<bool>()) {
    return nullptr;
  }
  auto bytes =
      val(typed_memory_view(imageBytes->size(), static_cast<const uint8_t*>(imageBytes->data())));
  auto nativeImage = nativeImageClass.call<val>("createFromBytes", bytes, width(), height());
  bool usePromise = AsyncSupport;
  if (!usePromise) {
    nativeImage = nativeImage.await();
  }
  return std::shared_ptr<NativeImageBuffer>(
      new NativeImageBuffer(width(), height(), std::move(nativeImage), usePromise));
}
}  // namespace tgfx
