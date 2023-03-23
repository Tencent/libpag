/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <atomic>
#include "tgfx/gpu/ResourceCache.h"
#include "tgfx/gpu/ResourceKey.h"

namespace tgfx {
/**
 * The base class for GPU resource. Overrides the onReleaseGPU() method to free all GPU resources.
 * No backend API calls should be made during destructuring since there may be no GPU context which
 * is current on the calling thread. Note: Resource is not thread safe, do not access any properties
 * of a Resource unless its associated device is locked.
 */
class Resource {
 public:
  template <class T>
  static std::shared_ptr<T> Wrap(Context* context, T* resource) {
    resource->context = context;
    static_cast<Resource*>(resource)->computeScratchKey(&resource->scratchKey);
    return std::static_pointer_cast<T>(context->resourceCache()->addResource(resource));
  }

  virtual ~Resource() = default;

  /**
   * Retrieves the context associated with this Resource.
   */
  Context* getContext() const {
    return context;
  }

  /**
   * Retrieves the amount of GPU memory used by this resource in bytes.
   */
  virtual size_t memoryUsage() const = 0;

  /**
   * Assigns a UniqueKey to the resource. The resource will be findable via this UniqueKey using
   * ResourceCache.findUniqueResource(). This method is not thread safe, call it only when the
   * associated context is locked.
   */
  void assignUniqueKey(const UniqueKey& newKey);

  /*
   * Removes the UniqueKey from the resource. This method is not thread safe, call it only when
   * the associated context is locked.
   */
  void removeUniqueKey();

  /**
   * Marks the associated UniqueKey as expired. This method is thread safe.
   */
  void markUniqueKeyExpired();

  /**
   * Returns true if the Resource has the same UniqueKey to the newKey and not expired. This method
   * is thread safe.
   */
  bool hasUniqueKey(const UniqueKey& newKey) const;

 protected:
  Context* context = nullptr;

  /**
   * Overridden to compute a scratchKey to make this Resource reusable.
   */
  virtual void computeScratchKey(BytesKey*) const {
  }

 private:
  std::weak_ptr<Resource> weakThis;
  ScratchKey scratchKey = {};
  UniqueKey uniqueKey = {};
  std::atomic_uint32_t uniqueKeyGeneration = 0;
  std::list<Resource*>::iterator cachedPosition;
  int64_t lastUsedTime = 0;

  bool isPurgeable() const {
    return weakThis.expired();
  }

  bool hasValidUniqueKey() const {
    return !uniqueKey.empty() && !uniqueKey.unique() && uniqueKey.uniqueID() == uniqueKeyGeneration;
  }

  /**
   * Overridden to free GPU resources in the backend API.
   */
  virtual void onReleaseGPU() = 0;

  friend class ResourceCache;
};
}  // namespace tgfx
