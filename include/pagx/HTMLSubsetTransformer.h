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

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace pagx {

struct DOMNode;
class HTMLTransformPass;

/**
 * Rewrites an HTML DOM so that the produced tree strictly satisfies the subset documented in
 * `spec/html_subset.md`.
 *
 * Inputs that already follow the subset pass through unchanged. Inputs that use disallowed
 * elements, selectors, properties, or units are either rewritten (when there is a deterministic
 * equivalent within the subset) or pruned with a structured diagnostic.
 *
 * The transformer is designed to run as an automatic pre-pass inside `HTMLImporter::Parse` so
 * downstream code only ever sees subset-compliant HTML. It can also be invoked standalone (for
 * example by tooling that wants to inspect the normalised intermediate form).
 *
 * CSS resolution scope: inline styles, `<style>` blocks (class + element selectors only),
 * element defaults, inheritable cascade. Tailwind utility classes are NOT compiled; if such
 * classes are encountered, their declarations are only honoured when the same `<style>` block
 * defines them via a normal class rule.
 *
 * The transformer is structured as a configurable pipeline of independent passes (see
 * `Builder`) so new behaviour can be added without modifying the default flow.
 */
class HTMLSubsetTransformer {
 public:
  enum class Severity {
    Warning,
    Error,
  };

  /**
   * Machine-readable diagnostic emitted by a pass. `code` follows the format `subset:<category>`
   * (for example `subset:unsupported-property`, `subset:unsupported-tag`, `subset:unit-coerced`).
   */
  struct Diagnostic {
    Severity severity = Severity::Warning;
    std::string code = {};
    std::string message = {};
    std::string nodeName = {};
    int line = 0;
  };

  struct Options {
    /**
     * Treat every warning as a hard error. When true, the transformer stops on the first
     * unsupported construct and `Result::ok` is false.
     */
    bool strict = false;

    /**
     * Keep unknown HTML tags in place as `<div data-html-unknown="<tag>">` wrappers instead of
     * dropping the subtree. Matches the existing `--html-preserve-unknown` CLI flag.
     */
    bool preserveUnknownElements = false;

    /**
     * When true, the original `<style>` block is preserved (for forensic debugging). The cascade
     * is still inlined onto each element, so the duplicate rules are inert. Default false.
     */
    bool preserveStyleBlock = false;

    /**
     * When true, the original `class` attribute is preserved on each element. Default false, so
     * `class` is dropped once the cascade has been resolved into inline styles. The importer
     * does not need it after that point.
     */
    bool preserveClassAttribute = false;

    /**
     * Canvas width / height used to resolve `vw` / `vh` units. NaN means unknown — `vw`/`vh`
     * values are then dropped with a diagnostic.
     */
    float canvasWidth = NAN;
    float canvasHeight = NAN;

    Options() {
    }
  };

  struct Result {
    bool ok = true;
    std::vector<Diagnostic> diagnostics = {};
  };

  /**
   * Runs the default pipeline on `root`. The root pointer is expected to be the document root
   * produced by `XMLDOM` (typically the `<html>` element, possibly preceded by a wrapper). The
   * tree is mutated in place.
   */
  static Result Transform(const std::shared_ptr<DOMNode>& root, const Options& options = Options());

  /**
   * Pipeline builder for tests and tooling that need a non-default pass list. The default
   * pipeline (built by `Transform`) is:
   *   1. DocumentSkeletonPass
   *   2. StyleSheetCollectorPass
   *   3. ComputedStylePass
   *   4. PropertyFilterPass
   *   5. StructureNormalizationPass
   *   6. InlineStyleEmitterPass
   *
   * Custom passes can be inserted via `addPass`. The pipeline is owned by the resulting
   * transformer instance.
   */
  class Builder {
   public:
    Builder();
    ~Builder();
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
    Builder(Builder&&) noexcept;
    Builder& operator=(Builder&&) noexcept;

    Builder& withOptions(const Options& options);
    Builder& addDefaultPasses();
    Builder& addPass(std::unique_ptr<HTMLTransformPass> pass);

    Result run(const std::shared_ptr<DOMNode>& root);

   private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
  };
};

}  // namespace pagx
