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
#include <unordered_map>
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

// A 4x5 feColorMatrix has 20 coefficients laid out row-major (R, G, B, A rows). These name the
// columns the decoder reads so the matrix-shape checks read as intent, not magic indices.
constexpr size_t kMatrixSize = 20;
constexpr size_t kRedConstColumn = 4;       // constant term added to the red output
constexpr size_t kGreenConstColumn = 9;     // constant term added to the green output
constexpr size_t kBlueConstColumn = 14;     // constant term added to the blue output
constexpr size_t kAlphaOnAlphaColumn = 18;  // alpha-output coefficient on the alpha input

// The two built-in filter inputs the exporter feeds its sub-graphs from.
constexpr const char* kSourceGraphic = "SourceGraphic";
constexpr const char* kSourceAlpha = "SourceAlpha";

// Sentinel producer index meaning "a built-in source / unresolved reference", as opposed to a
// real primitive index (>= 0).
constexpr int kBuiltinSource = -1;

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

std::vector<std::shared_ptr<DOMNode>> ElementChildren(const std::shared_ptr<DOMNode>& node) {
  std::vector<std::shared_ptr<DOMNode>> out;
  if (!node) return out;
  for (auto child = node->getFirstChild(); child; child = child->getNextSibling()) {
    if (child->type == DOMNodeType::Element) out.push_back(child);
  }
  return out;
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

// Reads a flood paint (`flood-color` + optional `flood-opacity`) into a Color through the value
// parser, folding the opacity into the colour's alpha.
Color ReadFlood(const std::shared_ptr<DOMNode>& node, HTMLValueParser& parser) {
  Color color = parser.parseColor(Attr(node, "flood-color"));
  std::string opacity = Trim(Attr(node, "flood-opacity"));
  if (!opacity.empty()) {
    char* end = nullptr;
    float o = std::strtof(opacity.c_str(), &end);
    if (end != opacity.c_str()) color.alpha *= o;
  }
  return color;
}

bool IsBuiltinSourceName(const std::string& name) {
  return name == kSourceGraphic || name == kSourceAlpha;
}

// True when the feColorMatrix only re-derives the alpha channel as a scalar multiple of the input
// alpha (every coefficient zero except the alpha-on-alpha term, which is non-zero). The exporter
// emits this `1`-multiplier alpha-extract before a Drop / Inner / Blend filter; other tools emit
// an equivalent opaque-alpha form with a `255` multiplier. Both are alpha pre-passes rather than
// an author-meaningful colour transform.
bool IsAlphaExtractMatrix(const std::shared_ptr<DOMNode>& node) {
  if (!NameIs(node, "feColorMatrix")) return false;
  std::string type = Trim(Attr(node, "type"));
  if (!type.empty() && type != "matrix") return false;
  auto v = ParseFloatList(Attr(node, "values"));
  if (v.size() != kMatrixSize) return false;
  for (size_t i = 0; i < v.size(); i++) {
    if (i == kAlphaOnAlphaColumn) continue;
    if (v[i] != 0.0f) return false;
  }
  return v[kAlphaOnAlphaColumn] != 0.0f;
}

// True when the feColorMatrix has the DropShadow colour-fill shape: only the three colour constant
// columns and the alpha-on-alpha term are free, every other coefficient zero. Such a matrix tints
// its (alpha-only) input to a solid RGBA shadow colour. A pure-black shadow's matrix has all three
// colour columns zero, which is identical to an alpha-extract matrix; the two are told apart by
// data-flow position (an alpha pre-pass feeds a downstream stage, a colour fill terminates the
// shadow sub-graph), handled by the matchers, not by shape here.
bool IsDropColorMatrix(const std::shared_ptr<DOMNode>& node) {
  if (!NameIs(node, "feColorMatrix")) return false;
  std::string type = Trim(Attr(node, "type"));
  if (!type.empty() && type != "matrix") return false;
  auto v = ParseFloatList(Attr(node, "values"));
  if (v.size() != kMatrixSize) return false;
  for (size_t i = 0; i < v.size(); i++) {
    if (i == kRedConstColumn || i == kGreenConstColumn || i == kBlueConstColumn ||
        i == kAlphaOnAlphaColumn) {
      continue;
    }
    if (v[i] != 0.0f) return false;
  }
  return true;
}

Color ColorFromDropMatrix(const std::shared_ptr<DOMNode>& node) {
  auto v = ParseFloatList(Attr(node, "values"));
  Color color = {};
  if (v.size() != kMatrixSize) return color;
  color.red = v[kRedConstColumn];
  color.green = v[kGreenConstColumn];
  color.blue = v[kBlueConstColumn];
  color.alpha = v[kAlphaOnAlphaColumn];
  return color;
}

// True when the feComponentTransfer inverts the alpha channel via `feFuncA type="table"
// tableValues="1 0"` — the inner-shadow pre-pass that turns the shape's interior transparent.
bool IsAlphaInvertTransfer(const std::shared_ptr<DOMNode>& node) {
  if (!NameIs(node, "feComponentTransfer")) return false;
  auto funcs = ElementChildren(node);
  if (funcs.empty() || !NameIs(funcs[0], "feFuncA")) return false;
  return Attr(funcs[0], "type") == "table" && Trim(Attr(funcs[0], "tableValues")) == "1 0";
}

bool IsCompositeIn(const std::shared_ptr<DOMNode>& node) {
  return NameIs(node, "feComposite") && Trim(Attr(node, "operator")) == "in";
}

// One primitive node in the filter's data-flow graph. `in` is resolved at build time to a concrete
// upstream name (an omitted `in` becomes the previous primitive's output, per the SVG spec), so
// matching only ever connects primitives by name. `consumed` is set once a matched sub-graph has
// claimed this primitive.
struct Primitive {
  std::shared_ptr<DOMNode> node;
  std::string in;
  std::string in2;
  std::string result;
  bool consumed = false;
};

// The data-flow graph of a single <filter>: its primitives plus a result-name -> index lookup.
// A sub-graph is identified by how its primitives connect through in / result, never by their
// physical position, so reordered or sparsely-named primitives decode identically.
class FilterGraph {
 public:
  std::vector<Primitive> primitives;

  void build(const std::shared_ptr<DOMNode>& filterNode) {
    auto children = ElementChildren(filterNode);
    std::string previousOutput = kSourceGraphic;
    for (size_t i = 0; i < children.size(); i++) {
      Primitive p;
      p.node = children[i];
      p.in = Trim(Attr(children[i], "in"));
      p.in2 = Trim(Attr(children[i], "in2"));
      p.result = Trim(Attr(children[i], "result"));
      if (p.in.empty()) p.in = previousOutput;
      std::string output = p.result.empty() ? syntheticName(i) : p.result;
      _byResult[output] = i;
      previousOutput = output;
      primitives.push_back(std::move(p));
    }
  }

  // The producer index for an input name, or kBuiltinSource for a built-in / unknown reference.
  int producerOf(const std::string& inputName) const {
    auto it = _byResult.find(inputName);
    return it == _byResult.end() ? kBuiltinSource : static_cast<int>(it->second);
  }

  // The output name of a producer index (a real primitive's result, or SourceGraphic for the
  // initial pipeline input). Lets callers match a consumer either by producer index or by name.
  std::string outputNameOf(int producerIndex) const {
    if (producerIndex < 0) return kSourceGraphic;
    const auto& p = primitives[static_cast<size_t>(producerIndex)];
    return p.result.empty() ? syntheticName(static_cast<size_t>(producerIndex)) : p.result;
  }

 private:
  // A stable placeholder name for a primitive that omits `result`, so downstream primitives that
  // implicitly chain off it (via an omitted `in`) still resolve to a producer index. The leading
  // control character cannot collide with an author-chosen result name.
  static std::string syntheticName(size_t index) {
    return "\x01result" + std::to_string(index);
  }

  std::unordered_map<std::string, size_t> _byResult;
};

// Matches the five exporter filter sub-graphs against a FilterGraph, allocating PAGX filter nodes
// through the bound document. Holds all matching state for one decode() call so the matchers read
// as plain methods rather than threading the graph / document through every call.
class FilterMatcher {
 public:
  FilterMatcher(FilterGraph& graph, PAGXDocument* document, HTMLValueParser& valueParser)
      : _graph(graph), _document(document), _valueParser(valueParser) {
  }

  // Decodes the whole graph into a filter chain by matching forwards from SourceGraphic, each
  // match advancing the pipeline input to its sub-graph's output. Returns false when no further
  // sub-graph matches the current input (the caller then checks whether everything was consumed).
  std::vector<LayerFilter*> run() {
    std::vector<LayerFilter*> out;
    int currentInput = kBuiltinSource;  // SourceGraphic
    for (size_t guard = 0; guard <= _graph.primitives.size(); guard++) {
      LayerFilter* filter = nullptr;
      int output = matchAt(currentInput, filter);
      if (output == kNoMatch) break;
      out.push_back(filter);
      currentInput = output;
    }
    return out;
  }

  bool allConsumed() const {
    for (const auto& p : _graph.primitives) {
      if (!p.consumed) return false;
    }
    return true;
  }

 private:
  static constexpr int kNoMatch = -2;

  FilterGraph& _graph;
  PAGXDocument* _document;
  HTMLValueParser& _valueParser;

  // Tries each filter sub-graph anchored on the current pipeline input. On a match, writes the
  // PAGX filter to `filter`, marks the consumed primitives, and returns the matched sub-graph's
  // output primitive index. Returns kNoMatch when nothing matches.
  int matchAt(int inputIndex, LayerFilter*& filter) {
    int output = kNoMatch;
    if ((output = matchDropShadow(inputIndex, filter)) != kNoMatch) return output;
    if ((output = matchInnerShadow(inputIndex, filter)) != kNoMatch) return output;
    if ((output = matchBlend(inputIndex, filter)) != kNoMatch) return output;
    if ((output = matchBlur(inputIndex, filter)) != kNoMatch) return output;
    if ((output = matchColorMatrix(inputIndex, filter)) != kNoMatch) return output;
    return kNoMatch;
  }

  // True when `prim.in` reads the pipeline input `inputIndex` (or the SourceGraphic / SourceAlpha
  // built-ins when the pipeline is at its start). Drop / Inner / Blend consume alpha, so a built-in
  // SourceAlpha at the chain head also counts as reading the input.
  bool readsInput(const Primitive& prim, int inputIndex) const {
    if (_graph.producerOf(prim.in) == inputIndex && inputIndex != kBuiltinSource) return true;
    if (inputIndex == kBuiltinSource) {
      return prim.in == _graph.outputNameOf(kBuiltinSource) || IsBuiltinSourceName(prim.in);
    }
    return prim.in == _graph.outputNameOf(inputIndex);
  }

  // Returns the upstream producer of `inName`, transparently skipping an alpha pre-pass
  // (alpha-extract feColorMatrix). Writes the consumed pre-pass index (or kBuiltinSource) to
  // `prePass`. Drop / Inner / Blend filters consume source alpha; the exporter and other tools
  // insert that pre-pass between the pipeline input and the filter.
  int resolveThroughAlphaPrepass(const std::string& inName, int& prePass) const {
    prePass = kBuiltinSource;
    int producer = _graph.producerOf(inName);
    if (producer < 0) return producer;
    const auto& prim = _graph.primitives[static_cast<size_t>(producer)];
    if (IsAlphaExtractMatrix(prim.node)) {
      prePass = producer;
      return _graph.producerOf(prim.in);
    }
    return producer;
  }

  // Returns the index of an unconsumed primitive named `name` whose `in` is the output of
  // `producerIndex`, or -1. Connection is by name, so physical order is irrelevant.
  int consumerNamed(int producerIndex, const char* name) const {
    std::string producerOutput = _graph.outputNameOf(producerIndex);
    for (size_t k = 0; k < _graph.primitives.size(); k++) {
      const auto& p = _graph.primitives[k];
      if (p.consumed || !NameIs(p.node, name)) continue;
      if (_graph.producerOf(p.in) == producerIndex || p.in == producerOutput) {
        return static_cast<int>(k);
      }
    }
    return -1;
  }

  // Like consumerNamed, but selects by a predicate on the DOM node instead of a tag name.
  int consumerMatching(int producerIndex, bool (*pred)(const std::shared_ptr<DOMNode>&)) const {
    std::string producerOutput = _graph.outputNameOf(producerIndex);
    for (size_t k = 0; k < _graph.primitives.size(); k++) {
      const auto& p = _graph.primitives[k];
      if (p.consumed || !pred(p.node)) continue;
      if (_graph.producerOf(p.in) == producerIndex || p.in == producerOutput) {
        return static_cast<int>(k);
      }
    }
    return -1;
  }

  // Finds a feComposite(operator=in) masking a feFlood against the output of `maskProducer` (its
  // `in` is a flood, its `in2` is the masking shape). Writes the flood index to `floodIndex` and
  // returns the composite index, or -1.
  int floodMaskedBy(int maskProducer, int& floodIndex) const {
    floodIndex = -1;
    std::string maskOutput = _graph.outputNameOf(maskProducer);
    for (size_t k = 0; k < _graph.primitives.size(); k++) {
      const auto& comp = _graph.primitives[k];
      if (comp.consumed || !IsCompositeIn(comp.node)) continue;
      if (_graph.producerOf(comp.in2) != maskProducer && comp.in2 != maskOutput) continue;
      int fIndex = _graph.producerOf(comp.in);
      if (fIndex < 0 || !NameIs(_graph.primitives[static_cast<size_t>(fIndex)].node, "feFlood")) {
        continue;
      }
      floodIndex = fIndex;
      return static_cast<int>(k);
    }
    return -1;
  }

  // Returns the index of a feMerge stacking SourceGraphic (or the pipeline-input layer) together
  // with the output of `shadowProducer` — the non-shadow-only composition — or -1.
  int mergeOver(int shadowProducer) const {
    std::string shadowOutput = _graph.outputNameOf(shadowProducer);
    for (size_t k = 0; k < _graph.primitives.size(); k++) {
      const auto& merge = _graph.primitives[k];
      if (merge.consumed || !NameIs(merge.node, "feMerge")) continue;
      bool refsShadow = false;
      bool refsSource = false;
      for (auto child = merge.node->getFirstChild(); child; child = child->getNextSibling()) {
        if (child->type != DOMNodeType::Element || child->name != "feMergeNode") continue;
        std::string in = Trim(Attr(child, "in"));
        int producer = _graph.producerOf(in);
        if (producer == shadowProducer || in == shadowOutput) {
          refsShadow = true;
        } else if (IsBuiltinSourceName(in) || producer >= 0) {
          refsSource = true;
        }
      }
      if (refsShadow && refsSource) return static_cast<int>(k);
    }
    return -1;
  }

  // ---- BlurFilter: a lone feGaussianBlur reading the pipeline input. Deferred when its output
  //      feeds a shadow colour fill, so a shadow's internal blur is not mis-claimed. ----
  int matchBlur(int inputIndex, LayerFilter*& filter) {
    for (size_t i = 0; i < _graph.primitives.size(); i++) {
      auto& prim = _graph.primitives[i];
      if (prim.consumed || !NameIs(prim.node, "feGaussianBlur")) continue;
      if (!readsInput(prim, inputIndex)) continue;
      if (feedsShadowColouring(static_cast<int>(i))) continue;
      auto* blur = _document->makeNode<BlurFilter>();
      ReadBlurStdDeviation(prim.node, blur->blurX, blur->blurY);
      prim.consumed = true;
      filter = blur;
      return static_cast<int>(i);
    }
    return kNoMatch;
  }

  // ---- DropShadowFilter: feGaussianBlur(from alpha) -> [feOffset] -> drop colour fill
  //      [-> feMerge with source]. The colour fill is a tint feColorMatrix or feFlood+composite. --
  int matchDropShadow(int inputIndex, LayerFilter*& filter) {
    for (size_t i = 0; i < _graph.primitives.size(); i++) {
      auto& blurPrim = _graph.primitives[i];
      if (blurPrim.consumed || !NameIs(blurPrim.node, "feGaussianBlur")) continue;
      int prePass = kBuiltinSource;
      int resolved = resolveThroughAlphaPrepass(blurPrim.in, prePass);
      bool fromPipeline = (resolved == inputIndex) ||
                          (inputIndex == kBuiltinSource && IsBuiltinSourceName(blurPrim.in));
      if (!fromPipeline) continue;

      int offsetIndex = consumerNamed(static_cast<int>(i), "feOffset");
      int afterOffset = (offsetIndex >= 0) ? offsetIndex : static_cast<int>(i);

      Color color;
      int colourOutput = -1;
      int tintIndex = consumerMatching(afterOffset, &IsDropColorMatrix);
      // A pure-black tint matrix is byte-identical to an alpha-extract pre-pass; only the terminal
      // colour fill (no further consumer) is this shadow's tint — a matrix that feeds another stage
      // belongs to a downstream filter, so reject it here and let the blur fall through to a plain
      // BlurFilter.
      if (tintIndex >= 0 && hasDownstreamConsumer(tintIndex)) tintIndex = -1;
      int floodIndex = -1;
      int compositeIndex = -1;
      if (tintIndex >= 0) {
        color = ColorFromDropMatrix(_graph.primitives[static_cast<size_t>(tintIndex)].node);
        colourOutput = tintIndex;
      } else {
        compositeIndex = floodMaskedBy(afterOffset, floodIndex);
        if (compositeIndex < 0) continue;
        color = ReadFlood(_graph.primitives[static_cast<size_t>(floodIndex)].node, _valueParser);
        colourOutput = compositeIndex;
      }

      int mergeIndex = mergeOver(colourOutput);
      auto* drop = _document->makeNode<DropShadowFilter>();
      ReadBlurStdDeviation(blurPrim.node, drop->blurX, drop->blurY);
      if (offsetIndex >= 0) {
        auto& off = _graph.primitives[static_cast<size_t>(offsetIndex)].node;
        drop->offsetX = AttrFloat(off, "dx", 0);
        drop->offsetY = AttrFloat(off, "dy", 0);
      }
      drop->color = color;
      drop->shadowOnly = (mergeIndex < 0);

      markConsumed(prePass);
      blurPrim.consumed = true;
      markConsumed(offsetIndex);
      markConsumed(tintIndex);
      markConsumed(floodIndex);
      markConsumed(compositeIndex);
      int output = colourOutput;
      if (mergeIndex >= 0) {
        _graph.primitives[static_cast<size_t>(mergeIndex)].consumed = true;
        output = mergeIndex;
      }
      filter = drop;
      return output;
    }
    return kNoMatch;
  }

  // ---- InnerShadowFilter: feComponentTransfer(invert alpha) -> feGaussianBlur -> [feOffset] ->
  //      feFlood -> feComposite(in, blurred) -> feComposite(in, source alpha) [-> feMerge]. ----
  int matchInnerShadow(int inputIndex, LayerFilter*& filter) {
    for (size_t i = 0; i < _graph.primitives.size(); i++) {
      auto& invertPrim = _graph.primitives[i];
      if (invertPrim.consumed || !IsAlphaInvertTransfer(invertPrim.node)) continue;
      int prePass = kBuiltinSource;
      int resolved = resolveThroughAlphaPrepass(invertPrim.in, prePass);
      bool fromPipeline = (resolved == inputIndex) ||
                          (inputIndex == kBuiltinSource && IsBuiltinSourceName(invertPrim.in));
      if (!fromPipeline) continue;

      int blurIndex = consumerNamed(static_cast<int>(i), "feGaussianBlur");
      if (blurIndex < 0) continue;
      int offsetIndex = consumerNamed(blurIndex, "feOffset");
      int afterOffset = (offsetIndex >= 0) ? offsetIndex : blurIndex;

      int floodIndex = -1;
      int shadowComposite = floodMaskedBy(afterOffset, floodIndex);
      if (shadowComposite < 0) continue;
      int clipComposite = consumerMatching(shadowComposite, &IsCompositeIn);
      if (clipComposite < 0) continue;

      int mergeIndex = mergeOver(clipComposite);
      auto* inner = _document->makeNode<InnerShadowFilter>();
      ReadBlurStdDeviation(_graph.primitives[static_cast<size_t>(blurIndex)].node, inner->blurX,
                           inner->blurY);
      if (offsetIndex >= 0) {
        auto& off = _graph.primitives[static_cast<size_t>(offsetIndex)].node;
        inner->offsetX = AttrFloat(off, "dx", 0);
        inner->offsetY = AttrFloat(off, "dy", 0);
      }
      inner->color =
          ReadFlood(_graph.primitives[static_cast<size_t>(floodIndex)].node, _valueParser);
      inner->shadowOnly = (mergeIndex < 0);

      markConsumed(prePass);
      invertPrim.consumed = true;
      _graph.primitives[static_cast<size_t>(blurIndex)].consumed = true;
      markConsumed(offsetIndex);
      _graph.primitives[static_cast<size_t>(floodIndex)].consumed = true;
      _graph.primitives[static_cast<size_t>(shadowComposite)].consumed = true;
      _graph.primitives[static_cast<size_t>(clipComposite)].consumed = true;
      int output = clipComposite;
      if (mergeIndex >= 0) {
        _graph.primitives[static_cast<size_t>(mergeIndex)].consumed = true;
        output = mergeIndex;
      }
      filter = inner;
      return output;
    }
    return kNoMatch;
  }

  // ---- BlendFilter: feFlood -> feBlend(mode, with pipeline input) -> feComposite(in, source). --
  int matchBlend(int inputIndex, LayerFilter*& filter) {
    for (size_t i = 0; i < _graph.primitives.size(); i++) {
      auto& floodPrim = _graph.primitives[i];
      if (floodPrim.consumed || !NameIs(floodPrim.node, "feFlood")) continue;
      int blendIndex = consumerNamed(static_cast<int>(i), "feBlend");
      if (blendIndex < 0) continue;
      auto& blend = _graph.primitives[static_cast<size_t>(blendIndex)];
      bool blendsInput =
          (_graph.producerOf(blend.in2) == inputIndex && inputIndex != kBuiltinSource) ||
          (inputIndex == kBuiltinSource && IsBuiltinSourceName(blend.in2)) ||
          blend.in2 == _graph.outputNameOf(inputIndex);
      if (!blendsInput) continue;
      int compositeIndex = consumerMatching(blendIndex, &IsCompositeIn);
      if (compositeIndex < 0) continue;

      auto* blendFilter = _document->makeNode<BlendFilter>();
      blendFilter->color = ReadFlood(floodPrim.node, _valueParser);
      std::string mode = Trim(Attr(blend.node, "mode"));
      blendFilter->blendMode = mode.empty() ? BlendMode::Normal : SVGBlendModeFromString(mode);

      floodPrim.consumed = true;
      blend.consumed = true;
      _graph.primitives[static_cast<size_t>(compositeIndex)].consumed = true;
      filter = blendFilter;
      return compositeIndex;
    }
    return kNoMatch;
  }

  // ---- ColorMatrixFilter: a meaningful feColorMatrix colour transform reading the input. ----
  int matchColorMatrix(int inputIndex, LayerFilter*& filter) {
    for (size_t i = 0; i < _graph.primitives.size(); i++) {
      auto& prim = _graph.primitives[i];
      if (prim.consumed || !NameIs(prim.node, "feColorMatrix")) continue;
      if (!readsInput(prim, inputIndex)) continue;
      if (IsAlphaExtractMatrix(prim.node)) continue;  // an internal pre-pass, never standalone
      auto v = ParseFloatList(Attr(prim.node, "values"));
      if (v.size() != kMatrixSize) continue;
      auto* cm = _document->makeNode<ColorMatrixFilter>();
      for (size_t n = 0; n < kMatrixSize; n++) {
        cm->matrix[n] = v[n];
      }
      prim.consumed = true;
      filter = cm;
      return static_cast<int>(i);
    }
    return kNoMatch;
  }

  // True when the feGaussianBlur at `blurIndex` immediately feeds a DropShadow colour fill (a tint
  // feColorMatrix or a feFlood mask, directly or through a feOffset). matchBlur uses this to defer
  // to matchDropShadow so a shadow's internal blur is not mistaken for a standalone BlurFilter.
  bool feedsShadowColouring(int blurIndex) const {
    int offset = consumerNamed(blurIndex, "feOffset");
    int after = (offset >= 0) ? offset : blurIndex;
    int tint = consumerMatching(after, &IsDropColorMatrix);
    // A pure-black shadow tint matrix is byte-identical to an alpha-extract pre-pass; the colour
    // fill terminates the sub-graph, whereas a pre-pass feeds a further stage. Only treat it as a
    // shadow colour fill (and defer) when it is terminal — nothing else consumes its output.
    if (tint >= 0 && !hasDownstreamConsumer(tint)) return true;
    int floodIndex = -1;
    return floodMaskedBy(after, floodIndex) >= 0;
  }

  // True when any unconsumed primitive reads the output of `producerIndex` as a plain input
  // (`in` / `in2`), excluding feMerge. Used to tell a terminal shadow colour fill (consumed only by
  // a feMerge, or nothing) from an alpha-extract pre-pass, whose output feeds a downstream blur /
  // transfer stage. feMerge is excluded because stacking the source over a finished shadow is the
  // shadow's own non-shadow-only composition, not a separate consuming stage.
  bool hasDownstreamConsumer(int producerIndex) const {
    std::string output = _graph.outputNameOf(producerIndex);
    for (size_t k = 0; k < _graph.primitives.size(); k++) {
      const auto& p = _graph.primitives[k];
      if (p.consumed || static_cast<int>(k) == producerIndex || NameIs(p.node, "feMerge")) continue;
      if (_graph.producerOf(p.in) == producerIndex || p.in == output ||
          _graph.producerOf(p.in2) == producerIndex || p.in2 == output) {
        return true;
      }
    }
    return false;
  }

  void markConsumed(int index) {
    if (index >= 0) _graph.primitives[static_cast<size_t>(index)].consumed = true;
  }
};

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
  auto filterNode = _svgEmitter.lookupSharedDef(filterId);
  if (filterNode == nullptr || !NameIs(filterNode, "filter")) {
    _diagnostics.warn("html: filter references an unknown <filter> id '" + filterId + "'; ignored");
    return {};
  }
  FilterGraph graph;
  graph.build(filterNode);
  if (graph.primitives.empty()) {
    return {};
  }

  FilterMatcher matcher(graph, _document, _valueParser);
  auto result = matcher.run();
  // Success requires that every primitive folded into some matched sub-graph; a leftover primitive
  // means the filter does something outside the five supported templates, so drop it whole rather
  // than emit a partial chain.
  if (!matcher.allConsumed()) {
    _diagnostics.warn("html: <filter> '" + filterId +
                      "' uses primitives outside the supported set; ignored");
    return {};
  }
  return result;
}

}  // namespace pagx
