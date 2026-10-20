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

#include "pagx/html/HTMLAnimationWriter.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include "base/utils/MathUtil.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/AnimationTimeline.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/utils/ExporterUtils.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

namespace {

using pag::FloatNearlyZero;

std::string StripAtPrefix(const std::string& ref) {
  if (!ref.empty() && ref[0] == '@') {
    return ref.substr(1);
  }
  return ref;
}

Node* ResolveTargetNode(PAGXDocument& doc, const std::string& target) {
  if (target.empty()) {
    return nullptr;
  }
  auto* node = doc.findNode(target);
  if (node != nullptr) {
    return node;
  }
  std::string stripped = StripAtPrefix(target);
  if (stripped != target) {
    return doc.findNode(stripped);
  }
  return nullptr;
}

bool HasPlayableAnimations(const PAGXDocument& doc) {
  for (auto* node : doc.animations) {
    if (node != nullptr && node->nodeType() == NodeType::Animation) {
      auto* anim = static_cast<const Animation*>(node);
      if (anim->duration > 0 && anim->frameRate > 0.0f && !anim->objects.empty()) {
        return true;
      }
    }
  }
  return false;
}

void IndexElements(const std::vector<Element*>& elements,
                   std::unordered_map<const SolidColor*, const Node*>& solidToPainter) {
  for (auto* el : elements) {
    if (el == nullptr) {
      continue;
    }
    auto type = el->nodeType();
    if (type == NodeType::Fill) {
      auto* fill = static_cast<const Fill*>(el);
      if (fill->color != nullptr && fill->color->nodeType() == NodeType::SolidColor) {
        solidToPainter[static_cast<const SolidColor*>(fill->color)] = fill;
      }
    } else if (type == NodeType::Stroke) {
      auto* stroke = static_cast<const Stroke*>(el);
      if (stroke->color != nullptr && stroke->color->nodeType() == NodeType::SolidColor) {
        solidToPainter[static_cast<const SolidColor*>(stroke->color)] = stroke;
      }
    } else if (type == NodeType::Group || type == NodeType::TextBox) {
      auto* grp = static_cast<const Group*>(el);
      IndexElements(grp->elements, solidToPainter);
    }
  }
}

void CollectMaskPaths(const Layer* maskLayer, const Layer* hostLayer,
                      std::unordered_map<const Path*, const Layer*>& maskPathToLayer) {
  if (maskLayer == nullptr || hostLayer == nullptr) {
    return;
  }
  for (auto* el : maskLayer->contents) {
    if (el != nullptr && el->nodeType() == NodeType::Path) {
      maskPathToLayer[static_cast<const Path*>(el)] = hostLayer;
    }
  }
  for (auto* child : maskLayer->children) {
    CollectMaskPaths(child, hostLayer, maskPathToLayer);
  }
}

void IndexLayerTree(const std::vector<Layer*>& layers,
                    std::unordered_map<const SolidColor*, const Node*>& solidToPainter,
                    std::unordered_map<const LayerFilter*, const Layer*>& filterToLayer,
                    std::unordered_map<const Path*, const Layer*>& maskPathToLayer) {
  for (auto* layer : layers) {
    if (layer == nullptr) {
      continue;
    }
    IndexElements(layer->contents, solidToPainter);
    for (auto* filter : layer->filters) {
      if (filter != nullptr) {
        filterToLayer[filter] = layer;
      }
    }
    if (layer->mask != nullptr) {
      CollectMaskPaths(layer->mask, layer, maskPathToLayer);
    }
    if (layer->composition != nullptr) {
      IndexLayerTree(layer->composition->layers, solidToPainter, filterToLayer, maskPathToLayer);
    }
    IndexLayerTree(layer->children, solidToPainter, filterToLayer, maskPathToLayer);
  }
}

void CollectTimelineOffsets(const std::vector<Layer*>& layers,
                            std::unordered_map<std::string, int64_t>& animOffsets) {
  for (auto* layer : layers) {
    if (layer == nullptr) {
      continue;
    }
    for (const auto& tl : layer->timelines) {
      if (tl != nullptr && tl->timelineType() == TimelineType::Animation) {
        auto* animTl = static_cast<const AnimationTimeline*>(tl.get());
        std::string id = StripAtPrefix(animTl->animationId);
        if (!id.empty() && animTl->evaluationOffset != 0) {
          animOffsets[id] = animTl->evaluationOffset;
        }
      }
    }
    if (layer->composition != nullptr) {
      CollectTimelineOffsets(layer->composition->layers, animOffsets);
    }
    CollectTimelineOffsets(layer->children, animOffsets);
  }
}

// Segment easing descriptor extracted from a Channel at a given keyframe time.
struct SegmentEasing {
  KeyframeInterpolationType type = KeyframeInterpolationType::Linear;
  Point p1 = {0.0f, 0.0f};
  Point p2 = {1.0f, 1.0f};

  std::string toCss() const {
    if (type == KeyframeInterpolationType::Hold || type == KeyframeInterpolationType::None) {
      return "steps(1, end)";
    }
    if (type == KeyframeInterpolationType::Bezier) {
      return "cubic-bezier(" + CssFloatToString(p1.x) + ", " + CssFloatToString(p1.y) + ", " +
             CssFloatToString(p2.x) + ", " + CssFloatToString(p2.y) + ")";
    }
    return "linear";
  }

  bool operator==(const SegmentEasing& other) const {
    if (type != other.type) {
      return false;
    }
    if (type == KeyframeInterpolationType::Bezier) {
      return FloatNearlyZero(p1.x - other.p1.x) && FloatNearlyZero(p1.y - other.p1.y) &&
             FloatNearlyZero(p2.x - other.p2.x) && FloatNearlyZero(p2.y - other.p2.y);
    }
    return true;
  }

  bool operator!=(const SegmentEasing& other) const {
    return !(*this == other);
  }
};

template <typename T>
void CollectKeyframeTimesAndEasing(const std::vector<Keyframe<T>>& keys, std::set<Frame>& timesOut,
                                   std::unordered_map<Frame, SegmentEasing>& easingOut) {
  for (size_t i = 0; i < keys.size(); ++i) {
    const auto& k = keys[i];
    timesOut.insert(k.time);
    if (i + 1 < keys.size()) {
      SegmentEasing seg = {};
      seg.type = k.interpolation;
      if (seg.type == KeyframeInterpolationType::Bezier) {
        const auto& nextK = keys[i + 1];
        Point outPt = k.bezierOut;
        Point inPt = nextK.bezierIn;
        if (FloatNearlyZero(inPt.x) && FloatNearlyZero(inPt.y) &&
            (!FloatNearlyZero(k.bezierIn.x) || !FloatNearlyZero(k.bezierIn.y))) {
          inPt = k.bezierIn;
        }
        if (FloatNearlyZero(outPt.x) && FloatNearlyZero(outPt.y) && FloatNearlyZero(inPt.x) &&
            FloatNearlyZero(inPt.y)) {
          outPt = {0.42f, 0.0f};
          inPt = {0.58f, 1.0f};
        }
        seg.p1 = outPt;
        seg.p2 = inPt;
      }
      // Prefer non-linear easing if multiple channels share a timestamp.
      auto it = easingOut.find(k.time);
      if (it == easingOut.end() || it->second.type == KeyframeInterpolationType::Linear) {
        easingOut[k.time] = seg;
      }
    }
  }
}

void ExtractChannelTimesAndEasing(const Channel* ch, std::set<Frame>& timesOut,
                                  std::unordered_map<Frame, SegmentEasing>& easingOut) {
  if (ch == nullptr) {
    return;
  }
  switch (ch->valueType()) {
    case ChannelValueType::Float:
      CollectKeyframeTimesAndEasing(static_cast<const TypedChannel<float>*>(ch)->keyframes,
                                    timesOut, easingOut);
      break;
    case ChannelValueType::Color:
      CollectKeyframeTimesAndEasing(static_cast<const TypedChannel<Color>*>(ch)->keyframes,
                                    timesOut, easingOut);
      break;
    case ChannelValueType::Matrix:
      CollectKeyframeTimesAndEasing(static_cast<const TypedChannel<Matrix>*>(ch)->keyframes,
                                    timesOut, easingOut);
      break;
    case ChannelValueType::Bool:
      CollectKeyframeTimesAndEasing(static_cast<const TypedChannel<bool>*>(ch)->keyframes, timesOut,
                                    easingOut);
      break;
    case ChannelValueType::Int:
      CollectKeyframeTimesAndEasing(static_cast<const TypedChannel<int>*>(ch)->keyframes, timesOut,
                                    easingOut);
      break;
    case ChannelValueType::String:
      CollectKeyframeTimesAndEasing(static_cast<const TypedChannel<std::string>*>(ch)->keyframes,
                                    timesOut, easingOut);
      break;
    case ChannelValueType::ImageRef:
      CollectKeyframeTimesAndEasing(static_cast<const TypedChannel<ImageRef>*>(ch)->keyframes,
                                    timesOut, easingOut);
      break;
    case ChannelValueType::PAGImage:
      break;
  }
}

float EvalFloatChannel(const Channel* ch, Frame frame, float fallback) {
  if (ch == nullptr) {
    return fallback;
  }
  KeyValue kv = ch->evaluateAt(frame);
  const auto* v = std::get_if<float>(&kv);
  return v != nullptr ? *v : fallback;
}

Color EvalColorChannel(const Channel* ch, Frame frame, const Color& fallback) {
  if (ch == nullptr) {
    return fallback;
  }
  KeyValue kv = ch->evaluateAt(frame);
  const auto* v = std::get_if<Color>(&kv);
  return v != nullptr ? *v : fallback;
}

Matrix EvalMatrixChannel(const Channel* ch, Frame frame, const Matrix& fallback) {
  if (ch == nullptr) {
    return fallback;
  }
  KeyValue kv = ch->evaluateAt(frame);
  const auto* v = std::get_if<Matrix>(&kv);
  return v != nullptr ? *v : fallback;
}

std::string FormatMatrixCss(const Matrix& m) {
  return "matrix(" + CssFloatToString(m.a) + "," + CssFloatToString(m.b) + "," +
         CssFloatToString(m.c) + "," + CssFloatToString(m.d) + "," + CssFloatToString(m.tx) + "," +
         CssFloatToString(m.ty) + ")";
}

std::string FormatPercent(Frame time, Frame duration) {
  if (duration <= 0) {
    return "0%";
  }
  float pct = static_cast<float>(time) * 100.0f / static_cast<float>(duration);
  pct = std::clamp(pct, 0.0f, 100.0f);
  return CssFloatToString(pct) + "%";
}

void AppendCssDecl(std::string& decls, const std::string& prop, const std::string& val) {
  if (!decls.empty()) {
    decls += "; ";
  }
  decls += prop + ": " + val;
}

// Finds the opening tag `<tagName ... id="domId" ...>` in `html` and returns the tag name
// (lowercased) as well as `[tagStart, tagEnd)` bounds.
bool FindElementOpeningTagById(const std::string& html, const std::string& domId,
                               size_t& outTagStart, size_t& outTagEnd, std::string& outTagName) {
  std::string needle = "id=\"" + domId + "\"";
  size_t searchPos = 0;
  while (searchPos < html.size()) {
    size_t idPos = html.find(needle, searchPos);
    if (idPos == std::string::npos) {
      return false;
    }
    // Verify attribute boundary before `id=`.
    if (idPos == 0 || (html[idPos - 1] != ' ' && html[idPos - 1] != '\t' && html[idPos - 1] != '\n')) {
      searchPos = idPos + needle.size();
      continue;
    }
    size_t tagStart = html.rfind('<', idPos);
    size_t tagEnd = html.find('>', idPos);
    if (tagStart == std::string::npos || tagEnd == std::string::npos) {
      return false;
    }
    size_t nameStart = tagStart + 1;
    size_t nameEnd = nameStart;
    while (nameEnd < tagEnd && html[nameEnd] != ' ' && html[nameEnd] != '\t' &&
           html[nameEnd] != '/' && html[nameEnd] != '>') {
      nameEnd++;
    }
    outTagName = html.substr(nameStart, nameEnd - nameStart);
    for (char& c : outTagName) {
      if (c >= 'A' && c <= 'Z') {
        c = static_cast<char>(c - 'A' + 'a');
      }
    }
    outTagStart = tagStart;
    outTagEnd = tagEnd + 1;
    return true;
  }
  return false;
}

bool ParsePointChannelName(const std::string& name, size_t& outIndex, bool& outIsX) {
  if (name.rfind("point", 0) != 0) {
    return false;
  }
  size_t dotPos = name.find('.', 5);
  if (dotPos == std::string::npos || dotPos == 5) {
    return false;
  }
  std::string idxStr = name.substr(5, dotPos - 5);
  for (char c : idxStr) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  std::string axis = name.substr(dotPos + 1);
  if (axis == "x") {
    outIsX = true;
  } else if (axis == "y") {
    outIsX = false;
  } else {
    return false;
  }
  outIndex = static_cast<size_t>(std::strtoul(idxStr.c_str(), nullptr, 10));
  return true;
}

std::string RebuildAnimatedPathSVGString(const Path* pathNode,
                                         const std::unordered_map<size_t, const Channel*>& xChs,
                                         const std::unordered_map<size_t, const Channel*>& yChs,
                                         Frame frame) {
  if (pathNode == nullptr || pathNode->data == nullptr || pathNode->data->isEmpty()) {
    return "";
  }
  const PathData& base = *pathNode->data;
  const auto& verbs = base.verbs();
  std::vector<Point> pts = base.points();
  for (const auto& kv : xChs) {
    if (kv.first < pts.size()) {
      pts[kv.first].x = EvalFloatChannel(kv.second, frame, pts[kv.first].x);
    }
  }
  for (const auto& kv : yChs) {
    if (kv.first < pts.size()) {
      pts[kv.first].y = EvalFloatChannel(kv.second, frame, pts[kv.first].y);
    }
  }
  PathData rebuilt = PathDataFromSVGString("");
  size_t ptIdx = 0;
  for (auto verb : verbs) {
    switch (verb) {
      case PathVerb::Move:
        if (ptIdx < pts.size()) {
          rebuilt.moveTo(pts[ptIdx].x, pts[ptIdx].y);
          ptIdx += 1;
        }
        break;
      case PathVerb::Line:
        if (ptIdx < pts.size()) {
          rebuilt.lineTo(pts[ptIdx].x, pts[ptIdx].y);
          ptIdx += 1;
        }
        break;
      case PathVerb::Quad:
        if (ptIdx + 1 < pts.size()) {
          rebuilt.quadTo(pts[ptIdx].x, pts[ptIdx].y, pts[ptIdx + 1].x, pts[ptIdx + 1].y);
          ptIdx += 2;
        }
        break;
      case PathVerb::Cubic:
        if (ptIdx + 2 < pts.size()) {
          rebuilt.cubicTo(pts[ptIdx].x, pts[ptIdx].y, pts[ptIdx + 1].x, pts[ptIdx + 1].y,
                          pts[ptIdx + 2].x, pts[ptIdx + 2].y);
          ptIdx += 3;
        }
        break;
      case PathVerb::Close:
        rebuilt.close();
        break;
    }
  }
  auto pos = pathNode->renderPosition();
  float scale = pathNode->renderScale();
  Matrix m = Matrix::Translate(pos.x, pos.y) * Matrix::Scale(scale, scale);
  return m.isIdentity() ? PathDataToSVGString(rebuilt) : TransformPathDataToSVG(rebuilt, m);
}

// Injects `animation` declaration (and optional extra style properties such as
// `transform-origin:0 0`) into a single opening tag with `id="domId"` in `bodyHtml`.
bool WeaveStyleIntoSingleElementById(std::string& bodyHtml, const std::string& domId,
                                     const std::string& animDecl, bool needsZeroTransformOrigin) {
  size_t tagStart = 0;
  size_t tagEnd = 0;
  std::string tagName;
  if (!FindElementOpeningTagById(bodyHtml, domId, tagStart, tagEnd, tagName)) {
    return false;
  }
  std::string tagStr = bodyHtml.substr(tagStart, tagEnd - tagStart);
  size_t stylePos = tagStr.find(" style=\"");
  if (stylePos != std::string::npos) {
    size_t valStart = stylePos + 8;
    size_t valEnd = tagStr.find('"', valStart);
    if (valEnd != std::string::npos) {
      std::string styleVal = tagStr.substr(valStart, valEnd - valStart);
      if (needsZeroTransformOrigin && styleVal.find("transform-origin:") == std::string::npos) {
        if (!styleVal.empty() && styleVal.back() != ';') {
          styleVal += ';';
        }
        styleVal += "transform-origin:0 0";
      }
      if (!styleVal.empty() && styleVal.back() != ';') {
        styleVal += ';';
      }
      styleVal += "animation:" + animDecl;
      tagStr.replace(valStart, valEnd - valStart, styleVal);
      bodyHtml.replace(tagStart, tagEnd - tagStart, tagStr);
      return true;
    }
  }
  // No existing style="..." attribute on the tag (e.g. SVG <path/>). Insert one before `/>` or `>`.
  std::string newStyle = " style=\"";
  if (needsZeroTransformOrigin) {
    newStyle += "transform-origin:0 0;";
  }
  newStyle += "animation:" + animDecl + "\"";
  size_t insertPos = tagStr.size() - 1;
  if (insertPos > 0 && tagStr[insertPos - 1] == '/') {
    insertPos -= 1;
  }
  tagStr.insert(insertPos, newStyle);
  bodyHtml.replace(tagStart, tagEnd - tagStart, tagStr);
  return true;
}

// Weaves `animation` declaration into `id="domId"` and any multi-geometry sibling replicas
// (`id="domId__1"`, `id="domId__2"`, ...) emitted by `renderSVG` when a single animated Fill/Stroke
// paints multiple geometries.
void WeaveStyleIntoElementById(std::string& bodyHtml, const std::string& domId,
                               const std::string& animDecl, bool needsZeroTransformOrigin) {
  if (!WeaveStyleIntoSingleElementById(bodyHtml, domId, animDecl, needsZeroTransformOrigin)) {
    return;
  }
  for (size_t replicaIdx = 1;; ++replicaIdx) {
    std::string replicaId = domId + "__" + std::to_string(replicaIdx);
    if (!WeaveStyleIntoSingleElementById(bodyHtml, replicaId, animDecl, needsZeroTransformOrigin)) {
      break;
    }
  }
}

// ============================================================================
// Centralized NodeType + Channel Name -> Semantic Role Mapping Table
// ============================================================================
// To support a new PAGX animated property in HTML export:
//   1. Add a semantic role in `TargetChannelRole` (if not already covered).
//   2. Register `{NodeType::..., "channelName", TargetChannelRole::...}` in `kChannelBindingTable`.
//   3. Handle the role in `BindChannelToBundleSet` and the corresponding `Emit*Css` helper.

enum class TargetChannelRole {
  Unknown = 0,
  Opacity,
  LayerX,
  LayerY,
  LayerMatrix,
  GroupPosX,
  GroupPosY,
  Rotation,
  ScaleX,
  ScaleY,
  FillColor,
  FillAlpha,
  StrokeColor,
  StrokeAlpha,
  StrokeWidth,
  StrokeDashOffset,
  SolidColorValue,
  SolidColorAlpha,
  FilterBlurX,
  FilterBlurY,
  FilterOffsetX,
  FilterOffsetY,
  FilterColor,
  FilterAlpha,
};

struct ChannelBindingRule {
  NodeType nodeType;
  const char* channelName;
  TargetChannelRole role;
};

constexpr ChannelBindingRule kChannelBindingTable[] = {
    // Layer channels
    {NodeType::Layer, "alpha", TargetChannelRole::Opacity},
    {NodeType::Layer, "x", TargetChannelRole::LayerX},
    {NodeType::Layer, "y", TargetChannelRole::LayerY},
    {NodeType::Layer, "matrix", TargetChannelRole::LayerMatrix},
    {NodeType::Layer, "rotation", TargetChannelRole::Rotation},
    {NodeType::Layer, "scale.x", TargetChannelRole::ScaleX},
    {NodeType::Layer, "scale.y", TargetChannelRole::ScaleY},

    // Group & TextBox channels
    {NodeType::Group, "alpha", TargetChannelRole::Opacity},
    {NodeType::Group, "position.x", TargetChannelRole::GroupPosX},
    {NodeType::Group, "position.y", TargetChannelRole::GroupPosY},
    {NodeType::Group, "rotation", TargetChannelRole::Rotation},
    {NodeType::Group, "scale.x", TargetChannelRole::ScaleX},
    {NodeType::Group, "scale.y", TargetChannelRole::ScaleY},
    {NodeType::TextBox, "alpha", TargetChannelRole::Opacity},
    {NodeType::TextBox, "position.x", TargetChannelRole::GroupPosX},
    {NodeType::TextBox, "position.y", TargetChannelRole::GroupPosY},
    {NodeType::TextBox, "rotation", TargetChannelRole::Rotation},
    {NodeType::TextBox, "scale.x", TargetChannelRole::ScaleX},
    {NodeType::TextBox, "scale.y", TargetChannelRole::ScaleY},

    // Painter & ColorResource channels
    {NodeType::Fill, "color", TargetChannelRole::FillColor},
    {NodeType::Fill, "alpha", TargetChannelRole::FillAlpha},
    {NodeType::Stroke, "color", TargetChannelRole::StrokeColor},
    {NodeType::Stroke, "alpha", TargetChannelRole::StrokeAlpha},
    {NodeType::Stroke, "width", TargetChannelRole::StrokeWidth},
    {NodeType::Stroke, "dashOffset", TargetChannelRole::StrokeDashOffset},
    {NodeType::SolidColor, "color", TargetChannelRole::SolidColorValue},
    {NodeType::SolidColor, "alpha", TargetChannelRole::SolidColorAlpha},

    // LayerFilter channels
    {NodeType::BlurFilter, "blurX", TargetChannelRole::FilterBlurX},
    {NodeType::BlurFilter, "blurY", TargetChannelRole::FilterBlurY},
    {NodeType::DropShadowFilter, "blurX", TargetChannelRole::FilterBlurX},
    {NodeType::DropShadowFilter, "blurY", TargetChannelRole::FilterBlurY},
    {NodeType::DropShadowFilter, "offsetX", TargetChannelRole::FilterOffsetX},
    {NodeType::DropShadowFilter, "offsetY", TargetChannelRole::FilterOffsetY},
    {NodeType::DropShadowFilter, "color", TargetChannelRole::FilterColor},
    {NodeType::DropShadowFilter, "alpha", TargetChannelRole::FilterAlpha},
};

TargetChannelRole LookupChannelRole(NodeType nodeType, const std::string& channelName) {
  for (const auto& rule : kChannelBindingTable) {
    if (rule.nodeType == nodeType && channelName == rule.channelName) {
      return rule.role;
    }
  }
  return TargetChannelRole::Unknown;
}

// ============================================================================
// Per-DOM-Target Bundle Data Structures & Pipeline Step Helpers
// ============================================================================

struct DomTargetBundle {
  std::string domId;
  const Node* primaryHost = nullptr;
  std::vector<const AnimationObject*> objects = {};
};

struct FilterChannelSlot {
  const LayerFilter* filter = nullptr;
  const Channel* blurX = nullptr;
  const Channel* blurY = nullptr;
  const Channel* offsetX = nullptr;
  const Channel* offsetY = nullptr;
  const Channel* color = nullptr;
  const Channel* alpha = nullptr;
};

struct BundleChannelSet {
  const Channel* alphaCh = nullptr;
  const Channel* xCh = nullptr;
  const Channel* yCh = nullptr;
  const Channel* matrixCh = nullptr;
  const Channel* rotCh = nullptr;
  const Channel* scaleXCh = nullptr;
  const Channel* scaleYCh = nullptr;
  const Channel* posXCh = nullptr;
  const Channel* posYCh = nullptr;
  const Channel* fillColorCh = nullptr;
  const Channel* fillAlphaCh = nullptr;
  const Channel* strokeColorCh = nullptr;
  const Channel* strokeAlphaCh = nullptr;
  const Channel* strokeWidthCh = nullptr;
  const Channel* dashOffsetCh = nullptr;

  std::unordered_map<const LayerFilter*, FilterChannelSlot> filterChMap = {};
  bool hasFilterAnim = false;

  const Path* animatedMaskPath = nullptr;
  std::unordered_map<size_t, const Channel*> maskPtX = {};
  std::unordered_map<size_t, const Channel*> maskPtY = {};

  std::set<Frame> keyTimes = {};
  std::unordered_map<Frame, SegmentEasing> stopEasings = {};
};

struct BundleTimingInfo {
  Frame bundleShiftFrames = 0;
  Frame effectiveDuration = 0;
  bool uniformEasing = true;
  SegmentEasing commonEasing = {};
};

void BindChannelToBundleSet(const Node* objTarget, const Channel* ch,
                            const std::unordered_map<const SolidColor*, const Node*>& solidToPainter,
                            BundleChannelSet& out) {
  auto tType = objTarget->nodeType();
  if (tType == NodeType::Path) {
    size_t ptIdx = 0;
    bool isX = false;
    if (ParsePointChannelName(ch->name, ptIdx, isX)) {
      out.animatedMaskPath = static_cast<const Path*>(objTarget);
      if (isX) {
        out.maskPtX[ptIdx] = ch;
      } else {
        out.maskPtY[ptIdx] = ch;
      }
    }
    return;
  }

  TargetChannelRole role = LookupChannelRole(tType, ch->name);
  switch (role) {
    case TargetChannelRole::Opacity:
      out.alphaCh = ch;
      break;
    case TargetChannelRole::LayerX:
      out.xCh = ch;
      break;
    case TargetChannelRole::LayerY:
      out.yCh = ch;
      break;
    case TargetChannelRole::LayerMatrix:
      out.matrixCh = ch;
      break;
    case TargetChannelRole::GroupPosX:
      out.posXCh = ch;
      break;
    case TargetChannelRole::GroupPosY:
      out.posYCh = ch;
      break;
    case TargetChannelRole::Rotation:
      out.rotCh = ch;
      break;
    case TargetChannelRole::ScaleX:
      out.scaleXCh = ch;
      break;
    case TargetChannelRole::ScaleY:
      out.scaleYCh = ch;
      break;
    case TargetChannelRole::FillColor:
      out.fillColorCh = ch;
      break;
    case TargetChannelRole::FillAlpha:
      out.fillAlphaCh = ch;
      break;
    case TargetChannelRole::StrokeColor:
      out.strokeColorCh = ch;
      break;
    case TargetChannelRole::StrokeAlpha:
      out.strokeAlphaCh = ch;
      break;
    case TargetChannelRole::StrokeWidth:
      out.strokeWidthCh = ch;
      break;
    case TargetChannelRole::StrokeDashOffset:
      out.dashOffsetCh = ch;
      break;
    case TargetChannelRole::SolidColorValue:
    case TargetChannelRole::SolidColorAlpha: {
      auto sIt = solidToPainter.find(static_cast<const SolidColor*>(objTarget));
      if (sIt != solidToPainter.end() && sIt->second != nullptr) {
        if (sIt->second->nodeType() == NodeType::Fill) {
          if (role == TargetChannelRole::SolidColorValue) {
            out.fillColorCh = ch;
          } else {
            out.fillAlphaCh = ch;
          }
        } else if (sIt->second->nodeType() == NodeType::Stroke) {
          if (role == TargetChannelRole::SolidColorValue) {
            out.strokeColorCh = ch;
          } else {
            out.strokeAlphaCh = ch;
          }
        }
      }
      break;
    }
    case TargetChannelRole::FilterBlurX:
    case TargetChannelRole::FilterBlurY:
    case TargetChannelRole::FilterOffsetX:
    case TargetChannelRole::FilterOffsetY:
    case TargetChannelRole::FilterColor:
    case TargetChannelRole::FilterAlpha: {
      auto* flt = static_cast<const LayerFilter*>(objTarget);
      auto& fc = out.filterChMap[flt];
      fc.filter = flt;
      out.hasFilterAnim = true;
      if (role == TargetChannelRole::FilterBlurX) {
        fc.blurX = ch;
      } else if (role == TargetChannelRole::FilterBlurY) {
        fc.blurY = ch;
      } else if (role == TargetChannelRole::FilterOffsetX) {
        fc.offsetX = ch;
      } else if (role == TargetChannelRole::FilterOffsetY) {
        fc.offsetY = ch;
      } else if (role == TargetChannelRole::FilterColor) {
        fc.color = ch;
      } else if (role == TargetChannelRole::FilterAlpha) {
        fc.alpha = ch;
      }
      break;
    }
    case TargetChannelRole::Unknown:
      break;
  }
}

BundleChannelSet CollectBundleChannels(
    PAGXDocument& doc, const DomTargetBundle& bundle,
    const std::unordered_map<const SolidColor*, const Node*>& solidToPainter) {
  BundleChannelSet result = {};
  for (auto* obj : bundle.objects) {
    Node* objTarget = ResolveTargetNode(doc, obj->target);
    if (objTarget == nullptr) {
      continue;
    }
    for (auto* ch : obj->channels) {
      if (ch == nullptr) {
        continue;
      }
      ExtractChannelTimesAndEasing(ch, result.keyTimes, result.stopEasings);
      BindChannelToBundleSet(objTarget, ch, solidToPainter, result);
    }
  }
  return result;
}

BundleTimingInfo ComputeBundleTiming(const Animation* anim, int64_t evalOffsetFrames,
                                     const BundleChannelSet& chSet) {
  BundleTimingInfo timing = {};
  Frame minKeyTime = *chSet.keyTimes.begin();
  Frame maxKeyTime = *chSet.keyTimes.rbegin();

  // Detect per-bundle keyframe shift when multiple delayed animations were coalesced
  // into a single Animation with duration D, leaving this bundle's keyframes on [D0, D0 + D]
  // where maxKeyTime > anim->duration, OR when LoopMode::Once has no explicit evaluationOffset
  // and keyframes start at minKeyTime > 0 (HTMLAnimationBuilder bakes delayFrames into duration
  // and keyframe times for LoopMode::Once).
  if (minKeyTime > 0 &&
      (maxKeyTime > anim->duration || (anim->loop == LoopMode::Once && evalOffsetFrames == 0))) {
    timing.bundleShiftFrames = minKeyTime;
  }

  timing.effectiveDuration = anim->duration;
  if (timing.bundleShiftFrames > 0) {
    if (maxKeyTime > anim->duration) {
      timing.effectiveDuration = maxKeyTime - timing.bundleShiftFrames;
    } else if (anim->duration > timing.bundleShiftFrames) {
      timing.effectiveDuration = anim->duration - timing.bundleShiftFrames;
    }
  } else if (maxKeyTime > timing.effectiveDuration) {
    timing.effectiveDuration = maxKeyTime;
  }

  bool sawFirstEasing = false;
  for (const auto& kv : chSet.stopEasings) {
    if (!sawFirstEasing) {
      timing.commonEasing = kv.second;
      sawFirstEasing = true;
    } else if (timing.commonEasing != kv.second) {
      timing.uniformEasing = false;
    }
  }
  return timing;
}

void EmitTransformCssAtFrame(const DomTargetBundle& bundle, const BundleChannelSet& chSet, Frame t,
                             std::string& decls, bool& needsZeroOrigin) {
  if (chSet.matrixCh != nullptr) {
    Matrix m = EvalMatrixChannel(chSet.matrixCh, t, Matrix::Identity());
    AppendCssDecl(decls, "transform", FormatMatrixCss(m));
    needsZeroOrigin = true;
    return;
  }
  if (chSet.xCh != nullptr || chSet.yCh != nullptr) {
    float baseX = 0.0f;
    float baseY = 0.0f;
    if (bundle.primaryHost != nullptr && bundle.primaryHost->nodeType() == NodeType::Layer) {
      auto renderPos = static_cast<const Layer*>(bundle.primaryHost)->renderPosition();
      baseX = renderPos.x;
      baseY = renderPos.y;
    }
    float xVal = EvalFloatChannel(chSet.xCh, t, baseX);
    float yVal = EvalFloatChannel(chSet.yCh, t, baseY);
    float dx = xVal - baseX;
    float dy = yVal - baseY;
    AppendCssDecl(decls, "transform",
                  "translate(" + CssFloatToString(dx) + "px, " + CssFloatToString(dy) + "px)");
    return;
  }
  if (chSet.rotCh != nullptr || chSet.scaleXCh != nullptr || chSet.scaleYCh != nullptr ||
      chSet.posXCh != nullptr || chSet.posYCh != nullptr) {
    float basePosX = 0.0f;
    float basePosY = 0.0f;
    float baseRot = 0.0f;
    float baseSX = 1.0f;
    float baseSY = 1.0f;
    Point anchor = {0.0f, 0.0f};
    bool isTextBoxHost =
        (bundle.primaryHost != nullptr && bundle.primaryHost->nodeType() == NodeType::TextBox);
    if (bundle.primaryHost != nullptr &&
        (bundle.primaryHost->nodeType() == NodeType::Group || isTextBoxHost)) {
      auto* grp = static_cast<const Group*>(bundle.primaryHost);
      auto rp = grp->renderPosition();
      basePosX = rp.x;
      basePosY = rp.y;
      baseRot = grp->rotation;
      baseSX = grp->scale.x;
      baseSY = grp->scale.y;
      anchor = grp->anchor;
    }
    float px = EvalFloatChannel(chSet.posXCh, t, basePosX);
    float py = EvalFloatChannel(chSet.posYCh, t, basePosY);
    // Unlike Group (`position:relative` at 0,0 with renderPosition baked into its CSS transform),
    // TextBox containers already emit `position:absolute;left:basePosX;top:basePosY`, so any CSS
    // translate() on a TextBox must be relative to (basePosX, basePosY).
    if (isTextBoxHost) {
      px -= basePosX;
      py -= basePosY;
    }
    float rot = EvalFloatChannel(chSet.rotCh, t, baseRot);
    float sx = EvalFloatChannel(chSet.scaleXCh, t, baseSX);
    float sy = EvalFloatChannel(chSet.scaleYCh, t, baseSY);

    std::string tf;
    if (!FloatNearlyZero(px) || !FloatNearlyZero(py) || chSet.posXCh != nullptr ||
        chSet.posYCh != nullptr) {
      tf += "translate(" + CssFloatToString(px) + "px, " + CssFloatToString(py) + "px)";
    }
    if (chSet.rotCh != nullptr || !FloatNearlyZero(rot)) {
      if (!tf.empty()) tf += ' ';
      tf += "rotate(" + CssFloatToString(rot) + "deg)";
    }
    if (chSet.scaleXCh != nullptr || chSet.scaleYCh != nullptr || !FloatNearlyZero(sx - 1.0f) ||
        !FloatNearlyZero(sy - 1.0f)) {
      if (!tf.empty()) tf += ' ';
      if (FloatNearlyZero(sx - sy)) {
        tf += "scale(" + CssFloatToString(sx) + ")";
      } else {
        tf += "scale(" + CssFloatToString(sx) + ", " + CssFloatToString(sy) + ")";
      }
    }
    if (!FloatNearlyZero(anchor.x) || !FloatNearlyZero(anchor.y)) {
      if (!tf.empty()) tf += ' ';
      tf += "translate(" + CssFloatToString(-anchor.x) + "px, " + CssFloatToString(-anchor.y) +
            "px)";
    }
    if (tf.empty()) {
      tf = "rotate(0deg)";
    }
    AppendCssDecl(decls, "transform", tf);
    needsZeroOrigin = true;
  }
}

void EmitPaintCssAtFrame(const BundleChannelSet& chSet, const std::string& tagName, Frame t,
                         std::string& decls) {
  bool isSvgShape =
      (tagName == "path" || tagName == "rect" || tagName == "ellipse" || tagName == "use");
  if (chSet.fillColorCh != nullptr) {
    Color c = EvalColorChannel(chSet.fillColorCh, t, Color{});
    if (tagName == "span") {
      AppendCssDecl(decls, "color", ColorToRGBA(c));
    } else if (isSvgShape) {
      AppendCssDecl(decls, "fill", ColorToSVGHex(c));
    } else {
      AppendCssDecl(decls, "background-color", ColorToRGBA(c));
    }
  }
  if (chSet.fillAlphaCh != nullptr) {
    float fa = EvalFloatChannel(chSet.fillAlphaCh, t, 1.0f);
    AppendCssDecl(decls, isSvgShape ? "fill-opacity" : "opacity", CssFloatToString(fa));
  }
  if (chSet.strokeColorCh != nullptr) {
    Color sc = EvalColorChannel(chSet.strokeColorCh, t, Color{});
    AppendCssDecl(decls, "stroke", ColorToSVGHex(sc));
  }
  if (chSet.strokeAlphaCh != nullptr) {
    float sa = EvalFloatChannel(chSet.strokeAlphaCh, t, 1.0f);
    AppendCssDecl(decls, "stroke-opacity", CssFloatToString(sa));
  }
  if (chSet.strokeWidthCh != nullptr) {
    float sw = EvalFloatChannel(chSet.strokeWidthCh, t, 1.0f);
    AppendCssDecl(decls, "stroke-width", CssFloatToString(sw) + "px");
  }
  if (chSet.dashOffsetCh != nullptr) {
    float doff = EvalFloatChannel(chSet.dashOffsetCh, t, 0.0f);
    AppendCssDecl(decls, "stroke-dashoffset", CssFloatToString(doff));
  }
}

void EmitFilterCssAtFrame(const DomTargetBundle& bundle, const BundleChannelSet& chSet, Frame t,
                          std::string& decls) {
  if (!chSet.hasFilterAnim || bundle.primaryHost == nullptr ||
      bundle.primaryHost->nodeType() != NodeType::Layer) {
    return;
  }
  auto* hostLayer = static_cast<const Layer*>(bundle.primaryHost);
  std::string filterVal;
  for (auto* f : hostLayer->filters) {
    if (f == nullptr) {
      continue;
    }
    auto fIt = chSet.filterChMap.find(f);
    const FilterChannelSlot* fc = (fIt != chSet.filterChMap.end()) ? &fIt->second : nullptr;
    if (f->nodeType() == NodeType::BlurFilter) {
      auto* bf = static_cast<const BlurFilter*>(f);
      float bx = fc ? EvalFloatChannel(fc->blurX, t, bf->blurX) : bf->blurX;
      float by = fc ? EvalFloatChannel(fc->blurY, t, bf->blurY) : bf->blurY;
      float r = std::max(bx, by);
      if (!filterVal.empty()) filterVal += ' ';
      filterVal += "blur(" + CssFloatToString(r) + "px)";
    } else if (f->nodeType() == NodeType::DropShadowFilter) {
      auto* ds = static_cast<const DropShadowFilter*>(f);
      float ox = fc ? EvalFloatChannel(fc->offsetX, t, ds->offsetX) : ds->offsetX;
      float oy = fc ? EvalFloatChannel(fc->offsetY, t, ds->offsetY) : ds->offsetY;
      float bx = fc ? EvalFloatChannel(fc->blurX, t, ds->blurX) : ds->blurX;
      float by = fc ? EvalFloatChannel(fc->blurY, t, ds->blurY) : ds->blurY;
      Color col = fc ? EvalColorChannel(fc->color, t, ds->color) : ds->color;
      float a = fc && fc->alpha ? EvalFloatChannel(fc->alpha, t, col.alpha) : col.alpha;
      if (!filterVal.empty()) filterVal += ' ';
      filterVal += "drop-shadow(" + CssFloatToString(ox) + "px " + CssFloatToString(oy) + "px " +
                   CssFloatToString(std::max(bx, by)) + "px " + ColorToRGBA(col, a) + ")";
    }
  }
  if (!filterVal.empty()) {
    AppendCssDecl(decls, "filter", filterVal);
  }
}

std::string EvaluateKeyframeCssDecls(const DomTargetBundle& bundle, const BundleChannelSet& chSet,
                                     const BundleTimingInfo& timing, const std::string& tagName,
                                     Frame t, bool& needsZeroOrigin) {
  std::string decls;
  if (chSet.alphaCh != nullptr) {
    float a = EvalFloatChannel(chSet.alphaCh, t, 1.0f);
    AppendCssDecl(decls, "opacity", CssFloatToString(a));
  }
  EmitTransformCssAtFrame(bundle, chSet, t, decls, needsZeroOrigin);
  EmitPaintCssAtFrame(chSet, tagName, t, decls);
  EmitFilterCssAtFrame(bundle, chSet, t, decls);

  if (chSet.animatedMaskPath != nullptr) {
    std::string dStr =
        RebuildAnimatedPathSVGString(chSet.animatedMaskPath, chSet.maskPtX, chSet.maskPtY, t);
    AppendCssDecl(decls, "clip-path", "path(\"" + dStr + "\")");
  }

  auto easeIt = chSet.stopEasings.find(t);
  if (easeIt != chSet.stopEasings.end() &&
      (easeIt->second.type != KeyframeInterpolationType::Linear || !timing.uniformEasing)) {
    AppendCssDecl(decls, "animation-timing-function", easeIt->second.toCss());
  }
  return decls;
}

std::string MakeSafeKeyframesName(const std::string& domId, int animIndex) {
  std::string kfName = "pagx_" + domId + "_" + std::to_string(animIndex);
  for (char& c : kfName) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
          c == '_' || c == '-')) {
      c = '_';
    }
  }
  return kfName;
}

std::string BuildAnimationShorthand(const Animation* anim, const std::string& kfName,
                                    const BundleTimingInfo& timing, int64_t evalOffsetFrames) {
  float durationSec = static_cast<float>(timing.effectiveDuration) / anim->frameRate;
  std::string timingStr = timing.uniformEasing ? timing.commonEasing.toCss() : "linear";
  std::string shorthand = kfName + " " + CssFloatToString(durationSec) + "s " + timingStr;

  int64_t totalDelayFrames = evalOffsetFrames + static_cast<int64_t>(timing.bundleShiftFrames);
  if (totalDelayFrames > 0) {
    float delaySec = static_cast<float>(totalDelayFrames) / anim->frameRate;
    shorthand += " " + CssFloatToString(delaySec) + "s";
  }

  if (anim->loop == LoopMode::Loop) {
    shorthand += (timing.bundleShiftFrames > 0) ? " infinite both" : " infinite";
  } else if (anim->loop == LoopMode::PingPong) {
    shorthand +=
        (timing.bundleShiftFrames > 0) ? " infinite alternate both" : " infinite alternate";
  } else {
    shorthand += (timing.bundleShiftFrames > 0) ? " 1 both" : " 1 forwards";
  }
  return shorthand;
}

std::vector<DomTargetBundle> GroupObjectsIntoDomBundles(
    PAGXDocument& doc, const Animation* anim, const HTMLWriterContext& ctx,
    const std::unordered_map<const SolidColor*, const Node*>& solidToPainter,
    const std::unordered_map<const LayerFilter*, const Layer*>& filterToLayer,
    const std::unordered_map<const Path*, const Layer*>& maskPathToLayer) {
  std::vector<DomTargetBundle> bundles = {};
  std::unordered_map<std::string, size_t> bundleIndex = {};

  for (auto* obj : anim->objects) {
    if (obj == nullptr || obj->channels.empty()) {
      continue;
    }
    Node* targetNode = ResolveTargetNode(doc, obj->target);
    if (targetNode == nullptr) {
      continue;
    }
    std::string domId;
    const Node* primaryHost = targetNode;
    auto nType = targetNode->nodeType();
    if (nType == NodeType::Layer) {
      domId = targetNode->id;
    } else if (nType == NodeType::Group || nType == NodeType::TextBox || nType == NodeType::Fill ||
               nType == NodeType::Stroke) {
      auto it = ctx.animatedElementIds.find(targetNode);
      if (it != ctx.animatedElementIds.end()) {
        domId = it->second;
      }
    } else if (nType == NodeType::SolidColor) {
      auto sIt = solidToPainter.find(static_cast<const SolidColor*>(targetNode));
      if (sIt != solidToPainter.end() && sIt->second != nullptr) {
        primaryHost = sIt->second;
        auto idIt = ctx.animatedElementIds.find(primaryHost);
        if (idIt != ctx.animatedElementIds.end()) {
          domId = idIt->second;
        }
      }
    } else if (nType == NodeType::BlurFilter || nType == NodeType::DropShadowFilter) {
      auto fIt = filterToLayer.find(static_cast<const LayerFilter*>(targetNode));
      if (fIt != filterToLayer.end() && fIt->second != nullptr) {
        primaryHost = fIt->second;
        domId = primaryHost->id;
      }
    } else if (nType == NodeType::Path) {
      auto* pathNode = static_cast<const Path*>(targetNode);
      auto mIt = maskPathToLayer.find(pathNode);
      if (mIt != maskPathToLayer.end() && mIt->second != nullptr) {
        primaryHost = mIt->second;
        domId = primaryHost->id;
      } else {
        auto idIt = ctx.animatedElementIds.find(pathNode);
        if (idIt != ctx.animatedElementIds.end()) {
          domId = idIt->second;
        }
      }
    }

    if (domId.empty()) {
      continue;
    }
    auto bIt = bundleIndex.find(domId);
    if (bIt == bundleIndex.end()) {
      bundleIndex[domId] = bundles.size();
      DomTargetBundle b;
      b.domId = domId;
      b.primaryHost = primaryHost;
      b.objects.push_back(obj);
      bundles.push_back(std::move(b));
    } else {
      bundles[bIt->second].objects.push_back(obj);
    }
  }
  return bundles;
}

std::string EmitBundleKeyframesRule(const std::string& kfName, const DomTargetBundle& bundle,
                                    const BundleChannelSet& chSet, const BundleTimingInfo& timing,
                                    const std::string& tagName, bool& outNeedsZeroOrigin) {
  std::string rules = "@keyframes " + kfName + " {\n";
  for (Frame t : chSet.keyTimes) {
    std::string decls =
        EvaluateKeyframeCssDecls(bundle, chSet, timing, tagName, t, outNeedsZeroOrigin);
    Frame relTime = (t >= timing.bundleShiftFrames) ? (t - timing.bundleShiftFrames) : 0;
    rules += "  " + FormatPercent(relTime, timing.effectiveDuration) + " { " + decls + "; }\n";
  }
  rules += "}\n";
  return rules;
}

}  // namespace

HTMLAnimationSession::HTMLAnimationSession(PAGXDocument& doc, HTMLWriterContext& ctx, bool enabled)
    : _doc(doc), _ctx(ctx) {
  if (!enabled || !HasPlayableAnimations(doc)) {
    return;
  }
  _active = true;

  IndexLayerTree(doc.layers, _solidToPainter, _filterToLayer, _maskPathToLayer);

  for (auto* animNode : doc.animations) {
    if (animNode == nullptr || animNode->nodeType() != NodeType::Animation) {
      continue;
    }
    auto* anim = static_cast<Animation*>(animNode);
    for (auto* obj : anim->objects) {
      if (obj == nullptr || obj->channels.empty()) {
        continue;
      }
      Node* targetNode = ResolveTargetNode(doc, obj->target);
      if (targetNode == nullptr) {
        continue;
      }
      auto nType = targetNode->nodeType();
      if (nType == NodeType::Layer) {
        auto* layer = static_cast<Layer*>(targetNode);
        if (layer->id.empty()) {
          _restoredNodeIds.emplace_back(layer, layer->id);
          layer->id = _ctx.nextId("animL");
        }
        for (auto* ch : obj->channels) {
          if (ch != nullptr && LookupChannelRole(nType, ch->name) == TargetChannelRole::Opacity &&
              !layer->groupOpacity) {
            _restoredGroupOpacity.emplace_back(layer, layer->groupOpacity);
            layer->groupOpacity = true;
            break;
          }
        }
      } else if (nType == NodeType::Group || nType == NodeType::TextBox) {
        auto* group = static_cast<Group*>(targetNode);
        std::string domId = !group->id.empty() ? group->id : StripAtPrefix(obj->target);
        if (domId.empty()) {
          domId = _ctx.nextId("animG");
        }
        _ctx.animatedElementIds[group] = domId;
      } else if (nType == NodeType::SolidColor) {
        auto* solid = static_cast<const SolidColor*>(targetNode);
        auto it = _solidToPainter.find(solid);
        if (it != _solidToPainter.end() && it->second != nullptr) {
          std::string domId = !solid->id.empty() ? solid->id : StripAtPrefix(obj->target);
          if (domId.empty()) {
            domId = _ctx.nextId("animC");
          }
          _ctx.animatedElementIds[it->second] = domId;
        }
      } else if (nType == NodeType::Fill || nType == NodeType::Stroke) {
        std::string domId = !targetNode->id.empty() ? targetNode->id : StripAtPrefix(obj->target);
        if (domId.empty()) {
          domId = _ctx.nextId("animP");
        }
        _ctx.animatedElementIds[targetNode] = domId;
      } else if (nType == NodeType::BlurFilter || nType == NodeType::DropShadowFilter) {
        auto* filter = static_cast<const LayerFilter*>(targetNode);
        auto it = _filterToLayer.find(filter);
        if (it != _filterToLayer.end() && it->second != nullptr) {
          auto* hostLayer = const_cast<Layer*>(it->second);
          if (hostLayer->id.empty()) {
            _restoredNodeIds.emplace_back(hostLayer, hostLayer->id);
            hostLayer->id = _ctx.nextId("animF");
          }
        }
      } else if (nType == NodeType::Path) {
        auto* pathNode = static_cast<const Path*>(targetNode);
        auto maskIt = _maskPathToLayer.find(pathNode);
        if (maskIt != _maskPathToLayer.end() && maskIt->second != nullptr) {
          auto* hostLayer = const_cast<Layer*>(maskIt->second);
          if (hostLayer->id.empty()) {
            _restoredNodeIds.emplace_back(hostLayer, hostLayer->id);
            hostLayer->id = _ctx.nextId("animM");
          }
        } else {
          std::string domId = !pathNode->id.empty() ? pathNode->id : StripAtPrefix(obj->target);
          if (domId.empty()) {
            domId = _ctx.nextId("animPath");
          }
          _ctx.animatedElementIds[pathNode] = domId;
        }
      }
    }
  }
}

HTMLAnimationSession::~HTMLAnimationSession() {
  for (auto& kv : _restoredNodeIds) {
    kv.first->id = kv.second;
  }
  for (auto& kv : _restoredGroupOpacity) {
    kv.first->groupOpacity = kv.second;
  }
  for (auto& kv : _restoredGroupAlpha) {
    kv.first->alpha = kv.second;
  }
  _ctx.animatedElementIds.clear();
}

std::string HTMLAnimationSession::emitKeyframesAndWeave(std::string& bodyHtml) {
  if (!_active) {
    return {};
  }

  std::unordered_map<std::string, int64_t> timelineOffsets = {};
  CollectTimelineOffsets(_doc.layers, timelineOffsets);

  std::string keyframesCss;
  int animCounter = 0;

  std::vector<std::string> domIdOrder = {};
  std::unordered_map<std::string, std::vector<std::string>> domIdAnimDecls = {};
  std::unordered_map<std::string, bool> domIdNeedsZeroOrigin = {};

  for (auto* animNode : _doc.animations) {
    if (animNode == nullptr || animNode->nodeType() != NodeType::Animation) {
      continue;
    }
    auto* anim = static_cast<const Animation*>(animNode);
    if (anim->duration <= 0 || anim->frameRate <= 0.0f || anim->objects.empty()) {
      continue;
    }

    int64_t evalOffsetFrames = 0;
    if (!anim->id.empty()) {
      auto offIt = timelineOffsets.find(anim->id);
      if (offIt != timelineOffsets.end()) {
        evalOffsetFrames = offIt->second;
      }
    }

    // Step 1: Group AnimationObjects by their resolved DOM element id.
    auto bundles = GroupObjectsIntoDomBundles(_doc, anim, _ctx, _solidToPainter, _filterToLayer,
                                              _maskPathToLayer);

    for (const auto& bundle : bundles) {
      size_t tagStart = 0;
      size_t tagEnd = 0;
      std::string tagName;
      if (!FindElementOpeningTagById(bodyHtml, bundle.domId, tagStart, tagEnd, tagName)) {
        continue;
      }

      // Step 2: Classify channels via kChannelBindingTable and collect keyTimes / easings.
      BundleChannelSet chSet = CollectBundleChannels(_doc, bundle, _solidToPainter);
      if (chSet.keyTimes.empty()) {
        continue;
      }

      // Step 3: Compute timing (bundleShiftFrames, effectiveDuration, uniformEasing).
      BundleTimingInfo timing = ComputeBundleTiming(anim, evalOffsetFrames, chSet);
      std::string kfName = MakeSafeKeyframesName(bundle.domId, animCounter++);

      // Step 4: Evaluate channels at each keyframe timestamp and emit @keyframes block.
      bool needsZeroOrigin = false;
      keyframesCss +=
          EmitBundleKeyframesRule(kfName, bundle, chSet, timing, tagName, needsZeroOrigin);

      // Step 5: Build the CSS `animation:` shorthand for this target element.
      std::string shorthand = BuildAnimationShorthand(anim, kfName, timing, evalOffsetFrames);
      if (domIdAnimDecls.find(bundle.domId) == domIdAnimDecls.end()) {
        domIdOrder.push_back(bundle.domId);
      }
      domIdAnimDecls[bundle.domId].push_back(shorthand);
      if (needsZeroOrigin) {
        domIdNeedsZeroOrigin[bundle.domId] = true;
      }
    }
  }

  // Step 6: Weave accumulated `animation:` declarations into the DOM elements in `bodyHtml`.
  for (const auto& domId : domIdOrder) {
    const auto& decls = domIdAnimDecls[domId];
    std::string combined;
    for (size_t i = 0; i < decls.size(); ++i) {
      if (i > 0) {
        combined += ", ";
      }
      combined += decls[i];
    }
    WeaveStyleIntoElementById(bodyHtml, domId, combined, domIdNeedsZeroOrigin[domId]);
  }

  return keyframesCss;
}

}  // namespace pagx
