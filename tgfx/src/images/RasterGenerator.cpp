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

#include "RasterGenerator.h"
#include "tgfx/core/Bitmap.h"

namespace tgfx {
std::shared_ptr<ImageGenerator> RasterGenerator::MakeFrom(const ImageInfo& info,
                                                          std::shared_ptr<Data> pixels) {
  if (info.isEmpty() || pixels == nullptr || info.byteSize() > pixels->size()) {
    return nullptr;
  }
  return std::shared_ptr<ImageGenerator>(new RasterGenerator(info, std::move(pixels)));
}

RasterGenerator::RasterGenerator(const ImageInfo& info, std::shared_ptr<Data> pixels)
    : ImageGenerator(info.width(), info.height()), info(info), pixels(std::move(pixels)) {
}

std::shared_ptr<ImageBuffer> RasterGenerator::onMakeBuffer(bool tryHardware) const {
  Bitmap bitmap(width(), height(), isAlphaOnly(), tryHardware);
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  auto success = bitmap.writePixels(info, pixels->data());
  if (!success) {
    return nullptr;
  }
  return bitmap.makeBuffer();
}
}  // namespace tgfx
