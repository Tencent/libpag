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

class ResourceListener;

/**
 * Resource is a retained, loader-facing asset object. A ResourceLoader may fill it immediately or
 * keep it for delayed completion. When resource data changes, registered ResourceListener objects
 * are notified so document-managed nodes can consume the new data.
 */
class Resource {
 public:
  virtual ~Resource() = default;

  /**
   * Returns the concrete resource kind without requiring RTTI.
   */
  virtual ResourceType resourceType() const = 0;

  /**
   * Registers an object that receives update notifications from this resource.
   * @param listener the object to notify when this resource updates.
   */
  void addListener(ResourceListener* listener);

  /**
   * Removes a previously registered update listener.
   * @param listener the object to stop notifying.
   */
  void removeListener(ResourceListener* listener);

  /**
   * Returns all current update listeners.
   */
  const std::vector<ResourceListener*>& listeners() const {
    return resourceListeners;
  }

 protected:
  /**
   * Notifies registered listeners that this resource has new data.
   */
  void notifyUpdated();

 private:
  std::vector<ResourceListener*> resourceListeners = {};
};

/**
 * ResourceListener receives update callbacks from retained Resources. PAGXDocument implements this
 * interface to keep resource ownership and node mutation centralized in the document.
 */
class ResourceListener {
 public:
  virtual ~ResourceListener() = default;

  /**
   * Called when the retained Resource changes.
   * @param resource the updated resource.
   */
  virtual void resourceUpdated(Resource* resource) = 0;
};

}  // namespace pagx
