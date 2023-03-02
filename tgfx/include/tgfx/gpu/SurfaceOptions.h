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

namespace tgfx {
/**
 * Describes properties and constraints of a given Surface. The rendering engine can parse these
 * during drawing, and can sometimes optimize its performance (e.g. disabling an expensive feature).
 */
class SurfaceOptions {
 public:
  /**
   * If this flag is set, the Surface will skip generating new caches to the associated Context for
   * Cacheable objects during drawing.
   */
  static constexpr uint32_t DisableCacheFlag = 1 << 0;

  /**
   * If this flag is set, the Surface will perform all CPU-side tasks in the current thread rather
   * than run them in parallel asynchronously.
   */
  static constexpr uint32_t DisableAsyncTaskFlag = 1 << 1;

  SurfaceOptions() = default;

  SurfaceOptions(uint32_t flags) : _flags(flags) {
  }

  uint32_t flags() const {
    return _flags;
  }

  bool cacheDisabled() const {
    return _flags & DisableCacheFlag;
  }

  bool asyncTaskDisabled() const {
    return _flags & DisableAsyncTaskFlag;
  }

  bool operator==(const SurfaceOptions& that) const {
    return _flags == that._flags;
  }

 private:
  uint32_t _flags = 0;
};

}  // namespace tgfx
