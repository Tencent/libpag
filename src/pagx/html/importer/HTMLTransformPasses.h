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
#include "pagx/html/importer/HTMLTransformContext.h"

namespace pagx::html {

/**
 * Pass 1 — DocumentSkeleton.
 *
 * Ensures the tree has the expected `<html><head>?<body></body></html>` shape, lower-cases tag
 * names, strips comments and processing instructions, drops disallowed `<head>` children
 * (`<script>`, `<link>`, ...), and normalises void elements so the downstream XHTML parser
 * never trips on `<br>` / `<img>` without a self-closing slash.
 */
class DocumentSkeletonPass : public HTMLTransformPass {
 public:
  const char* name() const override {
    return "DocumentSkeleton";
  }
  void apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) override;
};

/**
 * Pass 2 — StyleSheetCollector.
 *
 * Scans every `<style>` element inside `<head>`, tokenises the CSS, and populates the context's
 * class-rule and element-rule tables. Unsupported selectors (`*`, descendant combinators,
 * pseudo-classes, attribute selectors, `@media`, `@font-face`, `@keyframes`, ...) are dropped
 * with `subset:unsupported-selector` diagnostics.
 *
 * The originating `<style>` element is removed unless `Options::preserveStyleBlock` is true.
 */
class StyleSheetCollectorPass : public HTMLTransformPass {
 public:
  const char* name() const override {
    return "StyleSheetCollector";
  }
  void apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) override;
};

/**
 * Pass 3 — ComputedStyle.
 *
 * Walks the DOM and computes the resolved CSS property map for every element. The cascade is
 * inherited-from-parent → element defaults → element rules → class rules → inline `style="…"`.
 * Inheritance only applies to text-related properties (the same set used by the importer's
 * `HTMLInheritedStyle`).
 */
class ComputedStylePass : public HTMLTransformPass {
 public:
  const char* name() const override {
    return "ComputedStyle";
  }
  void apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) override;
};

/**
 * Pass 4 — PropertyFilter.
 *
 * Filters every resolved property map through `SubsetPropertyTable()`. Disallowed properties
 * are removed with a `subset:unsupported-property` diagnostic. Values are normalised (unit
 * conversion, shorthand collapse, value sanitisation). The pass writes back into the cached
 * resolved map so the emitter can serialise the result without re-resolving.
 */
class PropertyFilterPass : public HTMLTransformPass {
 public:
  const char* name() const override {
    return "PropertyFilter";
  }
  void apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) override;
};

/**
 * Optional Pass — AbsoluteToFlexInference.
 *
 * Recovers flexbox semantics from a tree where every visual element is `position: absolute`
 * with explicit `left/top/width/height` (the canonical output of `tools/html-snapshot`). For
 * each container whose children form a single 1D row or column with consistent gaps,
 * symmetric padding, and uniform cross-axis alignment, the pass rewrites the parent into a
 * `display: flex` container and strips the children's `position`/`left`/`right`/`top`/
 * `bottom` declarations.
 *
 * Off by default (`Options::inferFlexFromAbsolute`). Enable via `--html-infer-flex` in the
 * CLI when the upstream HTML is known to be flat absolute output and you want the resulting
 * PAGX to be edit-friendly.
 *
 * Containers that don't admit a clean 1D inference (overlapping siblings, mixed cross-axis
 * alignment, inconsistent spacing, mixed `position` values) are left untouched and an
 * informational `subset:flex-inference-skipped` diagnostic records the reason. Successful
 * conversions emit `subset:flex-inferred`.
 */
class AbsoluteToFlexInferencePass : public HTMLTransformPass {
 public:
  const char* name() const override {
    return "AbsoluteToFlexInference";
  }
  void apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) override;
};

/**
 * Pass 5 — StructureNormalization.
 *
 * Removes tags outside the subset (`<table>`, `<form>`, `<input>`, `<script>`, ...), wraps
 * stray text children inside containers, collapses redundant single-child inline wrappers,
 * and ensures self-closing void elements.
 */
class StructureNormalizationPass : public HTMLTransformPass {
 public:
  const char* name() const override {
    return "StructureNormalization";
  }
  void apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) override;
};

/**
 * Pass 6 — InlineStyleEmitter.
 *
 * Re-serialises each element's cached resolved property map back into a deterministic
 * `style="…"` attribute (alphabetical ordering for stable golden tests), then drops the
 * element's `class` attribute (unless `Options::preserveClassAttribute` is true) since the
 * cascade has already been baked into the inline style.
 */
class InlineStyleEmitterPass : public HTMLTransformPass {
 public:
  const char* name() const override {
    return "InlineStyleEmitter";
  }
  void apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) override;
};

}  // namespace pagx::html
