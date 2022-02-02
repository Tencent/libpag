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

#include <list>
#include <unordered_map>
#include "Caps.h"
#include "Color4f.h"
#include "base/utils/BytesKey.h"
#include "base/utils/UniqueID.h"
#include "gpu/Device.h"
#include "pag/gpu.h"

namespace pag {

class Resource;

class Texture;

class Program;

class ProgramCreator;

class GradientCache;

class Context {
 public:
  virtual ~Context();

  /**
   * Returns the unique ID of the associated device.
   */
  Device* getDevice() const;

  /**
   * Returns a program cache of specified ProgramMaker. If there is no associated cache available,
   * a new program will be created by programMaker. Returns null if the programMaker fails to make a
   * new program.
   */
  Program* getProgram(const ProgramCreator* programMaker);

  const Texture* getGradient(const Color4f* colors, const float* positions, int count);

  /**
   * Returns a reusable resource in the cache.
   */
  std::shared_ptr<Resource> getRecycledResource(const BytesKey& recycleKey);

  /**
   * Purges GPU resources that haven't been used in the past 'usNotUsed' microseconds.
   */
  void purgeResourcesNotUsedIn(int64_t usNotUsed);

  /**
   * Returns the GPU backend of this context.
   */
  virtual Backend backend() const = 0;

  /**
   * Returns the capabilities info of this context.
   */
  virtual const Caps* caps() const = 0;

 protected:
  explicit Context(Device* device);

 private:
  Device* device = nullptr;
  bool purgingResource = false;
  std::list<Program*> programLRU = {};
  std::unordered_map<BytesKey, Program*, BytesHasher> programMap = {};
  GradientCache* gradientCache = nullptr;
  std::vector<Resource*> nonpurgeableResources = {};
  std::vector<std::shared_ptr<Resource>> strongReferences = {};
  std::unordered_map<BytesKey, std::vector<Resource*>, BytesHasher> recycledResources = {};
  std::mutex removeLocker = {};
  std::vector<Resource*> pendingRemovedResources = {};

  static void AddToList(std::vector<Resource*>& list, Resource* resource);
  static void RemoveFromList(std::vector<Resource*>& list, Resource* resource);
  static void NotifyReferenceReachedZero(Resource* resource);

  std::shared_ptr<Resource> wrapResource(Resource* resource);
  void removeResource(Resource* resource);
  void removeOldestProgram(bool releaseGPU = true);
  void releaseAll(bool releaseGPU);
  void onLocked();
  void onUnlocked();

  friend class Device;

  friend class Resource;

  friend class PurgeGuard;
};

}  // namespace pag
