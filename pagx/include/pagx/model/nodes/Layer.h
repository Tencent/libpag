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
#include "pagx/model/nodes/LayerFilter.h"
#include "pagx/model/nodes/LayerStyle.h"
#include "pagx/model/nodes/Node.h"
#include "pagx/model/types/Types.h"
#include "pagx/model/nodes/VectorElement.h"
#include "pagx/model/types/enums/BlendMode.h"
#include "pagx/model/types/enums/MaskType.h"

namespace pagx {

/**
 * Layer node.
 */
struct Layer : public Node {
  std::string id = {};
  std::string name = {};
  bool visible = true;
  float alpha = 1;
  BlendMode blendMode = BlendMode::Normal;
  float x = 0;
  float y = 0;
  Matrix matrix = {};
  std::vector<float> matrix3D = {};
  bool preserve3D = false;
  bool antiAlias = true;
  bool groupOpacity = false;
  bool passThroughBackground = true;
  bool excludeChildEffectsInLayerStyle = false;
  Rect scrollRect = {};
  bool hasScrollRect = false;
  std::string mask = {};
  MaskType maskType = MaskType::Alpha;
  std::string composition = {};

  std::vector<std::unique_ptr<VectorElement>> contents = {};
  std::vector<std::unique_ptr<LayerStyle>> styles = {};
  std::vector<std::unique_ptr<LayerFilter>> filters = {};
  std::vector<std::unique_ptr<Layer>> children = {};

  // Custom data from SVG data-* attributes (key without "data-" prefix)
  std::unordered_map<std::string, std::string> customData = {};

  NodeType type() const override {
    return NodeType::Layer;
  }

  std::unique_ptr<Node> clone() const override {
    auto node = std::make_unique<Layer>();
    node->id = id;
    node->name = name;
    node->visible = visible;
    node->alpha = alpha;
    node->blendMode = blendMode;
    node->x = x;
    node->y = y;
    node->matrix = matrix;
    node->matrix3D = matrix3D;
    node->preserve3D = preserve3D;
    node->antiAlias = antiAlias;
    node->groupOpacity = groupOpacity;
    node->passThroughBackground = passThroughBackground;
    node->excludeChildEffectsInLayerStyle = excludeChildEffectsInLayerStyle;
    node->scrollRect = scrollRect;
    node->hasScrollRect = hasScrollRect;
    node->mask = mask;
    node->maskType = maskType;
    node->composition = composition;
    for (const auto& element : contents) {
      node->contents.push_back(
          std::unique_ptr<VectorElement>(static_cast<VectorElement*>(element->clone().release())));
    }
    for (const auto& style : styles) {
      node->styles.push_back(
          std::unique_ptr<LayerStyle>(static_cast<LayerStyle*>(style->clone().release())));
    }
    for (const auto& filter : filters) {
      node->filters.push_back(
          std::unique_ptr<LayerFilter>(static_cast<LayerFilter*>(filter->clone().release())));
    }
    for (const auto& child : children) {
      node->children.push_back(
          std::unique_ptr<Layer>(static_cast<Layer*>(child->clone().release())));
    }
    node->customData = customData;
    return node;
  }
};

}  // namespace pagx
