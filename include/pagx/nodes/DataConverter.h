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

#include <map>
#include <string>
#include "pagx/nodes/Node.h"

namespace pagx {

/**
 * DataConverter transforms ViewModel property values before they are applied to render node
 * channels. A converter carries a type name (e.g. "secondsToFrames") and a set of parameters.
 * The runtime DataBind engine invokes the converter through a registered converter function.
 */
class DataConverter : public Node {
 public:
  /**
   * The converter type identifier (e.g. "secondsToFrames", "priceFormat"). Runtime converter
   * functions are registered by this name.
   */
  std::string converterType = {};

  /**
   * The converter parameters as key-value pairs. Interpretation is converter-specific.
   */
  std::map<std::string, std::string> params = {};

  NodeType nodeType() const override {
    return NodeType::DataConverter;
  }

 private:
  DataConverter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
