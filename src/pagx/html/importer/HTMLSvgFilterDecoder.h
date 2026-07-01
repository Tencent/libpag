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
#include <vector>

namespace pagx {

struct DOMNode;
class HTMLDiagnosticSink;
class HTMLInlineSvgEmitter;
class HTMLValueParser;
class LayerFilter;
class PAGXDocument;

/**
 * Decodes an SVG `<filter>` definition (referenced by `filter: url(#id)`) back into the PAGX
 * `LayerFilter` chain that produced it. This is the inverse of `HTMLWriter::writeFilterDefs`,
 * which serialises each of the five layer filters — Blur, DropShadow, InnerShadow, Blend,
 * ColorMatrix — into a fixed sequence of `fe*` primitives chained through `result` / `in`.
 *
 * Rather than assuming the primitives sit in a fixed physical order, the decoder models the
 * `<filter>` as the data-flow graph it actually is: every primitive names its output via
 * `result` and references upstream outputs (or the `SourceGraphic` / `SourceAlpha` built-ins) via
 * `in` / `in2`. Decoding starts from `SourceGraphic` and repeatedly matches one of the five filter
 * sub-graphs against the current pipeline input, consuming its primitives and advancing the input
 * to that sub-graph's output, until every primitive has been consumed. Connecting by name rather
 * than by adjacency tolerates reordered attributes, alternate `result` spellings, an omitted
 * `result` on the final primitive, and alpha pre-passes that differ only by a scalar multiplier.
 *
 * It deliberately recognises only the five exporter templates: a `<filter>` whose primitives do
 * not all fold into those sub-graphs (e.g. `feImage`, `feTurbulence`) is reported via the
 * diagnostic sink and dropped whole, so the caller never emits a partial chain that would not
 * match the author's intent.
 */
class HTMLSvgFilterDecoder {
 public:
  HTMLSvgFilterDecoder(HTMLDiagnosticSink& sink, HTMLInlineSvgEmitter& svgEmitter,
                       HTMLValueParser& valueParser);

  /** Wires the document handle so filter nodes can be allocated. Called once after the document
   *  has been constructed. */
  void bindDocument(PAGXDocument* document);

  /**
   * Resolves the `<filter>` def carrying `filterId` through the shared-defs table and decodes its
   * primitive children into a PAGX filter chain. Returns an empty vector (and emits a diagnostic)
   * when the id is unknown or the primitive graph does not fold entirely into exporter templates.
   */
  std::vector<LayerFilter*> decode(const std::string& filterId);

 private:
  HTMLDiagnosticSink& _diagnostics;
  HTMLInlineSvgEmitter& _svgEmitter;
  HTMLValueParser& _valueParser;
  PAGXDocument* _document = nullptr;
};

}  // namespace pagx
