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

#include "pagx/model/NodeType.h"

namespace pagx {

/**
 * Layer style types.
 */
enum class LayerStyleType {
  DropShadowStyle,
  InnerShadowStyle,
  BackgroundBlurStyle
};

/**
 * Returns the string name of a layer style type.
 */
const char* LayerStyleTypeName(LayerStyleType type);

/**
 * Base class for layer styles (DropShadowStyle, InnerShadowStyle, BackgroundBlurStyle).
 */
class LayerStyle {
 public:
  virtual ~LayerStyle() = default;

  /**
   * Returns the layer style type of this layer style.
   */
  virtual LayerStyleType layerStyleType() const = 0;

  /**
   * Returns the unified node type of this layer style.
   */
  virtual NodeType type() const = 0;

 protected:
  LayerStyle() = default;
};

}  // namespace pagx
