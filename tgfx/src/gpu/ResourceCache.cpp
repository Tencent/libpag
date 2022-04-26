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
#include "core/utils/Log.h"
#include "tgfx/core/BytesKey.h"
#include "tgfx/core/Clock.h"
#include "tgfx/gpu/Resource.h"

namespace tgfx {
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

ResourceCache::ResourceCache(Context* context) : context(context) {
}

bool ResourceCache::empty() const {
  return nonpurgeableResources.empty() && pendingRemovedResources.empty() &&
         recycledResources.empty();
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
  std::vector<Resource*> removedResources = {};
  std::swap(removedResources, pendingRemovedResources);
  for (auto& resource : removedResources) {
    removeResource(resource);
  }
  DEBUG_ASSERT(pendingRemovedResources.empty());
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
  for (auto& item : recycledResources) {
    for (auto& resource : item.second) {
      if (releaseGPU) {
        resource->onReleaseGPU();
      }
      delete resource;
    }
  }
  recycledResources.clear();
}

void ResourceCache::purgeNotUsedIn(int64_t usNotUsed) {
  PurgeGuard guard(this);
  auto currentTime = Clock::Now();
  std::unordered_map<BytesKey, std::vector<Resource*>, BytesHasher> recycledMap = {};
  for (auto& item : recycledResources) {
    std::vector<Resource*> needToRecycle = {};
    for (auto& resource : item.second) {
      if (currentTime - resource->lastUsedTime < usNotUsed) {
        needToRecycle.push_back(resource);
      } else {
        resource->onReleaseGPU();
        delete resource;
      }
    }
    if (!needToRecycle.empty()) {
      recycledMap[item.first] = needToRecycle;
    }
  }
  recycledResources = recycledMap;
}

std::shared_ptr<Resource> ResourceCache::getRecycled(const BytesKey& resourceKey) {
  auto result = recycledResources.find(resourceKey);
  if (result == recycledResources.end()) {
    return nullptr;
  }
  auto& list = result->second;
  auto resource = list.back();
  list.pop_back();
  if (list.empty()) {
    recycledResources.erase(result);
  }
  return wrapResource(resource);
}

void ResourceCache::AddToList(std::vector<Resource*>& list, Resource* resource) {
  auto index = list.size();
  list.push_back(resource);
  resource->cacheArrayIndex = index;
}

void ResourceCache::RemoveFromList(std::vector<Resource*>& list, Resource* resource) {
  auto tail = *(list.end() - 1);
  auto index = resource->cacheArrayIndex;
  list[index] = tail;
  tail->cacheArrayIndex = index;
  list.pop_back();
}

void ResourceCache::NotifyReferenceReachedZero(Resource* resource) {
  if (resource->context) {
    resource->context->resourceCache()->removeResource(resource);
  } else {
    // Resource 上的属性都是在 Context 加锁的情况下读写的，这里如果 context
    // 为空，说明已经被释放了，可以直接删除。
    delete resource;
  }
}

std::shared_ptr<Resource> ResourceCache::wrapResource(Resource* resource) {
  AddToList(nonpurgeableResources, resource);
  auto result = std::shared_ptr<Resource>(resource, ResourceCache::NotifyReferenceReachedZero);
  result->weakThis = result;
  return result;
}

void ResourceCache::removeResource(Resource* resource) {
  if (currentThreadCaches.count(this) == 0) {
    // 当 strongReferences 列表清空时，其他线程的 Resource 也有可能会触发
    // NotifyReferenceReachedZero()。 如果触发的线程不是当前 context 被锁定时的线程,
    // 先放进一个队列延后处理。
    std::lock_guard<std::mutex> autoLock(removeLocker);
    pendingRemovedResources.push_back(resource);
    return;
  }
  // 禁止 Resource 嵌套，防止 Context 析构时无法释放子项。
  DEBUG_ASSERT(!purgingResource);
  // 只有 context 锁定的情况下才有可能
  // 触发 NotifyReferenceReachedZero()
  DEBUG_ASSERT(context->device()->contextLocked);
  RemoveFromList(nonpurgeableResources, resource);
  if (resource->recycleKey.isValid()) {
    resource->lastUsedTime = Clock::Now();
    recycledResources[resource->recycleKey].push_back(resource);
  } else {
    purgingResource = true;
    resource->onReleaseGPU();
    purgingResource = false;
    delete resource;
  }
}
}  // namespace tgfx
