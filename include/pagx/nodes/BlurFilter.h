/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "pagx/nodes/LayerFilter.h"
#include "pagx/types/TileMode.h"

namespace pagx {

/**
 * A blur filter that applies a Gaussian blur effect to the layer.
 */
class BlurFilter : public LayerFilter {
 public:
  /**
   * The horizontal blur radius in pixels. The default value is 0.
   */
  float blurX = 0;

  /**
   * The vertical blur radius in pixels. The default value is 0.
   */
  float blurY = 0;

  /**
   * The tile mode for handling blur edges. The default value is Decal.
   */
  TileMode tileMode = TileMode::Decal;

  NodeType nodeType() const override {
    return NodeType::BlurFilter;
  }

 private:
  BlurFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
