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

#include <functional>
#include <list>
#include <unordered_map>
#include "tgfx/core/Cacheable.h"
#include "tgfx/gpu/Context.h"

namespace tgfx {
class Resource;

/**
 * Manages the lifetime of all Resource instances.
 */
class ResourceCache {
 public:
  explicit ResourceCache(Context* context);

  /**
   * Returns true if there is no cache at all.
   */
  bool empty() const;

  /**
   * Returns the number of bytes consumed by resources.
   */
  size_t getResourceBytes() const {
    return totalBytes;
  }

  /**
   * Returns the number of bytes held by purgeable resources.
   */
  size_t getPurgeableBytes() const {
    return purgeableBytes;
  }

  /**
   * Returns current cache limits of max gpu memory byte size.
   */
  size_t getCacheLimit() const {
    return maxBytes;
  }

  /**
   * Sets the cache limits of max gpu memory byte size.
   */
  void setCacheLimit(size_t bytesLimit);

  /**
   * Returns a reusable resource in the cache by the specified recycleKey.
   */
  std::shared_ptr<Resource> getRecycled(const BytesKey& recycleKey);

  /**
   * Returns a unique resource in the cache by the specified contentKey.
   */
  std::shared_ptr<Resource> findResourceByOwner(const Cacheable* owner);

  /**
   * Purges GPU resources that haven't been used the passed in time.
   * @param purgeTime A timestamp previously returned by Clock::Now().
   * @param recycledResourcesOnly If it is true the purgeable resources containing persistent data
   * are spared. If it is false then all purgeable resources will be deleted.
   */
  void purgeNotUsedSince(int64_t purgeTime, bool recycledResourcesOnly = false);

  /**
   * Purge unreferenced resources from the cache until the the provided bytesLimit has been reached
   * or we have purged all unreferenced resources. Returns true if the total resource bytes is not
   * over the specified bytesLimit after purging.
   * @param bytesLimit The desired number of bytes after puring.
   * @param recycledResourcesOnly If it is true the purgeable resources containing persistent data
   * are spared. If it is false then all purgeable resources will be deleted.
   */
  bool purgeUntilMemoryTo(size_t bytesLimit, bool recycledResourcesOnly = false);

 private:
  Context* context = nullptr;
  size_t maxBytes = 0;
  size_t totalBytes = 0;
  size_t purgeableBytes = 0;
  bool purgingResource = false;
  std::vector<std::shared_ptr<Resource>> strongReferences = {};
  std::list<Resource*> nonpurgeableResources = {};
  std::list<Resource*> purgeableResources = {};
  std::unordered_map<BytesKey, std::vector<Resource*>, BytesHasher> recycleKeyMap = {};
  std::unordered_map<uint32_t, Resource*> cacheOwnerMap = {};
  std::mutex removeLocker = {};
  std::vector<Resource*> pendingPurgeableResources = {};

  static void AddToList(std::list<Resource*>& list, Resource* resource);
  static void RemoveFromList(std::list<Resource*>& list, Resource* resource);
  static void NotifyReferenceReachedZero(Resource* resource);

  void attachToCurrentThread();
  void detachFromCurrentThread();
  void releaseAll(bool releaseGPU);
  void processUnreferencedResource(Resource* resource);
  std::shared_ptr<Resource> wrapResource(Resource* resource);
  std::shared_ptr<Resource> addResource(Resource* resource);
  void removeResource(Resource* resource);
  void purgeResourcesByLRU(bool recycledResourcesOnly,
                           const std::function<bool(Resource*)>& satisfied);

  void changeCacheOwner(Resource* resource, const Cacheable* owner);
  void removeCacheOwner(Resource* resource);

  friend class Resource;
  friend class Context;
  friend class PurgeGuard;
};
}  // namespace tgfx
