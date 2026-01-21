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

#include <memory>
#include <string>

namespace pagx {

class Composition;
class Image;
class PathDataResource;
class ColorSource;

/**
 * Resource types that can be stored in PAGXDocument.resources.
 */
enum class ResourceType {
  Image,
  PathData,
  ColorSource,
  Composition
};

/**
 * Returns the string name of a resource type.
 */
const char* ResourceTypeName(ResourceType type);

/**
 * A resource in the document that can be referenced by ID.
 */
class Resource {
 public:
  virtual ~Resource() = default;

  /**
   * Returns the type of this resource.
   */
  virtual ResourceType type() const = 0;

  /**
   * Returns the ID of this resource.
   */
  virtual const std::string& id() const = 0;

 protected:
  Resource() = default;
};

/**
 * Image resource wrapper.
 */
class ImageResource : public Resource {
 public:
  std::unique_ptr<Image> image = nullptr;

  ResourceType type() const override {
    return ResourceType::Image;
  }

  const std::string& id() const override;
};

/**
 * PathData resource wrapper.
 */
class PathDataResourceWrapper : public Resource {
 public:
  std::unique_ptr<PathDataResource> pathData = nullptr;

  ResourceType type() const override {
    return ResourceType::PathData;
  }

  const std::string& id() const override;
};

/**
 * ColorSource resource wrapper.
 */
class ColorSourceResource : public Resource {
 public:
  std::unique_ptr<ColorSource> colorSource = nullptr;

  ResourceType type() const override {
    return ResourceType::ColorSource;
  }

  const std::string& id() const override;
};

/**
 * Composition resource wrapper.
 */
class CompositionResource : public Resource {
 public:
  std::unique_ptr<Composition> composition = nullptr;

  ResourceType type() const override {
    return ResourceType::Composition;
  }

  const std::string& id() const override;
};

}  // namespace pagx
