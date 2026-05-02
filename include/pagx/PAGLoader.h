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

// IMPORTANT: This header depends on tgfx — it returns tgfx::Layer trees.
// Include it only when your module consumes rendered layer graphs. For
// export-only use cases that don't need the renderer, include
// <pagx/PAGExporter.h> instead (that header is tgfx-render-free).
//
// ABI model: SAME-BUILD. libpag and tgfx MUST be built together as one
// compilation unit chain. Do NOT mix a prebuilt libpag with a different
// tgfx build — tgfx::Layer's vtable/layout is not version-stable. Rebuild
// libpag whenever tgfx is upgraded. See §I.4 of the design doc.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "pagx/Diagnostic.h"
#include "tgfx/core/Color.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

/**
 * PAGLoader — PAG v2 byte stream → tgfx::Layer tree loader.
 *
 * Internal pipeline: Codec::Decode (bytes → PAGDocument) → LayerInflater
 * (PAGDocument → tgfx::Layer). Codec fatals (300-399) always propagate to
 * errors; Codec warnings (400-499) are promoted to errors only when
 * Options::strict=true. Inflater warnings (600-699) are NEVER promoted —
 * they describe per-layer degradations that callers usually want visible
 * but non-fatal (e.g. a missing image shouldn't reject the whole file).
 *
 * Design doc: §15.3 (interface contract) + §9 (LayerInflater semantics).
 */
class PAGLoader {
 public:
  struct Options {
    /**
     * If true, Codec warnings (400-499) produced during Decode are promoted
     * to errors (which flips ok=false per the Result invariant). Inflater
     * warnings (600-699) are NOT promoted. Default false.
     */
    bool strict = false;

    // Explicit default constructor — needed so default-arg
    // `LoadFromBytes(..., options = Options())` resolves during the class
    // definition (same clang quirk PAGExporter::Options hit).
    Options() {
    }
  };

  /**
   * Load result. `ok ⟺ errors.empty()` invariant maintained by the
   * implementation — callers MUST NOT write to `ok` directly. Mirrors
   * PAGExporter::Result shape so upstream tooling can treat both
   * symmetrically (see §15.2/§15.3 cross-reference).
   */
  struct Result {
    bool ok = false;
    std::shared_ptr<tgfx::Layer> layer = nullptr;
    std::vector<Diagnostic> errors = {};
    std::vector<Diagnostic> warnings = {};
  };

  /**
   * Lightweight header preview for a .pag file. Peeks only the FileHeader
   * Tag inside the file container; does NOT Decode the full document or
   * inflate any layers. Useful for fast metadata probes (dimensions,
   * background colour, frame rate, duration) before deciding whether to
   * load the full file.
   */
  struct PeekResult {
    bool ok = false;
    float width = 0.0f;
    float height = 0.0f;
    tgfx::Color backgroundColor = {};
    // Frame rate stored as a rational (numerator/denominator) to preserve
    // exact values like 23.976 (24000/1001). Callers can divide lazily.
    uint32_t frameRateNumerator = 24;
    uint32_t frameRateDenominator = 1;
    // Total duration in frames.
    uint32_t duration = 1;
    std::vector<Diagnostic> errors = {};
    std::vector<Diagnostic> warnings = {};
  };

  /**
   * Load a .pag byte stream from memory.
   * @param data    Pointer to the bytes; may be nullptr iff `length == 0`.
   * @param length  Number of bytes in the buffer.
   * @param options Loader options; defaults suit most cases.
   */
  static Result LoadFromBytes(const uint8_t* data, size_t length,
                              const Options& options = Options());

  /**
   * Load a .pag file from disk. Emits FileReadFailed=307 when the file
   * can't be opened / read; otherwise delegates to LoadFromBytes.
   */
  static Result LoadFromFile(const std::string& filePath, const Options& options = Options());

  /**
   * Shallow peek at a .pag file's header. Parses only the file container
   * framing + the first FileHeader Tag; stops before touching any images,
   * fonts, or compositions. O(1) in the document's size beyond the first
   * few bytes.
   */
  static PeekResult Peek(const std::string& filePath);

  /**
   * Shallow peek at an in-memory byte stream. Same contract as the file
   * overload.
   */
  static PeekResult Peek(const uint8_t* data, size_t length);
};

}  // namespace pagx
