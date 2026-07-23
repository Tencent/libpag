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

#include <vector>
#include "pagx/nodes/Node.h"

namespace pagx {

class DataBind;
class Layer;
class ViewModel;

/**
 * Composition represents a reusable composition resource that contains a set of layers. It can be
 * referenced by a Layer's composition property to create instances.
 */
class Composition : public Node {
 public:
  /**
   * The width of the composition in pixels.
   */
  float width = 0.0f;

  /**
   * The height of the composition in pixels.
   */
  float height = 0.0f;

  /**
   * The layers contained in this composition.
   */
  std::vector<Layer*> layers = {};

  /**
   * The animation and state-machine definitions declared inside this composition's
   * &lt;Animations&gt; block, in declaration order. Each entry is either an Animation or a
   * StateMachine; check nodeType() to dispatch.
   */
  std::vector<Node*> animations = {};
  /**
   * The ViewModel schema bound to this composition.
   */
  ViewModel* viewModel = nullptr;
  /**
   * DataBind nodes that bind ViewModel properties to this composition's layers.
   */
  std::vector<DataBind*> dataBinds = {};

  NodeType nodeType() const override {
    return NodeType::Composition;
  }

 private:
  Composition() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
