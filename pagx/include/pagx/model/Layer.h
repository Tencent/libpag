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

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "pagx/model/Element.h"
#include "pagx/model/LayerFilter.h"
#include "pagx/model/LayerStyle.h"
#include "pagx/model/types/BlendMode.h"
#include "pagx/model/types/MaskType.h"
#include "pagx/model/types/Types.h"

namespace pagx {

/**
 * Layer represents a layer node that can contain vector elements, layer styles, filters, and child
 * layers. It is the main building block for composing visual content in a PAGX document.
 */
class Layer {
 public:
  /**
   * The unique identifier of the layer.
   */
  std::string id = {};

  /**
   * The display name of the layer.
   */
  std::string name = {};

  /**
   * Whether the layer is visible. The default value is true.
   */
  bool visible = true;

  /**
   * The opacity of the layer, ranging from 0 to 1. The default value is 1.
   */
  float alpha = 1;

  /**
   * The blend mode used when compositing the layer. The default value is Normal.
   */
  BlendMode blendMode = BlendMode::Normal;

  /**
   * The x-coordinate of the layer position. The default value is 0.
   */
  float x = 0;

  /**
   * The y-coordinate of the layer position. The default value is 0.
   */
  float y = 0;

  /**
   * The 2D transformation matrix of the layer.
   */
  Matrix matrix = {};

  /**
   * The 3D transformation matrix as a 16-element array in column-major order.
   */
  std::vector<float> matrix3D = {};

  /**
   * Whether to preserve 3D transformations for child layers. The default value is false.
   */
  bool preserve3D = false;

  /**
   * Whether to apply anti-aliasing to the layer edges. The default value is true.
   */
  bool antiAlias = true;

  /**
   * Whether to use group opacity mode for the layer and its children. The default value is false.
   */
  bool groupOpacity = false;

  /**
   * Whether layer effects pass through to the background. The default value is true.
   */
  bool passThroughBackground = true;

  /**
   * Whether to exclude child effects when applying layer styles. The default value is false.
   */
  bool excludeChildEffectsInLayerStyle = false;

  /**
   * The scroll rectangle for clipping the layer content.
   */
  Rect scrollRect = {};

  /**
   * Whether a scroll rectangle is applied to the layer. The default value is false.
   */
  bool hasScrollRect = false;

  /**
   * A reference to a mask layer (e.g., "#maskId").
   */
  std::string mask = {};

  /**
   * The type of masking to apply (Alpha, Luminosity, InvertedAlpha, or InvertedLuminosity). The
   * default value is Alpha.
   */
  MaskType maskType = MaskType::Alpha;

  /**
   * A reference to a composition (e.g., "#compositionId") used as the layer content.
   */
  std::string composition = {};

  /**
   * The vector elements contained in this layer (shapes, painters, modifiers, etc.).
   */
  std::vector<std::unique_ptr<Element>> contents = {};

  /**
   * The layer styles applied to this layer (drop shadow, inner shadow, etc.).
   */
  std::vector<std::unique_ptr<LayerStyle>> styles = {};

  /**
   * The filters applied to this layer (blur, color matrix, etc.).
   */
  std::vector<std::unique_ptr<LayerFilter>> filters = {};

  /**
   * The child layers contained in this layer.
   */
  std::vector<std::unique_ptr<Layer>> children = {};

  /**
   * Custom data from SVG data-* attributes. The keys are stored without the "data-" prefix.
   */
  std::unordered_map<std::string, std::string> customData = {};
};

}  // namespace pagx
