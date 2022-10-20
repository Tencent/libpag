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

#include "NativePlatform.h"
#include <emscripten/val.h>
#include "NativeTextShaper.h"
#ifdef PAG_USE_HARFBUZZ
#include "base/utils/USE.h"
#endif

using namespace emscripten;

namespace pag {
const Platform* Platform::Current() {
  static const NativePlatform platform = {};
  return &platform;
}

void NativePlatform::traceImage(const tgfx::ImageInfo& info, const void* pixels,
                                const std::string& tag) const {
  auto traceImage = val::module_property("traceImage");
  auto bytes = val(typed_memory_view(info.byteSize(), static_cast<const uint8_t*>(pixels)));
  traceImage(info, bytes, tag);
}

std::optional<PositionedGlyphs> NativePlatform::shapeText(
    const std::string& text, const std::shared_ptr<tgfx::Typeface>& typeface) const {
#ifdef PAG_USE_HARBUZZ
  USE(text);
  USE(typeface);
  return std::nullopt;
#else
  return NativeTextShaper::Shape(text, typeface);
#endif
}
}  // namespace pag
