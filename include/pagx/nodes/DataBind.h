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

#include <cstdint>
#include <string>
#include "pagx/nodes/Node.h"

namespace pagx {

/**
 * DataBind connects a ViewModel property (source) to a render node channel (target). When the
 * source value changes, the target channel is updated accordingly. DataBind nodes are typically
 * declared inside a Composition and reference the composition's own render nodes via @id.
 */
class DataBind : public Node {
 public:
  /**
   * The source path in the ViewModel data tree. Syntax: "$vm.propertyName" or
   * "$vm.parent.child" for nested properties. Resolution walks the DataContext chain.
   */
  std::string source = {};

  /**
   * The target render node id (e.g. "@layerId"). Must reference a node declared in the same
   * Composition.
   */
  std::string target = {};

  /**
   * The target channel name to apply the value to (e.g. "alpha", "text", "color").
   */
  std::string channel = {};

  NodeType nodeType() const override {
    return NodeType::DataBind;
  }

 private:
  DataBind() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
