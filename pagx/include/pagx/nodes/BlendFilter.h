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

#include "pagx/nodes/LayerFilter.h"
#include "pagx/nodes/BlendMode.h"
#include "pagx/nodes/Color.h"

namespace pagx {

/**
 * A blend filter that blends a color with the layer content using a specified blend mode.
 */
class BlendFilter : public LayerFilter {
 public:
  /**
   * The color to blend with the layer content.
   */
  Color color = {};

  /**
   * The blend mode used for combining the color with the layer. The default value is Normal.
   */
  BlendMode blendMode = BlendMode::Normal;

  NodeType nodeType() const override {
    return NodeType::BlendFilter;
  }

 private:
  BlendFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
