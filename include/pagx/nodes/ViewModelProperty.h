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
#include <optional>
#include <string>
#include "pagx/nodes/Node.h"
#include "pagx/types/Color.h"

namespace pagx {

class DataConverter;
class ViewModel;

/**
 * ViewModelPropertyType enumerates the value types a ViewModel property can hold.
 */
enum class ViewModelPropertyType {
  Number,
  String,
  Boolean,
  Color,
  Image,
  ViewModel,
  Enum,
  Trigger
};

/**
 * ViewModelProperty declares a single property within a ViewModel schema. It defines the property's
 * name, value type, default value, and an optional data converter that transforms values before
 * they are applied to render nodes.
 */
class ViewModelProperty : public Node {
 public:
  /**
   * The property name used for programmatic access (e.g. "title", "enterSpeed").
   */
  std::string name = {};

  /**
   * The value type of this property.
   */
  ViewModelPropertyType propertyType = ViewModelPropertyType::Number;

  /**
   * An optional data converter that transforms this property's value before applying it through
   * a DataBind target channel. When null, the raw value is used directly.
   */
  DataConverter* dataConverter = nullptr;

  /**
   * When propertyType is ViewModel, this references the target ViewModel schema whose instances
   * populate this property's value at runtime. Ignored for other property types.
   */
  ViewModel* viewModelRef = nullptr;

  /**
   * Default numeric value, used when propertyType is Number.
   */
  float defaultNumber = 0.0f;

  /**
   * Minimum allowed value for Number properties. nullopt means no constraint.
   */
  std::optional<float> minValue = std::nullopt;

  /**
   * Maximum allowed value for Number properties. nullopt means no constraint.
   */
  std::optional<float> maxValue = std::nullopt;

  /**
   * Default string value, used when propertyType is String.
   */
  std::string defaultString = {};

  /**
   * Default boolean value, used when propertyType is Boolean.
   */
  bool defaultBoolean = false;

  /**
   * Default color value, used when propertyType is Color.
   */
  Color defaultColor = {};

  /**
   * Default image reference, used when propertyType is Image. The value is an image resource id
   * (e.g. "assets/hero.png") or a data URI.
   */
  std::string defaultImage = {};

  /**
   * Allowed string values for Enum properties. Each value maps to its index (0-based).
   * Ignored for other property types.
   */
  std::vector<std::string> enumOptions = {};

  NodeType nodeType() const override {
    return NodeType::ViewModelProperty;
  }

 private:
  ViewModelProperty() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
