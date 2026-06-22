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
#include <vector>
#include "pagx/nodes/ViewModelProperty.h"

namespace pagx {

/**
 * PropertyData exposes reflection metadata for a ViewModel property at runtime. It mirrors the
 * schema-level ViewModelProperty node so that business layers can auto-generate UI controls
 * without hardcoding property names or types.
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

  friend class PAGViewModel;
  friend class PAGScene;
};

/**
 * Reflection metadata for a Number property. Adds min/max range values for UI controls
 * like sliders and progress bars.
 */
class NumberPropertyData : public PropertyData {
 public:
  NumberPropertyData() {
    propertyType = Type::Number;
  }

  /**
   * The minimum allowed value for this property. Default is 0.0.
   */
  float minValue = 0.0f;

  /**
   * The maximum allowed value for this property. Default is 0.0 (unbounded).
   */
  float maxValue = 0.0f;
};

/**
 * Reflection metadata for a String property.
 */
class StringPropertyData : public PropertyData {
 public:
  StringPropertyData() {
    propertyType = Type::String;
  }
};

/**
 * Reflection metadata for a Boolean property.
 */
class BooleanPropertyData : public PropertyData {
 public:
  BooleanPropertyData() {
    propertyType = Type::Boolean;
  }
};

/**
 * Reflection metadata for a Color property.
 */
class ColorPropertyData : public PropertyData {
 public:
  ColorPropertyData() {
    propertyType = Type::Color;
  }
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
   * The id of the referenced ViewModel schema.
   */
  std::string viewModelRef = {};
};

/**
 * Reflection metadata for an Enum property. Stores the list of allowed string values.
 * The runtime value is the integer index into this list.
 */
class EnumPropertyData : public PropertyData {
 public:
  EnumPropertyData() {
    propertyType = Type::Enum;
  }

  /**
   * The list of allowed enum string values. The runtime integer value is the index.
   */
  std::vector<std::string> options = {};
};

/**
 * Reflection metadata for a Trigger property. Triggers are boolean-like values that
 * fire once and auto-reset.
 */
class TriggerPropertyData : public PropertyData {
 public:
  TriggerPropertyData() {
    propertyType = Type::Trigger;
  }
};

}  // namespace pagx
