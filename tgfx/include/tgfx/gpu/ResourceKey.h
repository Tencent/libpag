/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "tgfx/utils/BytesKey.h"

namespace tgfx {
/**
 * A key used for scratch resources. There are three important rules about scratch keys:
 *
 *    1) Multiple resources can share the same scratch key. Therefore, resources assigned the same
 *       scratch key should be interchangeable with respect to the code that uses them.
 *    2) A resource can have at most one scratch key, and it is set at resource creation by the
 *       resource itself.
 *    3) When a scratch resource is referenced, it will not be returned from the cache for a
 *       subsequent cache request until all refs are released. This facilitates using a scratch key
 *       for multiple render-to-texture scenarios.
 */
using ScratchKey = BytesKey;

template <typename T>
using ScratchKeyMap = BytesKeyMap<T>;

class UniqueData;

/**
 * A key that allows for exclusive use of a resource for a use case (AKA "domain"). There are three
 * rules governing the use of unique keys:
 *
 *    1) Only one resource can have a given unique key at a time. Hence, "unique".
 *    2) A resource can have at most one unique key at a time.
 *    3) Unlike scratch keys, multiple requests for a unique key will return the same resource even
 *       if the resource already has refs.
 *
 * This key type allows a code path to create cached resources for which it is the exclusive user.
 * The code path creates a domain which it sets on its keys. This guarantees that there are no
 * cross-domain collisions. Unique keys preempt scratch keys. While a resource has a unique key, it
 * is inaccessible via its scratch key. It can become scratch again if the unique key is removed.
 */
class UniqueKey {
 public:
  /**
   * Creates a new valid UniqueKey.
   */
  static UniqueKey Next();

  /**
   * Creates an empty UniqueKey.
   */
  UniqueKey() = default;

  /**
   * Returns a global unique ID for the UniqueKey.
   */
  uint32_t uniqueID() const {
    return id == nullptr ? 0 : *id;
  }

  /**
   * Returns true if the UniqueKey is empty.
   */
  bool empty() const {
    return id == nullptr;
  }

  /**
   * Returns true if the UniqueKey has only one valid reference.
   */
  bool unique() const {
    return id.unique();
  }

  /**
   * Returns true if a is equivalent to b.
   */
  friend bool operator==(const UniqueKey& a, const UniqueKey& b) {
    return a.id == b.id;
  }

  /**
   * Returns true if a is not equivalent to b.
   */
  friend bool operator!=(const UniqueKey& a, const UniqueKey& b) {
    return a.id != b.id;
  }

 private:
  std::shared_ptr<uint32_t> id = nullptr;
};
}  // namespace tgfx
