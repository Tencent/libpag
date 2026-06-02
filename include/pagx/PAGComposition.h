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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pagx {

class Layer;
class PAGFile;
class PAGLayer;
class PAGTimeline;

/**
 * PAGComposition is the runtime instance backing a single Layer.composition reference. Each
 * instance owns its own layer subtree and timeline list, so multiple Layers referencing the same
 * Composition keep independent state. Child compositions are constructed recursively.
 *
 * PAGFile is itself a PAGComposition: the file's top-level runtime subtree is the base
 * composition's data, and the file's nested compositions are this base's child compositions.
 */
class PAGComposition {
 public:
  virtual ~PAGComposition();

  /**
   * Advances this composition's own spawned timelines, then recursively advances every child
   * composition, so the advance delta propagates to all timelines in the composition subtree.
   * @param deltaMicroseconds the elapsed time in microseconds. May be negative.
   */
  void advance(int64_t deltaMicroseconds);

  /**
   * Applies this composition's own spawned timelines, then recursively applies every child
   * composition.
   * @param mix blend weight, defaults to 1.0 (full overwrite).
   */
  void apply(float mix = 1.0f);

  /**
   * Returns the top-most PAGLayer under the given point, or nullptr if nothing is hit. Coordinates
   * are in this composition's root coordinate space (for PAGFile this is the surface/display space
   * before zoom and contentOffset are applied — see PAGFile::hitTest, which performs that
   * conversion). Hit testing uses layer bounding boxes; it does not require a prior GPU render.
   * @param x the x coordinate in the composition root coordinate space.
   * @param y the y coordinate in the composition root coordinate space.
   */
  std::shared_ptr<PAGLayer> hitTest(float x, float y);

 protected:
  PAGComposition();

  // Base runtime state (root subtree, binding, spawned timelines, child compositions). The
  // concrete type lives in PAGComposition.cpp to keep tgfx and RuntimeBinding out of this header.
  struct Impl;
  std::unique_ptr<Impl> composition;

  // Builds a runtime child composition for ownerLayer.composition. Returns nullptr if ownerLayer
  // is null, has no composition, or the document is not laid out. Internal factory used by
  // PAGFile and PAGComposition to populate child compositions.
  static std::unique_ptr<PAGComposition> MakeChild(const Layer* ownerLayer, PAGFile* parentFile);

  friend class PAGFile;
  friend class PAGTimeline;
};

}  // namespace pagx
