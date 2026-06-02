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
 * PAGComposition is the runtime instance of a composition. It can drive the animations declared
 * inside the composition and answer hit-test queries against its content. When the same composition
 * is referenced from multiple places, each reference produces an independent PAGComposition with
 * its own playback state, so animating or hit-testing one does not affect the others.
 *
 * PAGFile is a PAGComposition, so a whole file is driven and queried through the same interface.
 */
class PAGComposition {
 public:
  virtual ~PAGComposition();

  /**
   * Advances the animations that play automatically inside this composition, including those in
   * nested child compositions, by the given amount of time.
   * @param deltaMicroseconds the elapsed time in microseconds. May be negative.
   */
  void advance(int64_t deltaMicroseconds);

  /**
   * Applies the current state of the animations that play automatically inside this composition,
   * including those in nested child compositions, to the displayed content.
   * @param mix blend weight, defaults to 1.0 (full overwrite).
   */
  void apply(float mix = 1.0f);

  /**
   * Returns the layers under the given point. The first layer in the array is the top-most under
   * the point, the last is the bottom-most. Returns an empty array if nothing is hit. The point is
   * in this composition's coordinate space; PAGFile::getLayersUnderPoint takes surface coordinates
   * instead. Hit testing does not require the content to have been drawn first.
   * @param x the x coordinate in the composition coordinate space.
   * @param y the y coordinate in the composition coordinate space.
   */
  std::vector<std::shared_ptr<PAGLayer>> getLayersUnderPoint(float x, float y);

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
