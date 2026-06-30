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

#include "pagx/html/importer/HTMLSvgFilterDecoder.h"
#include <cmath>
#include "pagx/PAGXDocument.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLInlineSvgEmitter.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/svg/SVGBlendMode.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

using namespace pagx::html;

namespace {

// The alpha-extract feColorMatrix the exporter inserts via EnsurePipelineAlpha to feed a
// Drop / Inner / Blend filter from the running pipeline's alpha channel.
const char* kAlphaExtractValues = "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0";

bool NameIs(const std::shared_ptr<DOMNode>& node, const char* name) {
  return node && node->type == DOMNodeType::Element && node->name == name;
}

std::string Attr(const std::shared_ptr<DOMNode>& node, const char* name) {
  if (!node) return {};
  const std::string* v = node->findAttribute(name);
  return v ? *v : std::string();
}

float AttrFloat(const std::shared_ptr<DOMNode>& node, const char* name, float fallback) {
  std::string raw = Trim(Attr(node, name));
  if (raw.empty()) return fallback;
  char* end = nullptr;
  float v = std::strtof(raw.c_str(), &end);
  return end == raw.c_str() ? fallback : v;
}

// Collects an element's direct element children (filter primitives / feFunc nodes).
std::vector<std::shared_ptr<DOMNode>> ElementChildren(const std::shared_ptr<DOMNode>& node) {
  std::vector<std::shared_ptr<DOMNode>> out;
  if (!node) return out;
  for (auto child = node->getFirstChild(); child; child = child->getNextSibling()) {
    if (child->type == DOMNodeType::Element) out.push_back(child);
  }
  return out;
}

// True when the feColorMatrix carries the alpha-extract values the exporter uses before a
// Drop / Inner / Blend filter (as opposed to an author-meaningful colour transform).
bool IsAlphaExtractMatrix(const std::shared_ptr<DOMNode>& node) {
  if (!NameIs(node, "feColorMatrix")) return false;
  auto values = ParseFloatList(Attr(node, "values"));
  auto expected = ParseFloatList(kAlphaExtractValues);
  if (values.size() != expected.size()) return false;
  for (size_t i = 0; i < values.size(); i++) {
    if (values[i] != expected[i]) return false;
  }
  return true;
}

// True when the feColorMatrix has the DropShadow colour-fill shape:
// "0 0 0 0 R  0 0 0 0 G  0 0 0 0 B  0 0 0 A 0" — only the colour columns (idx 4/9/14) and the
// alpha multiplier (idx 18) are free; every other entry is zero.
bool IsDropColorMatrix(const std::shared_ptr<DOMNode>& node) {
  if (!NameIs(node, "feColorMatrix")) return false;
  auto v = ParseFloatList(Attr(node, "values"));
  if (v.size() != 20) return false;
  const int zeros[] = {0, 1, 2, 3, 5, 6, 7, 8, 10, 11, 12, 13, 15, 16, 17, 19};
  for (int idx : zeros) {
    if (v[static_cast<size_t>(idx)] != 0.0f) return false;
  }
  return true;
}

void ReadBlurStdDeviation(const std::shared_ptr<DOMNode>& node, float& blurX, float& blurY) {
  auto comps = ParseFloatList(Attr(node, "stdDeviation"));
  if (comps.empty()) {
    blurX = 0;
    blurY = 0;
  } else if (comps.size() == 1) {
    blurX = comps[0];
    blurY = comps[0];
  } else {
    blurX = comps[0];
    blurY = comps[1];
  }
}

}  // namespace

HTMLSvgFilterDecoder::HTMLSvgFilterDecoder(HTMLDiagnosticSink& sink,
                                           HTMLInlineSvgEmitter& svgEmitter,
                                           HTMLValueParser& valueParser)
    : _diagnostics(sink), _svgEmitter(svgEmitter), _valueParser(valueParser) {
}

void HTMLSvgFilterDecoder::bindDocument(PAGXDocument* document) {
  _document = document;
}

std::vector<LayerFilter*> HTMLSvgFilterDecoder::decode(const std::string& filterId) {
  std::vector<LayerFilter*> result;
  auto filterNode = _svgEmitter.lookupSharedDef(filterId);
  if (filterNode == nullptr || !NameIs(filterNode, "filter")) {
    _diagnostics.warn("html: filter references an unknown <filter> id '" + filterId + "'; ignored");
    return result;
  }
  auto prims = ElementChildren(filterNode);
  size_t index = 0;
  while (index < prims.size()) {
    size_t before = index;
    LayerFilter* filter = decodeStep(prims, index);
    if (filter == nullptr || index == before) {
      // Unrecognised primitive run: drop the whole filter so we never emit a partial chain.
      _diagnostics.warn("html: <filter> '" + filterId +
                        "' uses primitives outside the supported set; ignored");
      return {};
    }
    result.push_back(filter);
  }
  return result;
}

LayerFilter* HTMLSvgFilterDecoder::decodeStep(const std::vector<std::shared_ptr<DOMNode>>& prims,
                                              size_t& index) {
  size_t i = index;

  // An alpha-extract feColorMatrix only ever precedes a Drop / Inner / Blend filter that
  // consumes the pipeline alpha; fold it into that following filter when present.
  if (i < prims.size() && IsAlphaExtractMatrix(prims[i]) && i + 1 < prims.size()) {
    const auto& next = prims[i + 1];
    if (NameIs(next, "feGaussianBlur") || NameIs(next, "feComponentTransfer") ||
        NameIs(next, "feFlood")) {
      i++;
    }
  }
  if (i >= prims.size()) return nullptr;

  const auto& head = prims[i];

  // ---- InnerShadowFilter: feComponentTransfer(feFuncA table "1 0") -> feGaussianBlur ->
  //      [feOffset] -> feFlood -> feComposite(in) -> feComposite(in) -> [feMerge] ----
  if (NameIs(head, "feComponentTransfer")) {
    auto funcs = ElementChildren(head);
    bool invert = !funcs.empty() && NameIs(funcs[0], "feFuncA") &&
                  Attr(funcs[0], "type") == "table" && Trim(Attr(funcs[0], "tableValues")) == "1 0";
    if (!invert) return nullptr;
    size_t j = i + 1;
    if (j >= prims.size() || !NameIs(prims[j], "feGaussianBlur")) return nullptr;
    auto* inner = _document->makeNode<InnerShadowFilter>();
    ReadBlurStdDeviation(prims[j], inner->blurX, inner->blurY);
    j++;
    if (j < prims.size() && NameIs(prims[j], "feOffset")) {
      inner->offsetX = AttrFloat(prims[j], "dx", 0);
      inner->offsetY = AttrFloat(prims[j], "dy", 0);
      j++;
    }
    if (j >= prims.size() || !NameIs(prims[j], "feFlood")) return nullptr;
    Color color = _valueParser.parseColor(Attr(prims[j], "flood-color"));
    std::string opacity = Trim(Attr(prims[j], "flood-opacity"));
    if (!opacity.empty()) {
      color.alpha *= std::strtof(opacity.c_str(), nullptr);
    }
    inner->color = color;
    j++;
    if (j >= prims.size() || !NameIs(prims[j], "feComposite")) return nullptr;
    j++;
    if (j >= prims.size() || !NameIs(prims[j], "feComposite")) return nullptr;
    j++;
    if (j < prims.size() && NameIs(prims[j], "feMerge")) {
      inner->shadowOnly = false;
      j++;
    } else {
      inner->shadowOnly = true;
    }
    index = j;
    return inner;
  }

  // ---- BlendFilter: feFlood -> feBlend(mode) -> feComposite(in) ----
  if (NameIs(head, "feFlood") && i + 1 < prims.size() && NameIs(prims[i + 1], "feBlend")) {
    auto* blend = _document->makeNode<BlendFilter>();
    Color color = _valueParser.parseColor(Attr(head, "flood-color"));
    std::string opacity = Trim(Attr(head, "flood-opacity"));
    if (!opacity.empty()) {
      color.alpha *= std::strtof(opacity.c_str(), nullptr);
    }
    blend->color = color;
    std::string mode = Trim(Attr(prims[i + 1], "mode"));
    blend->blendMode = mode.empty() ? BlendMode::Normal : SVGBlendModeFromString(mode);
    size_t j = i + 2;
    if (j >= prims.size() || !NameIs(prims[j], "feComposite")) return nullptr;
    j++;
    index = j;
    return blend;
  }

  // ---- BlurFilter vs DropShadowFilter, both starting with feGaussianBlur ----
  if (NameIs(head, "feGaussianBlur")) {
    // DropShadow look-ahead: feGaussianBlur -> [feOffset] -> feColorMatrix(drop colour) [-> feMerge]
    size_t k = i + 1;
    if (k < prims.size() && NameIs(prims[k], "feOffset")) k++;
    if (k < prims.size() && IsDropColorMatrix(prims[k])) {
      auto* drop = _document->makeNode<DropShadowFilter>();
      ReadBlurStdDeviation(head, drop->blurX, drop->blurY);
      if (NameIs(prims[i + 1], "feOffset")) {
        drop->offsetX = AttrFloat(prims[i + 1], "dx", 0);
        drop->offsetY = AttrFloat(prims[i + 1], "dy", 0);
      }
      auto v = ParseFloatList(Attr(prims[k], "values"));
      Color color = {};
      color.red = v[4];
      color.green = v[9];
      color.blue = v[14];
      color.alpha = v[18];
      drop->color = color;
      size_t j = k + 1;
      if (j < prims.size() && NameIs(prims[j], "feMerge")) {
        drop->shadowOnly = false;
        j++;
      } else {
        drop->shadowOnly = true;
      }
      index = j;
      return drop;
    }
    // Plain Gaussian blur.
    auto* blur = _document->makeNode<BlurFilter>();
    ReadBlurStdDeviation(head, blur->blurX, blur->blurY);
    index = i + 1;
    return blur;
  }

  // ---- ColorMatrixFilter: a standalone feColorMatrix colour transform ----
  if (NameIs(head, "feColorMatrix")) {
    auto v = ParseFloatList(Attr(head, "values"));
    if (v.size() != 20) return nullptr;
    auto* cm = _document->makeNode<ColorMatrixFilter>();
    for (size_t n = 0; n < 20; n++) {
      cm->matrix[n] = v[n];
    }
    index = i + 1;
    return cm;
  }

  return nullptr;
}

}  // namespace pagx
