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
#include <memory>
#include <vector>

namespace pagx {

/**
 * ResourceType identifies the kind of external resource requested during PAGXDocument import.
 */
enum class ResourceType : uint8_t {
  Image = 0,
  Font = 1,
  Composition = 2,
};

class ResourceReferencer;

/**
 * Resource is a retained, loader-facing asset object. A ResourceLoader may fill it immediately or
 * keep it for delayed completion. When resource data changes, all registered ResourceReferencer
 * objects are notified so importer-owned nodes can consume the new data.
 */
class Resource {
 public:
  virtual ~Resource() = default;

  /**
   * Returns the concrete resource kind without requiring RTTI.
   */
  virtual ResourceType resourceType() const = 0;

  /**
   * Registers a node or other internal object that consumes this resource.
   * @param referencer the object to notify when this resource updates.
   */
  void addReferencer(ResourceReferencer* referencer);

  /**
   * Removes a previously registered resource consumer.
   * @param referencer the object to stop notifying.
   */
  void removeReferencer(ResourceReferencer* referencer);

  /**
   * Returns all current resource consumers.
   */
  const std::vector<ResourceReferencer*>& referencers() const {
    return resourceReferencers;
  }

 protected:
  /**
   * Notifies registered referencers that this resource has new data.
   */
  void notifyUpdated();

 private:
  std::vector<ResourceReferencer*> resourceReferencers = {};
};

/**
 * ResourceReferencer is implemented by importer-owned nodes that consume retained Resources. It
 * mirrors Rive's FileAssetReferencer model: setting a Resource registers this object with that
 * Resource, and later resource updates are delivered through resourceUpdated().
 */
class ResourceReferencer {
 public:
  virtual ~ResourceReferencer();

  /**
   * Sets the retained Resource consumed by this referencer. The referencer is automatically removed
   * from the previous Resource and added to the new one.
   * @param resource the new retained Resource, or nullptr to clear the reference.
   */
  virtual void setResource(std::shared_ptr<Resource> resource);

  /**
   * Returns the retained Resource consumed by this referencer.
   */
  std::shared_ptr<Resource> resource() const {
    return retainedResource;
  }

  /**
   * Called when the retained Resource changes. Subclasses copy data from resource into their own
   * importer-owned node fields and notify the owning document.
   */
  virtual void resourceUpdated(Resource* resource) {
    (void)resource;
  }

 protected:
  std::shared_ptr<Resource> retainedResource = nullptr;
};

}  // namespace pagx
