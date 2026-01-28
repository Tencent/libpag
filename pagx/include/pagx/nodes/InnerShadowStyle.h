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

#include "pagx/nodes/LayerStyle.h"
#include "pagx/nodes/Color.h"

namespace pagx {

/**
 * An inner shadow layer style that renders a shadow inside the layer content.
 */
class InnerShadowStyle : public LayerStyle {
 public:
  /**
   * The horizontal offset of the shadow in pixels. The default value is 0.
   */
  float offsetX = 0;

  /**
   * The vertical offset of the shadow in pixels. The default value is 0.
   */
  float offsetY = 0;

  /**
   * The horizontal blur radius of the shadow in pixels. The default value is 0.
   */
  float blurX = 0;

  /**
   * The vertical blur radius of the shadow in pixels. The default value is 0.
   */
  float blurY = 0;

  /**
   * The color of the shadow.
   */
  Color color = {};

  NodeType nodeType() const override {
    return NodeType::InnerShadowStyle;
  }

 private:
  InnerShadowStyle() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
