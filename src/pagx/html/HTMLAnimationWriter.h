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

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "pagx/PAGXDocument.h"

namespace pagx {

class HTMLWriterContext;
class Layer;
class Group;
class Fill;
class Stroke;
class SolidColor;
class LayerFilter;
class Path;

/**
 * RAII session that manages PAGX-to-HTML keyframe animation export (`<Animation>` -> CSS
 * `@keyframes` + inline `animation` declarations).
 *
 * When `enabled` is false or the document contains no playable `<Animation>` nodes, the session is
 * a pure no-op (`isActive() == false`) and leaves `HTMLWriterContext` and `PAGXDocument`
 * completely untouched.
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
  PAGXDocument& _doc;
  HTMLWriterContext& _ctx;
  bool _active = false;

  std::vector<std::pair<Node*, std::string>> _restoredNodeIds = {};
  std::vector<std::pair<Layer*, bool>> _restoredGroupOpacity = {};
  std::vector<std::pair<Group*, float>> _restoredGroupAlpha = {};

  std::unordered_map<const SolidColor*, const Node*> _solidToPainter = {};
  std::unordered_map<const LayerFilter*, const Layer*> _filterToLayer = {};
  std::unordered_map<const Path*, const Layer*> _maskPathToLayer = {};
};

}  // namespace pagx
