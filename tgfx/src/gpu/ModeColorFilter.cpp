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

#include "ModeColorFilter.h"
#include "ConstColorProcessor.h"
#include "XfermodeFragmentProcessor.h"

namespace tgfx {
static bool IsNoOps(float alpha, BlendMode mode) {
  return BlendMode::Dst == mode ||
         (0.f == alpha &&
          (BlendMode::DstOver == mode || BlendMode::DstOut == mode || BlendMode::SrcATop == mode ||
           BlendMode::Xor == mode || BlendMode::Darken == mode)) ||
         (1.f == alpha && BlendMode::DstIn == mode);
}

std::shared_ptr<ColorFilter> ColorFilter::Blend(Color color, BlendMode mode) {
  float alpha = color.alpha;
  if (BlendMode::Clear == mode) {
    color = Color::Transparent();
    mode = BlendMode::Src;
  } else if (BlendMode::SrcOver == mode) {
    if (0.f == alpha) {
      mode = BlendMode::Dst;
    } else if (1.f == alpha) {
      mode = BlendMode::Src;
    }
  }
  // weed out combinations that are no-ops, and just return null.
  if (IsNoOps(alpha, mode)) {
    return nullptr;
  }
  return std::make_shared<ModeColorFilter>(color, mode);
}

std::unique_ptr<FragmentProcessor> ModeColorFilter::asFragmentProcessor() const {
  return XfermodeFragmentProcessor::MakeFromSrcProcessor(
      ConstColorProcessor::Make(color.premultiply(), InputMode::Ignore), mode);
}
}  // namespace tgfx
