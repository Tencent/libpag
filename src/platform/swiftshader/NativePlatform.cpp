/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "NativePlatform.h"
#include <atomic>
#include <fstream>
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"

namespace pag {
static std::atomic<NALUType> defaultType = {NALUType::AnnexB};

const Platform* Platform::Current() {
  static const NativePlatform platform = {};
  return &platform;
}

NALUType NativePlatform::naluType() const {
  return defaultType;
}

void NativePlatform::setNALUType(NALUType type) const {
  defaultType = type;
}

void NativePlatform::traceImage(const tgfx::ImageInfo& info, const void* pixels,
                                const std::string& tag) const {
  std::string path = tag;
  if (path.empty()) {
    path = "TraceImage.png";
  } else if (path.rfind(".png") != path.size() - 4 && path.rfind(".PNG") != path.size() - 4) {
    path += ".png";
  }
  auto bytes = tgfx::ImageCodec::Encode(tgfx::Pixmap(info, pixels), tgfx::EncodedFormat::PNG, 100);
  if (bytes) {
    std::ofstream out(path);
    out.write(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    out.close();
  }
}

std::string NativePlatform::getCacheDir() const {
  return "./Caches/libpag";
}
}  // namespace pag
