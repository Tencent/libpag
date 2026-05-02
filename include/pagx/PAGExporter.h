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

// IMPORTANT: This header is intentionally tgfx-free. It transitively depends
// on PAGXDocument.h which references tgfx::Data/Color/Matrix (pure data types
// with no rendering cost), but it does NOT pull in the tgfx rendering
// subsystem (no tgfx/layers/*, tgfx/gpu/*, tgfx/core/Surface.h). Export-only
// SDK users can include this header without dragging in the renderer.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "pagx/Diagnostic.h"
#include "pagx/PAGXDocument.h"

namespace pagx {

/**
 * PAGExporter — PAGX → PAG v2 byte stream serialiser.
 *
 * Internal pipeline: Bake (PAGX → PAGDocument) → Codec::Encode (PAGDocument →
 * bytes). Both stages contribute warnings to the returned Result; fatal
 * errors from Baker are always surfaced, and — when Options::strict is true
 * — all Warning-severity diagnostics from Baker (200-299) and Codec
 * (400-499) are promoted to errors so callers can gate on Result::ok.
 *
 * Design doc: §15.2 (interface contract) + §14.2 (CLI integration notes).
 */
class PAGExporter {
 public:
  /**
   * Controls how text is emitted.
   *   Render:      Emit text as reference to system or embedded fonts.
   *                Renderer performs glyph rasterisation at playback time.
   *                Default.
   *   OutlineAll:  Bake every glyph to vector paths at conversion time,
   *                producing a self-contained file at the cost of size.
   *
   * Phase 10 honours only Render; OutlineAll plumbing lands in Phase 11
   * together with the CLI wiring. Selecting OutlineAll today silently
   * behaves like Render.
   */
  enum class FontMode { Render, OutlineAll };

  struct Options {
    FontMode fontMode = FontMode::Render;

    /**
     * If true, all Diagnostic whose SeverityOf(code) == Warning are promoted
     * to errors (which flips ok=false per the Result invariant). Inflater
     * warnings (600-699) are unreachable from this export path; only
     * Baker (200-299) + Codec (400-499) warnings are affected in practice.
     * Default false.
     */
    bool strict = false;

    // Explicit constructor needed so `ToBytes/ToFile(..., options =
    // Options())` default argument can be resolved while PAGExporter is
    // still being defined (clang emits a hard error for nested-class
    // default-member-initialiser uses during the enclosing class's
    // definition without this).
    Options() {
    }
  };

  /**
   * Export result. `ok == true ⟺ errors.empty()` is the single truth-source
   * invariant maintained by the implementation — callers MUST NOT write to
   * `ok` directly.
   *
   * On ok:
   *   - ToBytes populates `bytes`; ToFile writes the file and leaves
   *     `bytes` empty.
   *   - `warnings` may be non-empty (degradations such as
   *     ImageSourceMissing).
   * On !ok:
   *   - `errors` is non-empty; `bytes` is empty (ToBytes) or the target
   *     file was never touched (ToFile writes to a temporary path first
   *     and atomically renames on success, so partial writes do not
   *     surface to callers — see ToFile docstring).
   *
   * Design doc §15.2 requires the Exporter/Loader Result contracts stay
   * symmetric. Adding fields here implies updating PAGLoader::Result too.
   */
  struct Result {
    bool ok = false;
    std::vector<uint8_t> bytes;
    std::vector<Diagnostic> errors;
    std::vector<Diagnostic> warnings;
  };

  /**
   * Export to a file, atomic-write style.
   *
   * Writes to `<filePath>.tmp-<pid>-<tid>` first, then renames to
   * `filePath` on success. If Bake / Encode / strict-promotion fail, the
   * temporary is unlinked and `filePath` stays untouched (no partial-write
   * visible to downstream consumers). The parent directory of `filePath`
   * must exist and be writable.
   *
   * @param document  Source document. Must have applyLayout() called.
   * @param filePath  Output path.
   * @param options   Export options; defaults suit most cases.
   *
   * Thread-safety: Reentrant. Same PAGXDocument may be exported
   * concurrently from multiple threads so long as no other thread mutates
   * it in parallel.
   */
  static Result ToFile(const PAGXDocument& document, const std::string& filePath,
                       const Options& options = Options());

  /**
   * Export to an in-memory byte buffer.
   * @see ToFile for semantics.
   */
  static Result ToBytes(const PAGXDocument& document, const Options& options = Options());
};

}  // namespace pagx
