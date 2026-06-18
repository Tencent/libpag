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

#include <string>
#include <unordered_map>
#include "pagx/nodes/ViewModelProperty.h"

namespace pagx {

/**
 * PropertyData exposes reflection metadata for a ViewModel property at runtime. It mirrors the
 * schema-level ViewModelProperty node so that business layers can auto-generate UI controls
 * without hardcoding property names or types.
 */
class PropertyData {
 public:
  using Type = ViewModelPropertyType;

  /**
   * Returns the value type of this property.
   */
  Type type() const {
    return propertyType;
  }

  /**
   * Returns the property name as declared in the ViewModel schema.
   */
  const std::string& name() const {
    return propertyName;
  }

  /**
   * Returns custom data attributes from the property's schema node (data-* XML attributes),
   * with the "data-" prefix stripped. Used by business layers to attach arbitrary metadata
   * (e.g. control hints, tracking events, grouping).
   */
  const std::unordered_map<std::string, std::string>& customData() const {
    return customDataMap;
  }

 private:
  PropertyData() = default;

  Type propertyType = Type::Number;
  std::string propertyName = {};
  std::unordered_map<std::string, std::string> customDataMap = {};

  friend class PAGViewModel;
};

}  // namespace pagx
