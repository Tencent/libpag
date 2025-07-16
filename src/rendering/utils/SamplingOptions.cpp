/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "SamplingOptions.h"

namespace pag {
tgfx::SamplingOptions GetSamplingOptions(const tgfx::Matrix& matrix, tgfx::Image* image) {
  if (image == nullptr) {
    return {};
  }
  auto mipmapMode = image->hasMipmaps() ? tgfx::MipmapMode::Linear : tgfx::MipmapMode::None;
  tgfx::SamplingOptions samplingOptions(tgfx::FilterMode::Linear, mipmapMode);
  if (matrix.isTranslate()) {
    float tx = matrix.getTranslateX();
    float ty = matrix.getTranslateY();
    if (std::floor(tx) == tx && std::floor(ty) == ty) {
      samplingOptions.filterMode = tgfx::FilterMode::Nearest;
    }
  }
  return samplingOptions;
}
}  // namespace pag
