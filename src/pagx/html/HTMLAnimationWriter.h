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
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "pagx/PAGXDocument.h"

namespace pagx {

class HTMLWriterContext;
class Animation;
class Layer;
class SolidColor;
class LayerFilter;
class Path;

/**
 * RAII session that manages PAGX-to-HTML keyframe animation export (`<Animation>` -> CSS
 * `@keyframes` + inline `animation` declarations).
 *
 * When `enabled` is false or neither the document nor a reachable Composition contains playable
 * `<Animation>` nodes, the session is a pure no-op (`isActive() == false`) and leaves
 * `HTMLWriterContext` and `PAGXDocument` completely untouched.
 */
class HTMLAnimationSession {
 public:
  HTMLAnimationSession(PAGXDocument& doc, HTMLWriterContext& ctx, bool enabled);
  ~HTMLAnimationSession();

  HTMLAnimationSession(const HTMLAnimationSession&) = delete;
  HTMLAnimationSession& operator=(const HTMLAnimationSession&) = delete;

  bool isActive() const {
    return _active;
  }

  /**
   * Compiles all active `<Animation>` nodes into CSS `@keyframes` rules and weaves the
   * corresponding `animation: ...` declarations into the target elements in `bodyHtml`.
   * Returns the `@keyframes` CSS text to be appended to the `<style>` block.
   */
  std::string emitKeyframesAndWeave(std::string& bodyHtml);

 private:
  // One <Animation> mount: a document-level animation, or one Composition slot's timeline. Both
  // carry their own timeline offset, so siblings that share a source Composition stay independent.
  struct AnimationMount {
    const Animation* animation = nullptr;
    size_t scope = 0;
    int64_t offset = 0;
    bool playing = true;
  };

  // Resolves every animation object target to the DOM id its tag will carry, claiming and
  // remembering ids for the nodes that need one.
  void ResolveAnimationTargets(const std::vector<const Animation*>& animations);
  // Lists the document-level animations and the timelines mounted by Composition slots.
  std::vector<AnimationMount> CollectAnimationMounts() const;

  // Declarations collected per target element while weaving. They are applied to the body in one
  // pass at the end instead of rewriting the body once per target.
  struct ElementEdits {
    std::vector<std::string> idOrder = {};
    std::unordered_map<std::string, std::vector<std::string>> animDecls = {};
    std::unordered_map<std::string, bool> needsZeroOrigin = {};
    std::unordered_map<std::string, bool> liftsSvgClip = {};
    std::unordered_map<std::string, std::vector<bool>> playStates = {};
  };

  static void ApplyElementEdits(std::string& bodyHtml, const ElementEdits& edits);

  PAGXDocument& _doc;
  HTMLWriterContext& _ctx;
  bool _active = false;

  std::vector<std::pair<Node*, std::string>> _restoredNodeIds = {};
  std::vector<std::pair<Layer*, bool>> _restoredGroupOpacity = {};

  std::unordered_map<const SolidColor*, const Node*> _solidToPainter = {};
  std::unordered_map<const LayerFilter*, const Layer*> _filterToLayer = {};
  std::unordered_map<const Path*, Layer*> _maskPathToLayer = {};
  std::unordered_map<std::string, int64_t> _timelineOffsets = {};
};

}  // namespace pagx
