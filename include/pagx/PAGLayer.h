/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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
#include <string>

namespace pagx {

class Layer;

/**
 * PAGLayer is a runtime handle to a single Layer node within a PAGComposition. It is returned by
 * hit testing to identify the layer under a point. This is a minimal handle in v1: it exposes the
 * source layer's name; richer per-instance queries will be added alongside the full PAGLayer
 * runtime.
 */
class PAGLayer {
 public:
  ~PAGLayer();

  /**
   * Returns the display name of the source Layer this handle refers to. Returns an empty string if
   * the source layer is unavailable.
   */
  std::string getName() const;

 private:
  PAGLayer();

  // Creates a PAGLayer handle for the given source Layer node. Returns nullptr if node is null.
  // Used by PAGComposition hit testing to wrap a resolved layer node.
  static std::shared_ptr<PAGLayer> Wrap(const Layer* node);

  struct Impl;
  std::unique_ptr<Impl> impl;

  friend class PAGComposition;
};

}  // namespace pagx
