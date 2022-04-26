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

#include <unordered_map>
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
   * Returns a reusable resource in the cache.
   */
  std::shared_ptr<Resource> getRecycled(const BytesKey& recycleKey);

  /**
   * Purges GPU resources that haven't been used in the past 'usNotUsed' microseconds.
   */
  void purgeNotUsedIn(int64_t usNotUsed);

 private:
  Context* context = nullptr;
  bool purgingResource = false;
  std::vector<Resource*> nonpurgeableResources = {};
  std::vector<std::shared_ptr<Resource>> strongReferences = {};
  std::unordered_map<BytesKey, std::vector<Resource*>, BytesHasher> recycledResources = {};
  std::mutex removeLocker = {};
  std::vector<Resource*> pendingRemovedResources = {};

  static void AddToList(std::vector<Resource*>& list, Resource* resource);
  static void RemoveFromList(std::vector<Resource*>& list, Resource* resource);
  static void NotifyReferenceReachedZero(Resource* resource);

  void attachToCurrentThread();
  void detachFromCurrentThread();
  void releaseAll(bool releaseGPU);
  std::shared_ptr<Resource> wrapResource(Resource* resource);
  void removeResource(Resource* resource);

  friend class Resource;
  friend class Context;
  friend class PurgeGuard;
};
}  // namespace tgfx
