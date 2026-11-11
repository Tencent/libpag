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
#include <unordered_map>
#include <vector>
#include "pagx/SVGExporter.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Node.h"

namespace pagx {

class Animation;
class AnimationTimeline;
class PAGXDocument;

/**
 * Binds one animated Channel to the PAGX node it drives together with the Animation (duration,
 * frame rate, loop mode) and the AnimationTimeline (start offset, initial playing state) that
 * supply the SMIL timing. The timeline pointer may be null when the animation is reached through a
 * Composition rather than a Layer timeline.
 */
struct SVGAnimationBinding {
  const Channel* channel = nullptr;
  const Animation* animation = nullptr;
  const AnimationTimeline* timeline = nullptr;
};

/**
 * SVGAnimationPlan is the pre-pass result consumed by SVGExporter. It answers two questions the
 * streaming SVG writer cannot answer on its own: which channels drive a given PAGX node, and
 * whether a node needs an SMIL host element at all.
 *
 * The plan only carries bindings for channels SVG can express as native SMIL: Layer alpha /
 * visible, the Layer and Group transform channels, and the Fill / Stroke paint channels. Every
 * other animated channel is reported through the warnings sink so batch pipelines can detect
 * silent drops.
 */
class SVGAnimationPlan {
 public:
  /**
   * Resolves every active timeline in the document into per-node channel bindings. Returns a
   * non-null plan even when the document has no animations; callers can test empty().
   */
  static std::unique_ptr<SVGAnimationPlan> Build(PAGXDocument* document,
                                                 std::vector<std::string>* warnings,
                                                 const SVGAnimationOptions& options = {});

  bool empty() const {
    return _bindings.empty();
  }

  bool rejected() const {
    return _rejected;
  }

  /**
   * Number of animations that were dropped before producing a single track: non-positive duration
   * or frame rate, or no channel the exporter can express. Used by the coverage tests to prove a
   * document is not silently reported as animated when nothing was emitted.
   */
  size_t SkippedAnimationCount() const {
    return _skippedAnimations;
  }

  /**
   * Returns every binding that targets `node`, or nullptr when the node is not animated.
   */
  const std::vector<SVGAnimationBinding>* BindingsFor(const Node* node) const;

  /**
   * Returns the binding for `channelName` on `node`, or nullptr. Later bindings for the same
   * channel win, matching the runtime's last-writer-wins apply order.
   */
  const SVGAnimationBinding* Find(const Node* node, const char* channelName) const;

  bool HasVisibilityAnimation(const Node* node) const;

  /**
   * True when any transform component of `node` (Layer or Group / TextBox) is animated.
   */
  bool HasTransformAnimation(const Node* node) const;

  /**
   * True when `node` has an animated alpha channel.
   */
  bool HasAlphaAnimation(const Node* node) const;

  /**
   * True when any channel of the Fill or Stroke painter is animated. Null painters are ignored.
   */
  bool HasPainterAnimation(const Node* fill, const Node* stroke) const;

  /**
   * True when `node` is a Layer or Group / TextBox whose transform channels are animated.
   */
  static bool IsTransformChannel(NodeType nodeType, const std::string& channelName);

 private:
  SVGAnimationPlan() = default;

  std::unordered_map<const Node*, std::vector<SVGAnimationBinding>> _bindings = {};
  size_t _bindingCount = 0;
  size_t _skippedAnimations = 0;
  bool _rejected = false;
};

}  // namespace pagx
