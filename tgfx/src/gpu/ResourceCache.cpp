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

#include "tgfx/gpu/ResourceCache.h"
#include <unordered_map>
#include <unordered_set>
#include "tgfx/core/BytesKey.h"
#include "tgfx/core/Clock.h"
#include "tgfx/gpu/Resource.h"
#include "utils/Log.h"

namespace tgfx {
// Default maximum number of bytes of gpu memory of budgeted resources in the cache.
static const size_t DefaultMaxBytes = 96 * (1 << 20);

static thread_local std::unordered_set<ResourceCache*> currentThreadCaches = {};

class PurgeGuard {
 public:
  explicit PurgeGuard(ResourceCache* cache) : cache(cache) {
    cache->purgingResource = true;
  }

  ~PurgeGuard() {
    cache->purgingResource = false;
  }

 private:
  ResourceCache* cache = nullptr;
};

ResourceCache::ResourceCache(Context* context) : context(context), maxBytes(DefaultMaxBytes) {
}

bool ResourceCache::empty() const {
  return nonpurgeableResources.empty() && pendingPurgeableResources.empty() &&
         purgeableResources.empty();
}

void ResourceCache::attachToCurrentThread() {
  currentThreadCaches.insert(this);
  // Triggers NotifyReferenceReachedZero() if a Resource has no other reference.
  strongReferences.clear();
}

void ResourceCache::detachFromCurrentThread() {
  for (auto& resource : nonpurgeableResources) {
    // Adds a strong reference to the external Resource to make sure it never triggers
    // NotifyReferenceReachedZero() while associated device is not locked.
    auto strongReference = resource->weakThis.lock();
    if (strongReference) {
      strongReferences.push_back(strongReference);
    }
  }
  // strongReferences 保护已经开启，这时候不会再有任何外部的 Resource 会触发
  // NotifyReferenceReachedZero()。
  std::vector<Resource*> pendingResources = {};
  std::swap(pendingResources, pendingPurgeableResources);
  for (auto& resource : pendingResources) {
    processUnreferencedResource(resource);
  }
  DEBUG_ASSERT(pendingPurgeableResources.empty());
  currentThreadCaches.erase(this);
}

void ResourceCache::releaseAll(bool releaseGPU) {
  PurgeGuard guard(this);
  for (auto& resource : nonpurgeableResources) {
    if (releaseGPU) {
      resource->onReleaseGPU();
    }
    // 标记 Resource 已经被释放，等外部指针计数为 0 时可以直接 delete。
    resource->context = nullptr;
  }
  nonpurgeableResources.clear();
  for (auto& resource : purgeableResources) {
    if (releaseGPU) {
      resource->onReleaseGPU();
    }
    delete resource;
  }
  purgeableResources.clear();
  recycleKeyMap.clear();
  cacheOwnerMap.clear();
  purgeableBytes = 0;
  totalBytes = 0;
}

void ResourceCache::setCacheLimit(size_t bytesLimit) {
  if (maxBytes == bytesLimit) {
    return;
  }
  maxBytes = bytesLimit;
  purgeUntilMemoryTo(maxBytes);
}

std::shared_ptr<Resource> ResourceCache::getRecycled(const BytesKey& recycleKey) {
  auto result = recycleKeyMap.find(recycleKey);
  if (result == recycleKeyMap.end()) {
    return nullptr;
  }
  auto& list = result->second;
  int index = 0;
  bool found = false;
  for (auto& resource : list) {
    if (resource->isPurgeable() && !resource->hasCacheOwner()) {
      found = true;
      break;
    }
    index++;
  }
  if (!found) {
    return nullptr;
  }
  auto resource = list[index];
  RemoveFromList(purgeableResources, resource);
  purgeableBytes -= resource->memoryUsage();
  return wrapResource(resource);
}

std::shared_ptr<Resource> ResourceCache::findResourceByOwner(const Cacheable* owner) {
  if (owner == nullptr) {
    return nullptr;
  }
  auto result = cacheOwnerMap.find(owner->uniqueID());
  if (result == cacheOwnerMap.end()) {
    return nullptr;
  }
  auto resource = result->second;
  if (!resource->hasCacheOwner()) {
    cacheOwnerMap.erase(result);
    return nullptr;
  }
  if (resource->isPurgeable()) {
    RemoveFromList(purgeableResources, resource);
    purgeableBytes -= resource->memoryUsage();
    return wrapResource(resource);
  }
  return resource->weakThis.lock();
}

bool ResourceCache::hasCache(const Cacheable* owner) {
  if (owner == nullptr) {
    return false;
  }
  return cacheOwnerMap.count(owner->uniqueID()) > 0;
}

void ResourceCache::AddToList(std::list<Resource*>& list, Resource* resource) {
  list.push_front(resource);
  resource->cachedPosition = list.begin();
}

void ResourceCache::RemoveFromList(std::list<Resource*>& list, Resource* resource) {
  list.erase(resource->cachedPosition);
}

void ResourceCache::NotifyReferenceReachedZero(Resource* resource) {
  if (resource->context) {
    resource->context->resourceCache()->processUnreferencedResource(resource);
  } else {
    // Resource 上的属性都是在 Context 加锁的情况下读写的，这里如果 context
    // 为空，说明已经被释放了，可以直接删除。
    delete resource;
  }
}

void ResourceCache::processUnreferencedResource(Resource* resource) {
  if (currentThreadCaches.count(this) == 0) {
    // 当 strongReferences 列表清空时，其他线程的 Resource 也有可能会触发
    // NotifyReferenceReachedZero()。 如果触发的线程不是当前 context 被锁定时的线程,
    // 先放进一个队列延后处理。
    std::lock_guard<std::mutex> autoLock(removeLocker);
    pendingPurgeableResources.push_back(resource);
    return;
  }
  // 禁止 Resource 嵌套，防止 Context 析构时无法释放子项。
  DEBUG_ASSERT(!purgingResource);
  // 只有 context 锁定的情况下才有可能
  // 触发 NotifyReferenceReachedZero()
  DEBUG_ASSERT(context->device()->contextLocked);
  RemoveFromList(nonpurgeableResources, resource);
  if (resource->recycleKey.isValid()) {
    auto hasBudget = purgeUntilMemoryTo(maxBytes - resource->memoryUsage());
    if (hasBudget) {
      AddToList(purgeableResources, resource);
      purgeableBytes += resource->memoryUsage();
      resource->lastUsedTime = Clock::Now();
      return;
    }
  }
  removeResource(resource);
}

void ResourceCache::changeCacheOwner(Resource* resource, const Cacheable* owner) {
  auto result = cacheOwnerMap.find(owner->uniqueID());
  if (result != cacheOwnerMap.end()) {
    result->second->removeCacheOwner();
  }
  if (resource->cacheOwnerID > 0) {
    cacheOwnerMap.erase(resource->cacheOwnerID);
  }
  resource->cacheOwnerID = owner->uniqueID();
  resource->cacheOwner = owner->weakThis;
  cacheOwnerMap[owner->uniqueID()] = resource;
}

void ResourceCache::removeCacheOwner(Resource* resource) {
  cacheOwnerMap.erase(resource->cacheOwnerID);
  resource->cacheOwnerID = 0;
  resource->cacheOwner.reset();
}

std::shared_ptr<Resource> ResourceCache::wrapResource(Resource* resource) {
  AddToList(nonpurgeableResources, resource);
  auto result = std::shared_ptr<Resource>(resource, ResourceCache::NotifyReferenceReachedZero);
  result->weakThis = result;
  return result;
}

std::shared_ptr<Resource> ResourceCache::addResource(Resource* resource) {
  if (resource->recycleKey.isValid()) {
    recycleKeyMap[resource->recycleKey].push_back(resource);
  }
  totalBytes += resource->memoryUsage();
  return wrapResource(resource);
}

void ResourceCache::removeResource(Resource* resource) {
  if (resource->cacheOwnerID > 0) {
    cacheOwnerMap.erase(resource->cacheOwnerID);
  }
  if (resource->recycleKey.isValid()) {
    auto result = recycleKeyMap.find(resource->recycleKey);
    if (result != recycleKeyMap.end()) {
      auto& list = result->second;
      list.erase(std::remove(list.begin(), list.end(), resource), list.end());
      if (list.empty()) {
        recycleKeyMap.erase(resource->recycleKey);
      }
    }
  }
  purgingResource = true;
  resource->onReleaseGPU();
  purgingResource = false;
  totalBytes -= resource->memoryUsage();
  if (resource->isPurgeable()) {
    delete resource;
  } else {
    // 标记 Resource 已经被释放，等外部指针计数为 0 时可以直接 delete。
    resource->context = nullptr;
  }
}

void ResourceCache::purgeNotUsedSince(int64_t purgeTime, bool recycledResourcesOnly) {
  purgeResourcesByLRU(recycledResourcesOnly,
                      [&](Resource* resource) { return resource->lastUsedTime >= purgeTime; });
}

bool ResourceCache::purgeUntilMemoryTo(size_t bytesLimit, bool recycledResourcesOnly) {
  purgeResourcesByLRU(recycledResourcesOnly, [&](Resource*) { return totalBytes <= bytesLimit; });
  return totalBytes <= bytesLimit;
}

void ResourceCache::purgeResourcesByLRU(bool recycledResourcesOnly,
                                        const std::function<bool(Resource*)>& satisfied) {
  std::vector<Resource*> needToPurge = {};
  for (auto item = purgeableResources.rbegin(); item != purgeableResources.rend(); item++) {
    auto* resource = *item;
    if (satisfied(resource)) {
      break;
    }
    if (!recycledResourcesOnly || !resource->hasCacheOwner()) {
      needToPurge.push_back(resource);
    }
  }
  for (auto& resource : needToPurge) {
    RemoveFromList(purgeableResources, resource);
    purgeableBytes -= resource->memoryUsage();
    removeResource(resource);
  }
}
}  // namespace tgfx
