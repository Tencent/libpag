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

#include <string>
#include "pagx/model/ColorSource.h"
#include "pagx/model/types/SamplingMode.h"
#include "pagx/model/types/TileMode.h"
#include "pagx/model/types/Types.h"

namespace pagx {

/**
 * An image pattern.
 */
class ImagePattern : public ColorSource {
 public:
  std::string id = {};
  std::string image = {};
  TileMode tileModeX = TileMode::Clamp;
  TileMode tileModeY = TileMode::Clamp;
  SamplingMode sampling = SamplingMode::Linear;
  Matrix matrix = {};

  ColorSourceType colorSourceType() const override {
    return ColorSourceType::ImagePattern;
  }

  ResourceType type() const override {
    return ResourceType::ImagePattern;
  }

  const std::string& resourceId() const override {
    return id;
  }
};

}  // namespace pagx
