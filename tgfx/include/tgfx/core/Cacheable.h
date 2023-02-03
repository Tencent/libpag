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

#include <memory>

namespace tgfx {
/**
 * The base class for CPU objects that can generate GPU caches. The content of a Cacheable is
 * immutable. Releasing an Cacheable will immediately mark the corresponding GPU cache in the
 * context as expired, which becomes recyclable and will be purged at some point in the future.
 */
class Cacheable {
 public:
  virtual ~Cacheable() = default;

  /**
   * Returns a global unique ID for this Cacheable. The content of a Cacheable cannot change after
   * it is created. Any operation to create a new Cacheable will receive generate a new unique ID.
   */
  uint32_t uniqueID() const {
    return _uniqueID;
  }

 protected:
  std::weak_ptr<Cacheable> weakThis;

  Cacheable();

 private:
  uint32_t _uniqueID = 0;

  friend class ResourceCache;
  friend class ProxyProvider;
  friend class TextureProxy;
};
}  // namespace tgfx
