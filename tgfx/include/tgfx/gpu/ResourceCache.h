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
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/ResourceKey.h"

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
   * Returns a scratch resource in the cache by the specified ScratchKey.
   */
  std::shared_ptr<Resource> findScratchResource(const ScratchKey& scratchKey);

  /**
   * Returns a unique resource in the cache by the specified UniqueKey.
   */
  std::shared_ptr<Resource> findUniqueResource(const UniqueKey& uniqueKey);

  /**
   * Returns true if there is a corresponding unique resource for the specified UniqueKey.
   */
  bool hasUniqueResource(const UniqueKey& uniqueKey);

  /**
   * Purges GPU resources that haven't been used the passed in time.
   * @param purgeTime A timestamp previously returned by Clock::Now().
   * @param scratchResourcesOnly If true, the purgeable resources containing unique keys are spared.
   * If false, then all purgeable resources will be deleted.
   */
  void purgeNotUsedSince(int64_t purgeTime, bool scratchResourcesOnly = false);

  /**
   * Purge unreferenced resources from the cache until the the provided bytesLimit has been reached
   * or we have purged all unreferenced resources. Returns true if the total resource bytes is not
   * over the specified bytesLimit after purging.
   * @param bytesLimit The desired number of bytes after puring.
   * @param scratchResourcesOnly If true, the purgeable resources containing unique keys are spared.
   * If false, then all purgeable resources will be deleted.
   */
  bool purgeUntilMemoryTo(size_t bytesLimit, bool scratchResourcesOnly = false);

 private:
  Context* context = nullptr;
  size_t maxBytes = 0;
  size_t totalBytes = 0;
  size_t purgeableBytes = 0;
  bool purgingResource = false;
  std::vector<std::shared_ptr<Resource>> strongReferences = {};
  std::list<Resource*> nonpurgeableResources = {};
  std::list<Resource*> purgeableResources = {};
  ScratchKeyMap<std::vector<Resource*>> scratchKeyMap = {};
  std::unordered_map<uint32_t, Resource*> uniqueKeyMap = {};
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
  void purgeResourcesByLRU(bool scratchResourcesOnly,
                           const std::function<bool(Resource*)>& satisfied);

  void changeUniqueKey(Resource* resource, const UniqueKey& uniqueKey);
  void removeUniqueKey(Resource* resource);
  Resource* getUniqueResource(const UniqueKey& uniqueKey);

  friend class Resource;
  friend class Context;
  friend class PurgeGuard;
};
}  // namespace tgfx
