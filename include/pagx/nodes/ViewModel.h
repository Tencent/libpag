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

class ViewModelProperty;

/**
 * ViewModel represents a ViewModel schema that declares properties for data binding. A ViewModel
 * is typically attached to a Composition (or the root document) and can be referenced by id from
 * other components. Its properties are the data contract: runtime PAGViewModel instances are
 * created from this schema and populated by business logic.
 */
class ViewModel : public Node {
 public:
  /**
   * The properties declared in this ViewModel schema, in declaration order.
   */
  std::vector<ViewModelProperty*> properties = {};

  NodeType nodeType() const override {
    return NodeType::ViewModel;
  }

 private:
  ViewModel() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
