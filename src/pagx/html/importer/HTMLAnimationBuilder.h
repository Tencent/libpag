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
 * (`alpha` / `x` / `y` on the element's Layer, and `color` on the Layer's solid Fill).
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

 private:
  HTMLDiagnosticSink& _diagnostics;
  HTMLValueParser& _valueParser;
  HTMLIdAllocator& _idAllocator;
  PAGXDocument* _document = nullptr;
  const std::unordered_map<std::string, html::CssKeyframesRule>* _keyframes = nullptr;
};

}  // namespace pagx
