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

#include "tgfx/gpu/ResourceCache.h"

namespace tgfx {
/**
 * The base class for GPU resource. Overrides the onReleaseGPU() method to to free all GPU resources.
 * No backend API calls should be made during destructuring since there may be no GPU context which
 * is current on the calling thread.
 * Note: Resource is not thread safe, do not access any properties of a Resource unless its
 * associated device is locked.
 */
class Resource {
 public:
  template <class T>
  static std::shared_ptr<T> Wrap(Context* context, T* resource) {
    resource->context = context;
    static_cast<Resource*>(resource)->computeRecycleKey(&resource->recycleKey);
    return std::static_pointer_cast<T>(context->resourceCache()->wrapResource(resource));
  }

  virtual ~Resource() = default;

  /**
   * Retrieves the context associated with this Resource.
   */
  Context* getContext() const {
    return context;
  }

 protected:
  Context* context = nullptr;

  /**
   * Overridden to compute a recycleKey to make this Resource reusable.
   */
  virtual void computeRecycleKey(BytesKey*) const {
  }

 private:
  std::weak_ptr<Resource> weakThis;
  BytesKey recycleKey = {};
  size_t cacheArrayIndex = 0;
  int64_t lastUsedTime = 0;

  /**
   * Overridden to free GPU resources in the backend API.
   */
  virtual void onReleaseGPU() = 0;

  friend class ResourceCache;
};
}  // namespace tgfx
