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

#include <memory>
#include <vector>
#include "pagx/nodes/Node.h"

namespace pagx {

class Animation;
class Layer;
class PAGXDocument;

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
   * The animations contained in this composition.
   */
  std::vector<Animation*> animations = {};

  // When this composition was loaded from an external PAGX document (via ResourceLoader or
  // cross-doc file reference), externalDoc keeps the sub-document alive so that its nodes remain
  // valid for runtime binding. Null for inline compositions defined in the host document.
  std::shared_ptr<PAGXDocument> externalDoc = nullptr;

  NodeType nodeType() const override {
    return NodeType::Composition;
  }

 private:
  Composition() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
