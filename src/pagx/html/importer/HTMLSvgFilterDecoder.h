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
 * The decoder walks the `<filter>`'s primitive children in document order and greedily matches
 * each filter's signature primitive run, allocating the corresponding node through the bound
 * document. It deliberately recognises only the exporter's templates: an arbitrary hand-authored
 * filter whose primitives do not line up with one of those templates is reported via the
 * diagnostic sink and decoding stops, so the caller drops the filter rather than emitting a
 * partial chain that would not match the author's intent.
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
   * when the id is unknown or the primitive sequence does not match an exporter template.
   */
  std::vector<LayerFilter*> decode(const std::string& filterId);

 private:
  // Decodes the primitive run starting at `prims[index]` into a single filter node, advancing
  // `index` past every primitive it consumed. Returns nullptr (and warns) when no exporter
  // template matches at that position.
  LayerFilter* decodeStep(const std::vector<std::shared_ptr<DOMNode>>& prims, size_t& index);

  HTMLDiagnosticSink& _diagnostics;
  HTMLInlineSvgEmitter& _svgEmitter;
  HTMLValueParser& _valueParser;
  PAGXDocument* _document = nullptr;
};

}  // namespace pagx
