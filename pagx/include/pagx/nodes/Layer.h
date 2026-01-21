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
#include "pagx/nodes/Element.h"
#include "pagx/nodes/LayerFilter.h"
#include "pagx/nodes/LayerStyle.h"
#include "pagx/nodes/Node.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/MaskType.h"
#include "pagx/types/Types.h"

namespace pagx {

/**
 * Layer node.
 */
class Layer : public Node {
 public:
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

  std::vector<std::unique_ptr<Element>> contents = {};
  std::vector<std::unique_ptr<LayerStyle>> styles = {};
  std::vector<std::unique_ptr<LayerFilter>> filters = {};
  std::vector<std::unique_ptr<Layer>> children = {};

  // Custom data from SVG data-* attributes (key without "data-" prefix)
  std::unordered_map<std::string, std::string> customData = {};

  NodeType type() const override {
    return NodeType::Layer;
  }
};

}  // namespace pagx
