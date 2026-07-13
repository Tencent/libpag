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

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "pagx/nodes/ViewModelProperty.h"

namespace pagx {

/**
 * PropertyData exposes reflection metadata for a ViewModel property at runtime. The engine
 * populates all fields from the schema — business code only reads them, never writes.
 *
 * Use type() to identify the concrete subclass, then static_cast to the derived type to access
 * type-specific fields. Subclasses are listed below.
 */
class PropertyData {
 public:
  using Type = ViewModelPropertyType;

  virtual ~PropertyData() = default;

  /**
   * Returns the value type of this property. Matches the concrete subclass.
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

 protected:
  PropertyData() = default;

  Type propertyType = Type::Number;
  std::string propertyName = {};
  std::unordered_map<std::string, std::string> customDataMap = {};

  friend class PAGScene;
};

/**
 * Reflection metadata for a Number property. Includes the default value and optional
 * min/max constraints for UI controls like sliders.
 */
class NumberPropertyData : public PropertyData {
 public:
  NumberPropertyData() {
    propertyType = Type::Number;
  }

  /**
   * The default numeric value from the schema.
   */
  std::optional<float> defaultValue = std::nullopt;

  /**
   * The minimum allowed value, or nullopt if not specified in the schema.
   */
  std::optional<float> minValue = std::nullopt;

  /**
   * The maximum allowed value, or nullopt if not specified in the schema.
   */
  std::optional<float> maxValue = std::nullopt;
};

/**
 * Reflection metadata for a String property.
 */
class StringPropertyData : public PropertyData {
 public:
  StringPropertyData() {
    propertyType = Type::String;
  }

  /**
   * The default string value from the schema.
   */
  std::optional<std::string> defaultValue = std::nullopt;
};

/**
 * Reflection metadata for a Boolean property.
 */
class BooleanPropertyData : public PropertyData {
 public:
  BooleanPropertyData() {
    propertyType = Type::Boolean;
  }

  /**
   * The default boolean value from the schema.
   */
  std::optional<bool> defaultValue = std::nullopt;
};

/**
 * Reflection metadata for a Color property.
 */
class ColorPropertyData : public PropertyData {
 public:
  ColorPropertyData() {
    propertyType = Type::Color;
  }

  /**
   * The default color value from the schema.
   */
  std::optional<Color> defaultValue = std::nullopt;
};

/**
 * Reflection metadata for an Image property.
 */
class ImagePropertyData : public PropertyData {
 public:
  ImagePropertyData() {
    propertyType = Type::Image;
  }
};

/**
 * Reflection metadata for a nested ViewModel property.
 */
class ViewModelPropertyData : public PropertyData {
 public:
  ViewModelPropertyData() {
    propertyType = Type::ViewModel;
  }

  /**
   * The id of the referenced ViewModel schema from viewModelRef.
   */
  std::string viewModelId = {};
};

/**
 * Reflection metadata for an Enum property. Stores the list of allowed string values.
 */
class EnumPropertyData : public PropertyData {
 public:
  EnumPropertyData() {
    propertyType = Type::Enum;
  }

  /**
   * The list of allowed enum string values from the schema's options attribute.
   */
  std::vector<std::string> options = {};

  /**
   * The default option index (0-based into options) from the schema.
   */
  int defaultValue = 0;
};

/**
 * Reflection metadata for a Trigger property.
 */
class TriggerPropertyData : public PropertyData {
 public:
  TriggerPropertyData() {
    propertyType = Type::Trigger;
  }
};

}  // namespace pagx
