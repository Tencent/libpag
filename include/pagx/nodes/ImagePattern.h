/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "pagx/nodes/ColorSource.h"
#include "pagx/types/FilterMode.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/MipmapMode.h"
#include "pagx/types/TileMode.h"

namespace pagx {

class Image;

/**
 * An image pattern color source that tiles an image to fill shapes.
 */
class ImagePattern : public ColorSource {
 public:
  /**
   * A reference to an image resource.
   */
  Image* image = nullptr;

  /**
   * The tile mode for the horizontal direction. The default value is Clamp.
   */
  TileMode tileModeX = TileMode::Clamp;

  /**
   * The tile mode for the vertical direction. The default value is Clamp.
   */
  TileMode tileModeY = TileMode::Clamp;

  /**
   * The filter mode for texture sampling. The default value is Linear.
   */
  FilterMode filterMode = FilterMode::Linear;

  /**
   * The mipmap mode for texture sampling. The default value is Linear.
   */
  MipmapMode mipmapMode = MipmapMode::Linear;

  /**
   * The transformation matrix applied to the pattern.
   */
  Matrix matrix = {};

  NodeType nodeType() const override {
    return NodeType::ImagePattern;
  }

 private:
  ImagePattern() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
