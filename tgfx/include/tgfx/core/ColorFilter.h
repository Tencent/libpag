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

#pragma once

#include <array>
#include <memory>
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Color.h"

namespace tgfx {
class FragmentProcessor;

class ColorFilter {
 public:
  static std::shared_ptr<ColorFilter> MakeLumaColorFilter();

  static std::shared_ptr<ColorFilter> Blend(Color color, BlendMode mode);

  static std::shared_ptr<ColorFilter> Matrix(const std::array<float, 20>& rowMajor);

  virtual ~ColorFilter() = default;

  virtual std::unique_ptr<FragmentProcessor> asFragmentProcessor() const = 0;
};
}  // namespace tgfx
