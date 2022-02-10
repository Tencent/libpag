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

#include "gpu/Context.h"
#include "GradientCache.h"
#include "Program.h"
#include "base/utils/GetTimer.h"
#include "gpu/Resource.h"

namespace pag {
#define MAX_PROGRAM_COUNT 128
static thread_local Context* threadCurrentContext = nullptr;

class PurgeGuard {
 public:
  explicit PurgeGuard(Context* context) : context(context) {
    context->purgingResource = true;
  }

  ~PurgeGuard() {
    context->purgingResource = false;
  }

 private:
  Context* context = nullptr;
};

Context::Context(Device* device) : device(device) {
  gradientCache = new GradientCache(this);
}

Context::~Context() {
  // The Device owner must call releaseAll() before deleting this Context, otherwise, GPU resources
  // may leak.
  DEBUG_ASSERT(nonpurgeableResources.empty());
  DEBUG_ASSERT(pendingRemovedResources.empty());
  DEBUG_ASSERT(recycledResources.empty());
  DEBUG_ASSERT(programMap.empty());
  DEBUG_ASSERT(gradientCache->empty())
  delete gradientCache;
}

Device* Context::getDevice() const {
  return device;
}

void Context::onLocked() {
  threadCurrentContext = this;
  // Triggers NotifyReferenceReachedZero() if a Resource has no other reference.
  strongReferences.clear();
}

void Context::onUnlocked() {
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
  threadCurrentContext = nullptr;
}

void Context::purgeResourcesNotUsedIn(int64_t usNotUsed) {
  PurgeGuard guard(this);
  auto currentTime = GetTimer();
  std::unordered_map<BytesKey, std::vector<Resource*>, BytesHasher> recycledMap = {};
  for (auto& item : recycledResources) {
    std::vector<Resource*> needToRecycle = {};
    for (auto& resource : item.second) {
      if (currentTime - resource->lastUsedTime < usNotUsed) {
        needToRecycle.push_back(resource);
      } else {
        resource->onRelease(this);
        delete resource;
      }
    }
    if (!needToRecycle.empty()) {
      recycledMap[item.first] = needToRecycle;
    }
  }
  recycledResources = recycledMap;
}

void Context::releaseAll(bool releaseGPU) {
  if (gradientCache) {
    gradientCache->releaseAll();
  }
  PurgeGuard guard(this);
  for (auto& resource : nonpurgeableResources) {
    if (releaseGPU) {
      resource->onRelease(this);
    }
    // 标记 Resource 已经被释放，等外部指针计数为 0 时可以直接 delete。
    resource->context = nullptr;
  }
  nonpurgeableResources.clear();
  while (!programLRU.empty()) {
    removeOldestProgram(releaseGPU);
  }
  for (auto& item : recycledResources) {
    for (auto& resource : item.second) {
      if (releaseGPU) {
        resource->onRelease(this);
      }
      delete resource;
    }
  }
  recycledResources.clear();
}

Program* Context::getProgram(const ProgramCreator* programMaker) {
  BytesKey uniqueKey = {};
  programMaker->computeUniqueKey(this, &uniqueKey);
  auto result = programMap.find(uniqueKey);
  if (result != programMap.end()) {
    programLRU.remove(result->second);
    programLRU.push_front(result->second);
    return result->second;
  }
  // TODO(domrjchen): createProgram() 应该统计到 programCompilingTime 里。
  auto program = programMaker->createProgram(this).release();
  if (program == nullptr) {
    return nullptr;
  }
  program->uniqueKey = uniqueKey;
  programLRU.push_front(program);
  programMap[uniqueKey] = program;
  while (programLRU.size() > MAX_PROGRAM_COUNT) {
    removeOldestProgram();
  }
  return program;
}

const Texture* Context::getGradient(const Color4f* colors, const float* positions, int count) {
  return gradientCache->getGradient(colors, positions, count);
}

std::shared_ptr<Resource> Context::getRecycledResource(const BytesKey& resourceKey) {
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

void Context::AddToList(std::vector<Resource*>& list, Resource* resource) {
  auto index = list.size();
  list.push_back(resource);
  resource->cacheArrayIndex = index;
}

void Context::RemoveFromList(std::vector<Resource*>& list, Resource* resource) {
  auto tail = *(list.end() - 1);
  auto index = resource->cacheArrayIndex;
  list[index] = tail;
  tail->cacheArrayIndex = index;
  list.pop_back();
}

void Context::NotifyReferenceReachedZero(Resource* resource) {
  if (resource->context) {
    resource->context->removeResource(resource);
  } else {
    // Resource 上的属性都是在 Context 加锁的情况下读写的，这里如果 context
    // 为空，说明已经被释放了，可以直接删除。
    delete resource;
  }
}

std::shared_ptr<Resource> Context::wrapResource(Resource* resource) {
  AddToList(nonpurgeableResources, resource);
  auto result = std::shared_ptr<Resource>(resource, Context::NotifyReferenceReachedZero);
  result->weakThis = result;
  return result;
}

void Context::removeResource(Resource* resource) {
  if (threadCurrentContext != this) {
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
  DEBUG_ASSERT(device->contextLocked);
  RemoveFromList(nonpurgeableResources, resource);
  if (resource->recycleKey.isValid()) {
    resource->lastUsedTime = GetTimer();
    recycledResources[resource->recycleKey].push_back(resource);
  } else {
    purgingResource = true;
    resource->onRelease(this);
    purgingResource = false;
    delete resource;
  }
}

void Context::removeOldestProgram(bool releaseGPU) {
  auto program = programLRU.back();
  programLRU.pop_back();
  programMap.erase(program->uniqueKey);
  if (releaseGPU) {
    program->onRelease(this);
  }
  delete program;
}
}  // namespace pag