/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "pagx/model/NodeType.h"

namespace pagx {

/**
 * ResourceType enumerates all types of resources that can be stored in a PAGX document.
 */
enum class ResourceType {
  /**
   * An image resource.
   */
  Image,
  /**
   * A reusable path data resource.
   */
  PathData,
  /**
   * A composition resource containing layers.
   */
  Composition,
  /**
   * A solid color resource.
   */
  SolidColor,
  /**
   * A linear gradient resource.
   */
  LinearGradient,
  /**
   * A radial gradient resource.
   */
  RadialGradient,
  /**
   * A conic gradient resource.
   */
  ConicGradient,
  /**
   * A diamond gradient resource.
   */
  DiamondGradient,
  /**
   * An image pattern resource.
   */
  ImagePattern
};

/**
 * Returns the string name of a resource type.
 */
const char* ResourceTypeName(ResourceType type);

/**
 * Resource is the base class for all resources in a PAGX document. Resources are reusable items
 * that can be referenced by ID (e.g., "#imageId", "#gradientId").
 */
class Resource {
 public:
  virtual ~Resource() = default;

  /**
   * Returns the resource type of this resource.
   */
  virtual ResourceType resourceType() const = 0;

  /**
   * Returns the unified node type of this resource.
   */
  virtual NodeType type() const = 0;

  /**
   * Returns the unique identifier of this resource.
   */
  virtual const std::string& resourceId() const = 0;

 protected:
  Resource() = default;
};

}  // namespace pagx
