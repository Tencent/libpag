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

namespace pagx {

class HTMLDiagnosticSink;
class HTMLIdAllocator;
class HTMLValueParser;
class Layer;
class PAGXDocument;

namespace html {
struct CssKeyframesRule;
}

/**
 * Maps the HTML animation subset (`@keyframes` + the `animation` shorthand, see
 * `spec/html_subset.md` §13) onto the PAGX animation model. For each animated element the builder
 * emits an `Animation` -> `AnimationObject` -> `Channel` -> `Keyframe` graph into
 * `PAGXDocument::animations`, restricted to the channels the runtime can play back
 * (`alpha` / `x` / `y` on the element's Layer for opacity and pure translation; the full affine
 * `matrix` channel for scale / rotate / skew transforms; and `color` on the Layer's solid Fill).
 *
 * The builder borrows the importer's diagnostic sink, value parser and id allocator; the document
 * handle and the `@keyframes` registry are bound after the document and cascade exist.
 */
class HTMLAnimationBuilder {
 public:
  HTMLAnimationBuilder(HTMLDiagnosticSink& sink, HTMLValueParser& valueParser,
                       HTMLIdAllocator& idAllocator);

  void bindDocument(PAGXDocument* document);

  /** Wires the `@keyframes` registry collected by the cascade. Pointer must outlive the builder. */
  void setKeyframes(const std::unordered_map<std::string, html::CssKeyframesRule>* keyframes);

  /**
   * Builds and registers an animation for `layer` from the element's resolved `animation`
   * shorthand / longhand declarations. No-op (returns false) when the element declares no
   * animation, the referenced `@keyframes` is unknown, or no playable channel could be produced.
   * `layer` receives a generated `id` when it has none so the emitted `Object target` can
   * reference it.
   */
  bool buildForElement(const std::unordered_map<std::string, std::string>& resolvedStyle,
                       Layer* layer);

  /**
   * Builds and registers animations for a single inline-SVG shape (`<path>`, `<rect>`, …) that
   * declares one or more CSS animations. The SVG importer materialises the shape as its own `Layer`
   * (keyed by the shape's DOM `id`, passed here as `shapeTargetId`) whose `Fill` / `Stroke` painters
   * get the derived ids `<id>__fill` / `<id>__stroke`. Painter-space properties (`fill`,
   * `fill-opacity`, `stroke`, `stroke-opacity`, `stroke-dashoffset`) target those painter ids;
   * layer-space properties (`opacity` -> `alpha`, and a pure-translate `transform` -> `x` / `y`)
   * target the shape's `Layer` directly. The shape layer sits at layout origin in the SVG user
   * (view-box) space, so translate keyframes — captured in that same space via `transform-box:
   * view-box` — map onto `x` / `y` verbatim. Non-translate transforms (scale / rotate / skew) have
   * no Layer channel and are dropped with a diagnostic. Honours the full comma-separated `animation`
   * list (each entry becomes its own `Animation`). `dashScale` (real path length / author
   * `pathLength`) rescales `stroke-dashoffset` keyframes into user units, mirroring the static dash
   * scaling the SVG importer applies. Returns true when at least one channel was emitted.
   */
  bool buildForInlineSvgShape(const std::unordered_map<std::string, std::string>& style,
                              const std::string& shapeTargetId, const std::string& fillTargetId,
                              const std::string& strokeTargetId, float dashScale);

 private:
  HTMLDiagnosticSink& _diagnostics;
  HTMLValueParser& _valueParser;
  HTMLIdAllocator& _idAllocator;
  PAGXDocument* _document = nullptr;
  const std::unordered_map<std::string, html::CssKeyframesRule>* _keyframes = nullptr;
};

}  // namespace pagx
