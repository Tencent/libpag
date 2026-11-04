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
#include <iomanip>
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
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/runtime/MatrixDecompose.h"
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

// Everything the animation export needs from the document tree. Collected by a single traversal:
// the playable animations, the SolidColor and filter owners, the mask paths and the timeline
// offsets all describe the same tree, so walking it once keeps those views consistent.
struct DocumentIndex {
  std::vector<const Animation*> animations = {};
  std::unordered_map<const SolidColor*, const Node*> solidToPainter = {};
  std::unordered_map<const LayerFilter*, const Layer*> filterToLayer = {};
  std::unordered_map<const Path*, Layer*> maskPathToLayer = {};
  std::unordered_map<std::string, int64_t> timelineOffsets = {};
};

// Appends the playable animations among `nodes` that were not collected yet.
void AppendPlayableAnimations(const std::vector<Node*>& nodes, DocumentIndex& index,
                              std::unordered_set<const Animation*>& seen) {
  for (auto* node : nodes) {
    if (node == nullptr || node->nodeType() != NodeType::Animation) {
      continue;
    }
    auto* anim = static_cast<const Animation*>(node);
    if (anim->duration > 0 && anim->frameRate > 0.0f && !anim->objects.empty() &&
        seen.insert(anim).second) {
      index.animations.push_back(anim);
    }
  }
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

void CollectMaskPaths(const Layer* maskLayer, Layer* hostLayer,
                      std::unordered_map<const Path*, Layer*>& maskPathToLayer) {
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

// Collects what a single layer contributes on its own: its element painters, its animation filters,
// the paths of its mask subtree and the offsets of the timelines it mounts.
void IndexLayerContents(Layer* layer, DocumentIndex& index) {
  IndexElements(layer->contents, index.solidToPainter);
  for (auto* filter : layer->filters) {
    if (filter != nullptr) {
      index.filterToLayer[filter] = layer;
    }
  }
  if (layer->mask != nullptr) {
    CollectMaskPaths(layer->mask, layer, index.maskPathToLayer);
  }
  for (const auto& tl : layer->timelines) {
    if (tl != nullptr && tl->timelineType() == TimelineType::Animation) {
      auto* animTl = static_cast<const AnimationTimeline*>(tl.get());
      std::string id = StripAtPrefix(animTl->animationId);
      if (!id.empty() && animTl->evaluationOffset != 0) {
        index.timelineOffsets[id] = animTl->evaluationOffset;
      }
    }
  }
}

// Walks the layer tree once, filling every part of `index`. A Composition is visited only the first
// time it is reached: siblings can mount the same source Composition.
void IndexLayerTree(const std::vector<Layer*>& layers, DocumentIndex& index,
                    std::unordered_set<const Animation*>& seenAnimations,
                    std::unordered_set<const Composition*>& visitedCompositions) {
  for (auto* layer : layers) {
    if (layer == nullptr) {
      continue;
    }
    IndexLayerContents(layer, index);
    if (layer->composition != nullptr && visitedCompositions.insert(layer->composition).second) {
      AppendPlayableAnimations(layer->composition->animations, index, seenAnimations);
      IndexLayerTree(layer->composition->layers, index, seenAnimations, visitedCompositions);
    }
    IndexLayerTree(layer->children, index, seenAnimations, visitedCompositions);
  }
}

// Single entry point for every document view the animation export needs.
DocumentIndex BuildDocumentIndex(const PAGXDocument& doc) {
  DocumentIndex index = {};
  std::unordered_set<const Animation*> seenAnimations = {};
  AppendPlayableAnimations(doc.animations, index, seenAnimations);
  std::unordered_set<const Composition*> visitedCompositions = {};
  IndexLayerTree(doc.layers, index, seenAnimations, visitedCompositions);
  return index;
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
    case ChannelValueType::Matrix3D:
    case ChannelValueType::PAGImage:
      // Neither channel value type currently maps to a CSS animation channel.
      break;
  }
}

bool EvalBoolChannel(const Channel* ch, Frame frame, bool fallback) {
  if (ch == nullptr) {
    return fallback;
  }
  KeyValue value = ch->evaluateAt(frame);
  const auto* result = std::get_if<bool>(&value);
  return result != nullptr ? *result : fallback;
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

// CSS interpolates matrix() by decomposition, but refuses to interpolate when either endpoint is
// singular (e.g. a progress bar whose initial scaleX is zero). Express the same runtime components
// as separate CSS functions so each factor, including a zero scale, interpolates continuously.
std::string FormatDecomposedMatrixCss(const Matrix& matrix) {
  auto components = DecomposeAffine(matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty);
  static constexpr float RadiansToDegrees = 57.29577951308232f;
  return "translate(" + CssFloatToString(components.translateX) + "px, " +
         CssFloatToString(components.translateY) + "px) rotate(" +
         CssFloatToString(components.rotation * RadiansToDegrees) + "deg) skewX(" +
         CssFloatToString(components.skew * RadiansToDegrees) + "deg) scale(" +
         CssFloatToString(components.scaleX) + ", " + CssFloatToString(components.scaleY) + ")";
}

bool HasSingularMatrixKey(const Channel* channel) {
  if (channel == nullptr || channel->valueType() != ChannelValueType::Matrix) {
    return false;
  }
  for (const auto& key : static_cast<const TypedChannel<Matrix>*>(channel)->keyframes) {
    const auto& matrix = key.value;
    if (FloatNearlyZero(matrix.a * matrix.d - matrix.b * matrix.c)) {
      return true;
    }
  }
  return false;
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

// Returns the matrix that maps the mask path's own coordinates into the masked element's space.
// A Path used by a mask lives in the mask layer's subtree, so the chain from the mask root down to
// its parent layer has to be accumulated the same way writeMaskGeometry does.
bool FindMaskPathMatrix(const Layer* layer, const Path* target, const Matrix& parent, Matrix& out) {
  if (layer == nullptr) {
    return false;
  }
  Matrix combined = parent * BuildLayerMatrix(layer);
  for (auto* element : layer->contents) {
    if (element == target) {
      out = combined;
      return true;
    }
  }
  for (auto* child : layer->children) {
    if (FindMaskPathMatrix(child, target, combined, out)) {
      return true;
    }
  }
  if (layer->composition != nullptr) {
    for (auto* compLayer : layer->composition->layers) {
      if (FindMaskPathMatrix(compLayer, target, combined, out)) {
        return true;
      }
    }
  }
  return false;
}

std::string RebuildAnimatedPathSVGString(const Path* pathNode,
                                         const std::unordered_map<size_t, const Channel*>& xChs,
                                         const std::unordered_map<size_t, const Channel*>& yChs,
                                         Frame frame, const Matrix& transform = {}) {
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
  if (!transform.isIdentity()) {
    for (auto& pt : pts) {
      pt = transform.mapPoint(pt);
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

// Returns the start of the innermost `<svg` opening tag that encloses `pos`, or npos when `pos` is
// not inside an <svg>. Scans backwards, skipping sibling <svg> elements that close before `pos`.
size_t FindEnclosingSvgStart(const std::string& html, size_t pos) {
  int closedSiblings = 0;
  size_t cursor = pos;
  while (cursor > 0) {
    size_t openPos = html.rfind("<svg", cursor - 1);
    size_t closePos = html.rfind("</svg>", cursor - 1);
    if (openPos == std::string::npos) {
      return std::string::npos;
    }
    if (closePos != std::string::npos && closePos > openPos) {
      closedSiblings++;
      cursor = closePos;
      continue;
    }
    if (closedSiblings == 0) {
      return openPos;
    }
    closedSiblings--;
    cursor = openPos;
  }
  return std::string::npos;
}

// Builds the opening tag text with the animation declarations added to its style attribute. A tag
// without a style attribute (a bare SVG <path/>) gets one inserted before its closing bracket.
std::string BuildWovenTag(const std::string& tagStr, const std::string& animDecl,
                          bool needsZeroTransformOrigin) {
  std::string woven = tagStr;
  size_t stylePos = woven.find(" style=\"");
  if (stylePos != std::string::npos) {
    size_t valStart = stylePos + 8;
    size_t valEnd = woven.find('"', valStart);
    if (valEnd != std::string::npos) {
      std::string styleVal = woven.substr(valStart, valEnd - valStart);
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
      woven.replace(valStart, valEnd - valStart, styleVal);
      return woven;
    }
  }
  std::string newStyle = " style=\"";
  if (needsZeroTransformOrigin) {
    newStyle += "transform-origin:0 0;";
  }
  newStyle += "animation:" + animDecl + "\"";
  size_t insertPos = woven.size() - 1;
  if (insertPos > 0 && woven[insertPos - 1] == '/') {
    insertPos -= 1;
  }
  woven.insert(insertPos, newStyle);
  return woven;
}

// Builds the <svg ...> opening tag with clipping lifted, or returns an empty string when the tag
// already exposes clipping or has no style attribute to extend.
std::string BuildLiftedSvgTag(const std::string& svgTag) {
  size_t stylePos = svgTag.find(" style=\"");
  if (stylePos == std::string::npos) {
    std::string lifted = svgTag;
    size_t insertPos = lifted.size() - 1;
    lifted.insert(insertPos, " style=\"overflow:visible\"");
    return lifted;
  }
  size_t valStart = stylePos + 8;
  size_t valEnd = svgTag.find('"', valStart);
  if (valEnd == std::string::npos) {
    return {};
  }
  std::string styleVal = svgTag.substr(valStart, valEnd - valStart);
  if (styleVal.find("overflow:") != std::string::npos) {
    return {};
  }
  if (!styleVal.empty() && styleVal.back() != ';') {
    styleVal += ';';
  }
  styleVal += "overflow:visible";
  std::string lifted = svgTag;
  lifted.replace(valStart, valEnd - valStart, styleVal);
  return lifted;
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
  LayerVisibility,
  GeometryWidth,
  RectangleRoundness,
  TrimEnd,
  GroupPosX,
  GroupPosY,
  Rotation,
  ScaleX,
  ScaleY,
  AnchorX,
  AnchorY,
  Skew,
  SkewAxis,
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
    {NodeType::Layer, "visible", TargetChannelRole::LayerVisibility},
    {NodeType::Layer, "rotation", TargetChannelRole::Rotation},
    {NodeType::Layer, "scale.x", TargetChannelRole::ScaleX},
    {NodeType::Layer, "scale.y", TargetChannelRole::ScaleY},

    // Shape geometry channels
    {NodeType::Rectangle, "size.width", TargetChannelRole::GeometryWidth},
    {NodeType::Rectangle, "roundness", TargetChannelRole::RectangleRoundness},
    {NodeType::Ellipse, "size.width", TargetChannelRole::GeometryWidth},
    {NodeType::TrimPath, "end", TargetChannelRole::TrimEnd},

    // Group & TextBox channels
    {NodeType::Group, "alpha", TargetChannelRole::Opacity},
    {NodeType::Group, "position.x", TargetChannelRole::GroupPosX},
    {NodeType::Group, "position.y", TargetChannelRole::GroupPosY},
    {NodeType::Group, "rotation", TargetChannelRole::Rotation},
    {NodeType::Group, "scale.x", TargetChannelRole::ScaleX},
    {NodeType::Group, "scale.y", TargetChannelRole::ScaleY},
    {NodeType::Group, "anchor.x", TargetChannelRole::AnchorX},
    {NodeType::Group, "anchor.y", TargetChannelRole::AnchorY},
    {NodeType::Group, "skew", TargetChannelRole::Skew},
    {NodeType::Group, "skewAxis", TargetChannelRole::SkewAxis},
    {NodeType::TextBox, "alpha", TargetChannelRole::Opacity},
    {NodeType::TextBox, "position.x", TargetChannelRole::GroupPosX},
    {NodeType::TextBox, "position.y", TargetChannelRole::GroupPosY},
    {NodeType::TextBox, "rotation", TargetChannelRole::Rotation},
    {NodeType::TextBox, "scale.x", TargetChannelRole::ScaleX},
    {NodeType::TextBox, "scale.y", TargetChannelRole::ScaleY},
    {NodeType::TextBox, "anchor.x", TargetChannelRole::AnchorX},
    {NodeType::TextBox, "anchor.y", TargetChannelRole::AnchorY},
    {NodeType::TextBox, "skew", TargetChannelRole::Skew},
    {NodeType::TextBox, "skewAxis", TargetChannelRole::SkewAxis},

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
  const Channel* visibleCh = nullptr;
  const Channel* geometryWidthCh = nullptr;
  const Channel* roundnessCh = nullptr;
  const Channel* trimEndCh = nullptr;
  const TrimPath* trimTarget = nullptr;
  const Channel* rotCh = nullptr;
  const Channel* scaleXCh = nullptr;
  const Channel* scaleYCh = nullptr;
  const Channel* anchorXCh = nullptr;
  const Channel* anchorYCh = nullptr;
  const Channel* skewCh = nullptr;
  const Channel* skewAxisCh = nullptr;
  const Channel* posXCh = nullptr;
  const Channel* posYCh = nullptr;
  const Channel* fillColorCh = nullptr;
  const Channel* fillAlphaCh = nullptr;
  const Channel* strokeColorCh = nullptr;
  const Channel* strokeAlphaCh = nullptr;
  const Channel* strokeWidthCh = nullptr;
  const Channel* dashOffsetCh = nullptr;
  const Fill* fillPainter = nullptr;
  const Stroke* strokePainter = nullptr;

  std::unordered_map<const LayerFilter*, FilterChannelSlot> filterChMap = {};
  bool hasFilterAnim = false;

  const Path* animatedMaskPath = nullptr;
  std::unordered_map<size_t, const Channel*> maskPtX = {};
  std::unordered_map<size_t, const Channel*> maskPtY = {};
  // Mask layer transform. A clip-path carries no matrix of its own, so the mask geometry has to be
  // baked into the generated coordinates.
  Matrix maskMatrix = {};

  std::set<Frame> keyTimes = {};
  std::unordered_map<Frame, SegmentEasing> stopEasings = {};
};

struct BundleTimingInfo {
  Frame bundleShiftFrames = 0;
  Frame effectiveDuration = 0;
  bool uniformEasing = true;
  SegmentEasing commonEasing = {};
};

// Binds `ch` to the CSS-mapped slot it drives and returns true, or returns false when the exporter
// has no CSS mapping for it (an unknown channel, a SolidColor no painter uses, or a malformed Path
// point channel). Only bound channels may contribute keyframe stops and easing to the bundle.
bool BindChannelToBundleSet(
    const Node* objTarget, const Channel* ch,
    const std::unordered_map<const SolidColor*, const Node*>& solidToPainter,
    BundleChannelSet& out) {
  auto tType = objTarget->nodeType();
  if (tType == NodeType::Path) {
    size_t ptIdx = 0;
    bool isX = false;
    if (!ParsePointChannelName(ch->name, ptIdx, isX)) {
      return false;
    }
    out.animatedMaskPath = static_cast<const Path*>(objTarget);
    if (isX) {
      out.maskPtX[ptIdx] = ch;
    } else {
      out.maskPtY[ptIdx] = ch;
    }
    return true;
  }

  TargetChannelRole role = LookupChannelRole(tType, ch->name);
  switch (role) {
    case TargetChannelRole::Opacity:
      out.alphaCh = ch;
      return true;
    case TargetChannelRole::LayerX:
      out.xCh = ch;
      return true;
    case TargetChannelRole::LayerY:
      out.yCh = ch;
      return true;
    case TargetChannelRole::LayerMatrix:
      out.matrixCh = ch;
      return true;
    case TargetChannelRole::LayerVisibility:
      if (ch->valueType() != ChannelValueType::Bool) {
        return false;
      }
      out.visibleCh = ch;
      return true;
    case TargetChannelRole::GeometryWidth:
      out.geometryWidthCh = ch;
      return true;
    case TargetChannelRole::RectangleRoundness:
      out.roundnessCh = ch;
      return true;
    case TargetChannelRole::TrimEnd: {
      auto* trim = static_cast<const TrimPath*>(objTarget);
      // A CSS dash animation models a separate trim on a single path. Multi-path continuous
      // trims redistribute length across paths and require different per-path stop values.
      if (trim->type != TrimType::Separate || trim->start != 0.0f || trim->offset != 0.0f) {
        return false;
      }
      out.trimEndCh = ch;
      out.trimTarget = trim;
      return true;
    }
    case TargetChannelRole::GroupPosX:
      out.posXCh = ch;
      return true;
    case TargetChannelRole::GroupPosY:
      out.posYCh = ch;
      return true;
    case TargetChannelRole::Rotation:
      out.rotCh = ch;
      return true;
    case TargetChannelRole::ScaleX:
      out.scaleXCh = ch;
      return true;
    case TargetChannelRole::ScaleY:
      out.scaleYCh = ch;
      return true;
    case TargetChannelRole::AnchorX:
      out.anchorXCh = ch;
      return true;
    case TargetChannelRole::AnchorY:
      out.anchorYCh = ch;
      return true;
    case TargetChannelRole::Skew:
      out.skewCh = ch;
      return true;
    case TargetChannelRole::SkewAxis:
      out.skewAxisCh = ch;
      return true;
    case TargetChannelRole::FillColor:
      out.fillColorCh = ch;
      out.fillPainter = static_cast<const Fill*>(objTarget);
      return true;
    case TargetChannelRole::FillAlpha:
      out.fillAlphaCh = ch;
      out.fillPainter = static_cast<const Fill*>(objTarget);
      return true;
    case TargetChannelRole::StrokeColor:
      out.strokeColorCh = ch;
      out.strokePainter = static_cast<const Stroke*>(objTarget);
      return true;
    case TargetChannelRole::StrokeAlpha:
      out.strokeAlphaCh = ch;
      out.strokePainter = static_cast<const Stroke*>(objTarget);
      return true;
    case TargetChannelRole::StrokeWidth:
      out.strokeWidthCh = ch;
      return true;
    case TargetChannelRole::StrokeDashOffset:
      out.dashOffsetCh = ch;
      return true;
    case TargetChannelRole::SolidColorValue:
    case TargetChannelRole::SolidColorAlpha: {
      auto sIt = solidToPainter.find(static_cast<const SolidColor*>(objTarget));
      if (sIt == solidToPainter.end() || sIt->second == nullptr) {
        return false;
      }
      if (sIt->second->nodeType() == NodeType::Fill) {
        out.fillPainter = static_cast<const Fill*>(sIt->second);
        if (role == TargetChannelRole::SolidColorValue) {
          out.fillColorCh = ch;
        } else {
          out.fillAlphaCh = ch;
        }
        return true;
      }
      if (sIt->second->nodeType() == NodeType::Stroke) {
        out.strokePainter = static_cast<const Stroke*>(sIt->second);
        if (role == TargetChannelRole::SolidColorValue) {
          out.strokeColorCh = ch;
        } else {
          out.strokeAlphaCh = ch;
        }
        return true;
      }
      return false;
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
      } else {
        fc.alpha = ch;
      }
      return true;
    }
    case TargetChannelRole::Unknown:
      return false;
  }
  return false;
}

BundleChannelSet CollectBundleChannels(
    PAGXDocument& doc, const DomTargetBundle& bundle,
    const std::unordered_map<const SolidColor*, const Node*>& solidToPainter,
    const std::unordered_map<const Path*, Layer*>& maskPathToLayer) {
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
      // An unbound channel has no CSS property to drive, so its keys must not split the bound
      // channels' keyframes or override their easing.
      if (BindChannelToBundleSet(objTarget, ch, solidToPainter, result)) {
        ExtractChannelTimesAndEasing(ch, result.keyTimes, result.stopEasings);
      }
    }
  }
  if (result.animatedMaskPath != nullptr) {
    auto maskIt = maskPathToLayer.find(result.animatedMaskPath);
    if (maskIt != maskPathToLayer.end() && maskIt->second != nullptr &&
        maskIt->second->mask != nullptr) {
      Matrix maskMatrix = {};
      if (FindMaskPathMatrix(maskIt->second->mask, result.animatedMaskPath, {}, maskMatrix)) {
        result.maskMatrix = maskMatrix;
      }
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
  }
  // A key past the duration is only an interpolation target: the runtime never advances beyond
  // anim->duration, so entering the window at that boundary already yields the interpolated value.
  // Extending the CSS duration to reach such a key would both slow the animation down and change
  // the loop period, so the window stays anim->duration long and the key itself is dropped.

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
  if (chSet.matrixCh != nullptr || chSet.xCh != nullptr || chSet.yCh != nullptr) {
    const Layer* hostLayer = nullptr;
    if (bundle.primaryHost != nullptr && bundle.primaryHost->nodeType() == NodeType::Layer) {
      hostLayer = static_cast<const Layer*>(bundle.primaryHost);
    }
    // The runtime composes the layer transform as Translate(x, y) * matrix (see
    // LayerRuntimeTarget::recompose). A keyframe `transform` replaces the element's inline transform
    // instead of stacking on it, so every frame re-emits the full chain: the animated translation
    // relative to the static left/top first, then the matrix part, which is the animated matrix
    // channel when present and otherwise the layer's static transform as written inline.
    std::string tf = {};
    if (chSet.xCh != nullptr || chSet.yCh != nullptr) {
      Point basePos = hostLayer != nullptr ? hostLayer->renderPosition() : Point{0.0f, 0.0f};
      if (hostLayer != nullptr && chSet.matrixCh == nullptr && hostLayer->matrix.isIdentity() &&
          hostLayer->matrix3D.isIdentity()) {
        // With layout constraints left/top are the layout slot, while LayerTransformCSS already
        // contains the authored x/y. Subtracting renderPosition here would subtract that slot a
        // second time; only the change relative to the authored value belongs in this translate.
        auto layoutPos = hostLayer->layoutPosition();
        if (!std::isnan(layoutPos.x)) {
          basePos.x = hostLayer->x;
        }
        if (!std::isnan(layoutPos.y)) {
          basePos.y = hostLayer->y;
        }
      }
      float dx = EvalFloatChannel(chSet.xCh, t, basePos.x) - basePos.x;
      float dy = EvalFloatChannel(chSet.yCh, t, basePos.y) - basePos.y;
      tf = "translate(" + CssFloatToString(dx) + "px, " + CssFloatToString(dy) + "px)";
    }
    std::string matrixPart = {};
    if (chSet.matrixCh != nullptr) {
      Matrix staticMatrix = hostLayer != nullptr ? hostLayer->matrix : Matrix::Identity();
      auto matrix = EvalMatrixChannel(chSet.matrixCh, t, staticMatrix);
      matrixPart = HasSingularMatrixKey(chSet.matrixCh) ? FormatDecomposedMatrixCss(matrix)
                                                        : FormatMatrixCss(matrix);
    } else if (hostLayer != nullptr) {
      matrixPart = LayerTransformCSS(hostLayer);
    }
    if (!matrixPart.empty()) {
      if (!tf.empty()) {
        tf += ' ';
      }
      tf += matrixPart;
      needsZeroOrigin = true;
    }
    AppendCssDecl(decls, "transform", tf);
    return;
  }
  if (chSet.rotCh != nullptr || chSet.scaleXCh != nullptr || chSet.scaleYCh != nullptr ||
      chSet.posXCh != nullptr || chSet.posYCh != nullptr || chSet.anchorXCh != nullptr ||
      chSet.anchorYCh != nullptr || chSet.skewCh != nullptr || chSet.skewAxisCh != nullptr) {
    float basePosX = 0.0f;
    float basePosY = 0.0f;
    float baseRot = 0.0f;
    float baseSX = 1.0f;
    float baseSY = 1.0f;
    float baseSkew = 0.0f;
    float baseSkewAxis = 0.0f;
    Point baseAnchor = {0.0f, 0.0f};
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
      baseSkew = grp->skew;
      baseSkewAxis = grp->skewAxis;
      baseAnchor = grp->anchor;
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
    float skew = EvalFloatChannel(chSet.skewCh, t, baseSkew);
    float skewAxis = EvalFloatChannel(chSet.skewAxisCh, t, baseSkewAxis);
    float anchorX = EvalFloatChannel(chSet.anchorXCh, t, baseAnchor.x);
    float anchorY = EvalFloatChannel(chSet.anchorYCh, t, baseAnchor.y);

    // Mirrors BuildGroupMatrix: T(position) * R(rotation) * R(-skewAxis) * Shear(tan(-skew)) *
    // R(skewAxis) * S(scale) * T(-anchor). Every factor gets its own transform function so the
    // browser interpolates each one independently, as the runtime does. Whether a factor is
    // emitted depends only on its channel or static value, so all stops share one function list.
    std::string tf;
    auto appendFunction = [&tf](const std::string& function) {
      if (!tf.empty()) {
        tf += ' ';
      }
      tf += function;
    };
    if (!FloatNearlyZero(px) || !FloatNearlyZero(py) || chSet.posXCh != nullptr ||
        chSet.posYCh != nullptr) {
      appendFunction("translate(" + CssFloatToString(px) + "px, " + CssFloatToString(py) + "px)");
    }
    if (chSet.rotCh != nullptr || !FloatNearlyZero(rot)) {
      appendFunction("rotate(" + CssFloatToString(rot) + "deg)");
    }
    // A zero skew is an identity shear, so the skew axis only matters while there is a skew.
    if (chSet.skewCh != nullptr || !FloatNearlyZero(baseSkew)) {
      bool hasSkewAxis = chSet.skewAxisCh != nullptr || !FloatNearlyZero(baseSkewAxis);
      if (hasSkewAxis) {
        appendFunction("rotate(" + CssFloatToString(-skewAxis) + "deg)");
      }
      appendFunction("skewX(" + CssFloatToString(-skew) + "deg)");
      if (hasSkewAxis) {
        appendFunction("rotate(" + CssFloatToString(skewAxis) + "deg)");
      }
    }
    if (chSet.scaleXCh != nullptr || chSet.scaleYCh != nullptr || !FloatNearlyZero(sx - 1.0f) ||
        !FloatNearlyZero(sy - 1.0f)) {
      if (FloatNearlyZero(sx - sy)) {
        appendFunction("scale(" + CssFloatToString(sx) + ")");
      } else {
        appendFunction("scale(" + CssFloatToString(sx) + ", " + CssFloatToString(sy) + ")");
      }
    }
    if (chSet.anchorXCh != nullptr || chSet.anchorYCh != nullptr ||
        !FloatNearlyZero(baseAnchor.x) || !FloatNearlyZero(baseAnchor.y)) {
      appendFunction("translate(" + CssFloatToString(-anchorX) + "px, " +
                     CssFloatToString(-anchorY) + "px)");
    }
    if (tf.empty()) {
      tf = "rotate(0deg)";
    }
    AppendCssDecl(decls, "transform", tf);
    needsZeroOrigin = true;
  }
}

// Returns the solid color a painter shows when its color is not animated, or opaque black when the
// painter has no solid color (gradient and image painters are not color-animated).
Color StaticPainterColor(const ColorSource* source) {
  if (source != nullptr && source->nodeType() == NodeType::SolidColor) {
    return static_cast<const SolidColor*>(source)->color;
  }
  return Color{};
}

// A gradient or image painter paints from a URL instead of a solid color. Re-emitting a solid color
// in a keyframe stop would replace that paint, so only the alpha may be animated for such painters.
bool HasSolidPaint(const ColorSource* source) {
  return source != nullptr && source->nodeType() == NodeType::SolidColor;
}

void EmitPaintCssAtFrame(const BundleChannelSet& chSet, const std::string& tagName, Frame t,
                         std::string& decls) {
  bool isSvgShape =
      (tagName == "path" || tagName == "rect" || tagName == "ellipse" || tagName == "use");
  // PAGX paints with the color's own alpha multiplied by the painter alpha. A keyframe stop
  // replaces the element's static paint, so each stop re-emits that product, taking the static
  // value for whichever factor is not animated. The product is folded into the paint color rather
  // than element `opacity`, which would also fade the element's children and text stroke and
  // collide with an animated Layer alpha.
  if (chSet.fillColorCh != nullptr || chSet.fillAlphaCh != nullptr) {
    const Fill* fill = chSet.fillPainter;
    const ColorSource* source = fill != nullptr ? fill->color : nullptr;
    float painterAlpha =
        EvalFloatChannel(chSet.fillAlphaCh, t, fill != nullptr ? fill->alpha : 1.0f);
    if (HasSolidPaint(source)) {
      Color color = EvalColorChannel(chSet.fillColorCh, t, StaticPainterColor(source));
      if (isSvgShape) {
        AppendCssDecl(decls, "fill", ColorToSVGHex(color));
        AppendCssDecl(decls, "fill-opacity", CssFloatToString(color.alpha * painterAlpha));
      } else {
        AppendCssDecl(decls, tagName == "span" ? "color" : "background-color",
                      ColorToRGBA(color, painterAlpha));
      }
    } else if (isSvgShape) {
      // Keep fill="url(#…)" and fade the referenced paint server instead of replacing it.
      AppendCssDecl(decls, "fill-opacity", CssFloatToString(painterAlpha));
    } else {
      // A CSS box paints the gradient as its own background, so a background-color would only add a
      // black layer underneath it. The box is a leaf that paints nothing else, so fading the box as
      // a whole matches the painter alpha.
      AppendCssDecl(decls, "opacity", CssFloatToString(painterAlpha));
    }
  }
  if (chSet.strokeColorCh != nullptr || chSet.strokeAlphaCh != nullptr) {
    const Stroke* stroke = chSet.strokePainter;
    const ColorSource* source = stroke != nullptr ? stroke->color : nullptr;
    float painterAlpha =
        EvalFloatChannel(chSet.strokeAlphaCh, t, stroke != nullptr ? stroke->alpha : 1.0f);
    if (HasSolidPaint(source)) {
      Color color = EvalColorChannel(chSet.strokeColorCh, t, StaticPainterColor(source));
      AppendCssDecl(decls, "stroke", ColorToSVGHex(color));
      AppendCssDecl(decls, "stroke-opacity", CssFloatToString(color.alpha * painterAlpha));
    } else {
      AppendCssDecl(decls, "stroke-opacity", CssFloatToString(painterAlpha));
    }
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
      // CSS blur() takes a single radius applied to both axes, so differing blurX and blurY can
      // only be approximated here by the larger one. A per-axis blur needs an SVG feGaussianBlur,
      // whose stdDeviation is an attribute that CSS keyframes cannot animate.
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

void EmitGeometryCssAtFrame(const DomTargetBundle& bundle, const BundleChannelSet& chSet,
                            const std::string& tagName, Frame t, std::string& decls) {
  if (bundle.primaryHost == nullptr) {
    return;
  }
  auto type = bundle.primaryHost->nodeType();
  if (type != NodeType::Rectangle && type != NodeType::Ellipse) {
    return;
  }
  Point center = {};
  float staticWidth = 0.0f;
  if (type == NodeType::Rectangle) {
    auto* rect = static_cast<const Rectangle*>(bundle.primaryHost);
    center = rect->renderPosition();
    staticWidth = rect->renderSize().width;
    if (chSet.roundnessCh != nullptr) {
      float roundness = EvalFloatChannel(chSet.roundnessCh, t, rect->roundness);
      if (tagName == "rect") {
        AppendCssDecl(decls, "rx", CssFloatToString(roundness) + "px");
        AppendCssDecl(decls, "ry", CssFloatToString(roundness) + "px");
      } else {
        AppendCssDecl(decls, "border-radius", CssFloatToString(roundness) + "px");
      }
    }
  } else {
    auto* ellipse = static_cast<const Ellipse*>(bundle.primaryHost);
    center = ellipse->renderPosition();
    staticWidth = ellipse->renderSize().width;
  }
  if (chSet.geometryWidthCh != nullptr) {
    float width = EvalFloatChannel(chSet.geometryWidthCh, t, staticWidth);
    if (tagName == "ellipse") {
      AppendCssDecl(decls, "rx", CssFloatToString(width * 0.5f) + "px");
    } else {
      // PAGX sizes shapes around their center. The CSS div/rect has a top-left origin, so the
      // left/x coordinate must move by half of the width delta to keep that center fixed.
      AppendCssDecl(decls, tagName == "rect" ? "x" : "left",
                    CssFloatToString(center.x - width * 0.5f) + "px");
      AppendCssDecl(decls, "width", CssFloatToString(width) + "px");
    }
  }
}

std::string EvaluateKeyframeCssDecls(const DomTargetBundle& bundle, const BundleChannelSet& chSet,
                                     const BundleTimingInfo& timing, const std::string& tagName,
                                     Frame t, bool& needsZeroOrigin,
                                     const float* alphaOverride = nullptr) {
  std::string decls;
  if (chSet.alphaCh != nullptr) {
    float a = alphaOverride != nullptr ? *alphaOverride : EvalFloatChannel(chSet.alphaCh, t, 1.0f);
    AppendCssDecl(decls, "opacity", CssFloatToString(a));
  }
  if (chSet.visibleCh != nullptr && bundle.primaryHost != nullptr &&
      bundle.primaryHost->nodeType() == NodeType::Layer) {
    bool visible =
        EvalBoolChannel(chSet.visibleCh, t, static_cast<const Layer*>(bundle.primaryHost)->visible);
    AppendCssDecl(decls, "visibility", visible ? "visible" : "hidden");
  }
  EmitTransformCssAtFrame(bundle, chSet, t, decls, needsZeroOrigin);
  EmitGeometryCssAtFrame(bundle, chSet, tagName, t, decls);
  if (chSet.trimEndCh != nullptr && chSet.trimTarget != nullptr) {
    float end = EvalFloatChannel(chSet.trimEndCh, t, chSet.trimTarget->end);
    float visibleLength = std::clamp(end, 0.0f, 1.0f);
    AppendCssDecl(decls, "stroke-dasharray",
                  CssFloatToString(visibleLength) + " " + CssFloatToString(1.0f - visibleLength));
    // A zero-length dash still paints a dot with round caps. CSS visibility stays visible until
    // the end of a visible-to-hidden transition, so only the empty end frame is suppressed.
    AppendCssDecl(decls, "visibility", visibleLength == 0.0f ? "hidden" : "visible");
  }
  EmitPaintCssAtFrame(chSet, tagName, t, decls);
  EmitFilterCssAtFrame(bundle, chSet, t, decls);

  if (chSet.animatedMaskPath != nullptr) {
    std::string dStr = RebuildAnimatedPathSVGString(chSet.animatedMaskPath, chSet.maskPtX,
                                                    chSet.maskPtY, t, chSet.maskMatrix);
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
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
          c == '-')) {
      c = '_';
    }
  }
  return kfName;
}

std::string BuildAnimationShorthand(const Animation* anim, const std::string& kfName,
                                    const BundleTimingInfo& timing, int64_t evalOffsetFrames,
                                    bool compositionMount = false) {
  float durationSec = static_cast<float>(timing.effectiveDuration) / anim->frameRate;
  std::string timingStr = timing.uniformEasing ? timing.commonEasing.toCss() : "linear";
  std::string shorthand = kfName + " " + CssFloatToString(durationSec) + "s " + timingStr;

  int64_t totalDelayFrames = evalOffsetFrames + static_cast<int64_t>(timing.bundleShiftFrames);
  if (compositionMount && anim->loop != LoopMode::Once) {
    // PAGX folds (elapsed - offset) into the playback period from t=0, even for positive
    // offsets. A negative CSS delay starts at that same phase instead of freezing during a
    // positive CSS delay. PingPong's period includes the backward leg.
    int64_t period =
        static_cast<int64_t>(anim->duration) * (anim->loop == LoopMode::PingPong ? 2 : 1);
    if (period > 0) {
      int64_t offset = totalDelayFrames % period;
      int64_t phase = ((period - offset) % period + period) % period;
      totalDelayFrames = -phase;
    }
  }
  if (totalDelayFrames != 0) {
    float delaySec = static_cast<float>(totalDelayFrames) / anim->frameRate;
    shorthand += " " + CssFloatToString(delaySec) + "s";
  }

  if (anim->loop == LoopMode::Loop) {
    shorthand += (timing.bundleShiftFrames > 0) ? " infinite both" : " infinite";
  } else if (anim->loop == LoopMode::PingPong) {
    shorthand +=
        (timing.bundleShiftFrames > 0) ? " infinite alternate both" : " infinite alternate";
  } else {
    // PAGX shows the first key value while the evaluationOffset delay elapses, which maps to the
    // backwards fill; the forwards fill keeps the last value after playback ends.
    shorthand += " 1 both";
  }
  return shorthand;
}

std::vector<DomTargetBundle> GroupObjectsIntoDomBundles(
    PAGXDocument& doc, const Animation* anim, const HTMLWriterContext& ctx,
    const std::unordered_map<const SolidColor*, const Node*>& solidToPainter,
    const std::unordered_map<const LayerFilter*, const Layer*>& filterToLayer,
    const std::unordered_map<const Path*, Layer*>& maskPathToLayer) {
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
               nType == NodeType::Stroke || nType == NodeType::Rectangle ||
               nType == NodeType::Ellipse) {
      auto it = ctx.animation.elementIds.find(targetNode);
      if (it != ctx.animation.elementIds.end()) {
        domId = it->second;
      }
    } else if (nType == NodeType::SolidColor) {
      auto sIt = solidToPainter.find(static_cast<const SolidColor*>(targetNode));
      if (sIt != solidToPainter.end() && sIt->second != nullptr) {
        primaryHost = sIt->second;
        auto idIt = ctx.animation.elementIds.find(primaryHost);
        if (idIt != ctx.animation.elementIds.end()) {
          domId = idIt->second;
        }
      }
    } else if (nType == NodeType::TrimPath) {
      auto idIt = ctx.animation.elementIds.find(targetNode);
      if (idIt != ctx.animation.elementIds.end()) {
        domId = idIt->second;
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
        auto idIt = ctx.animation.elementIds.find(pathNode);
        if (idIt != ctx.animation.elementIds.end()) {
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

std::string EmitBundleKeyframeStops(const DomTargetBundle& bundle, const BundleChannelSet& chSet,
                                    const BundleTimingInfo& timing, const std::string& tagName,
                                    bool& outNeedsZeroOrigin) {
  std::string stops = {};
  // PAGX holds the first/last key value outside the key range, whereas CSS synthesizes missing
  // 0%/100% stops from the element's static style. Always emit both boundary stops so the browser
  // holds the key values instead of interpolating from the static style.
  // Keys outside the playback window cannot become CSS stops: a percentage past 100% is invalid,
  // and a key before the window start is unreachable. The boundary stops below already carry the
  // value the runtime shows there, interpolated toward those keys.
  const Frame windowStart = timing.bundleShiftFrames;
  const Frame windowEnd = timing.bundleShiftFrames + timing.effectiveDuration;
  std::set<Frame> stopTimes = {};
  for (Frame t : chSet.keyTimes) {
    if (t >= windowStart && t <= windowEnd) {
      stopTimes.insert(t);
    }
  }
  stopTimes.insert(windowStart);
  stopTimes.insert(windowEnd);
  for (Frame t : stopTimes) {
    if (chSet.alphaCh != nullptr && chSet.alphaCh->valueType() == ChannelValueType::Float &&
        t > timing.bundleShiftFrames && timing.effectiveDuration > 0) {
      const auto& keys = static_cast<const TypedChannel<float>*>(chSet.alphaCh)->keyframes;
      auto first =
          std::find_if(keys.begin(), keys.end(), [t](const auto& key) { return key.time == t; });
      if (first != keys.end()) {
        auto last = first;
        while (last != keys.end() && last->time == t) {
          ++last;
        }
        if (last - first > 1 && !FloatNearlyZero(first->value - (last - 1)->value)) {
          // PAGX uses the first duplicate as the value approached from the left and the last
          // duplicate at t. CSS merges equal-offset stops, so insert one just before t: preserve
          // the preceding ramp, then make the tiny final interval a discrete jump.
          auto previousTime = *std::prev(stopTimes.find(t));
          double delta = std::min(0.01, static_cast<double>(t - previousTime) * 0.25);
          double percent = (static_cast<double>(t - timing.bundleShiftFrames) - delta) * 100.0 /
                           static_cast<double>(timing.effectiveDuration);
          std::ostringstream offset;
          offset << std::fixed << std::setprecision(8) << percent << '%';
          float beforeJump = first->value;
          auto beforeDecls = EvaluateKeyframeCssDecls(bundle, chSet, timing, tagName, t,
                                                      outNeedsZeroOrigin, &beforeJump);
          AppendCssDecl(beforeDecls, "animation-timing-function", "steps(1, end)");
          stops += "  " + offset.str() + " { " + beforeDecls + "; }\n";
        }
      }
    }
    std::string decls =
        EvaluateKeyframeCssDecls(bundle, chSet, timing, tagName, t, outNeedsZeroOrigin);
    Frame relTime = (t >= timing.bundleShiftFrames) ? (t - timing.bundleShiftFrames) : 0;
    stops += "  " + FormatPercent(relTime, timing.effectiveDuration) + " { " + decls + "; }\n";
  }
  return stops;
}

struct BundleElement {
  std::string id = {};
  std::string tagName = {};
};

bool IsHtmlSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Reads the id attribute of the opening tag spanning [`tagStart`, `tagEnd`), starting the attribute
// search at `nameEnd` so the tag name itself is never matched.
std::string ReadTagId(const std::string& html, size_t tagStart, size_t tagEnd, size_t nameEnd) {
  size_t searchFrom = nameEnd;
  while (searchFrom < tagEnd) {
    auto idPos = html.find("id=\"", searchFrom);
    if (idPos == std::string::npos || idPos >= tagEnd) {
      return {};
    }
    // A standalone attribute is preceded by whitespace; `data-id="..."` and text content are not.
    if (idPos > tagStart && IsHtmlSpace(html[idPos - 1])) {
      auto idEnd = html.find('"', idPos + 4);
      if (idEnd != std::string::npos && idEnd < tagEnd) {
        return html.substr(idPos + 4, idEnd - idPos - 4);
      }
      return {};
    }
    searchFrom = idPos + 4;
  }
  return {};
}

// Every id emitted into the body, resolved once: its tag name and the byte range of its opening tag.
// Looking an id up with a fresh scan of the whole body costs O(targets x bodyLength) on documents
// with many animated elements, and matching raw text mistakes a literal `id="..."` in content for an
// element, so the scan walks tags instead.
struct BodyTagIndex {
  std::unordered_map<std::string, std::string> nameById = {};
  std::unordered_map<std::string, std::pair<size_t, size_t>> rangeById = {};
};

BodyTagIndex BuildTagIndex(const std::string& html) {
  BodyTagIndex index = {};
  size_t pos = 0;
  while ((pos = html.find('<', pos)) != std::string::npos) {
    auto tagEnd = html.find('>', pos);
    if (tagEnd == std::string::npos) {
      break;
    }
    // Closing tags, comments and declarations carry no attributes.
    if (pos + 1 >= html.size() || html[pos + 1] == '/' || html[pos + 1] == '!' ||
        html[pos + 1] == '?') {
      pos = tagEnd + 1;
      continue;
    }
    size_t nameStart = pos + 1;
    size_t nameEnd = nameStart;
    while (nameEnd < tagEnd && !IsHtmlSpace(html[nameEnd]) && html[nameEnd] != '/' &&
           html[nameEnd] != '>') {
      nameEnd++;
    }
    std::string tagName = html.substr(nameStart, nameEnd - nameStart);
    for (char& c : tagName) {
      if (c >= 'A' && c <= 'Z') {
        c = static_cast<char>(c - 'A' + 'a');
      }
    }
    std::string id = ReadTagId(html, pos, tagEnd, nameEnd);
    if (!id.empty()) {
      index.nameById.emplace(id, std::move(tagName));
      index.rangeById.emplace(id, std::make_pair(pos, tagEnd));
    }
    pos = tagEnd + 1;
  }
  return index;
}

// Lists every tag rendering the bundle's target: the base id plus the "<base>__N" copies that
// HTMLWriterContext::claimAnimatedDomId hands out when one node renders into several tags.
std::vector<BundleElement> FindBundleElements(
    const std::unordered_map<std::string, std::string>& tagIndex, const std::string& domId) {
  std::vector<BundleElement> elements = {};
  for (size_t copyIndex = 0;; ++copyIndex) {
    BundleElement element = {};
    element.id = copyIndex == 0 ? domId : domId + "__" + std::to_string(copyIndex);
    auto it = tagIndex.find(element.id);
    if (it == tagIndex.end()) {
      break;
    }
    element.tagName = it->second;
    elements.push_back(std::move(element));
  }
  return elements;
}

// Resolves the tags a bundle must be woven into. A node can render into several tags (Repeater
// copies, a Fill shared by a box and a span), and one tag can be shared by several animated nodes
// (a shape, its Fill, its Stroke and its TrimPath), so resolve through the ids recorded for the
// node when a claim was made: searching only for its own base id would miss the tag that a sibling
// candidate claimed. Layers, layer filters and mask paths keep their own id instead.
// Animated tags grouped by the node they render, so a bundle does not have to scan every tag.
using AnimatedTagIndex =
    std::unordered_map<const Node*, std::vector<std::pair<size_t, std::string>>>;

AnimatedTagIndex BuildAnimatedTagIndex(const HTMLWriterContext& ctx) {
  AnimatedTagIndex index = {};
  for (const auto& tag : ctx.animation.tags) {
    index[tag.node].push_back({tag.scope, tag.id});
  }
  return index;
}

std::vector<BundleElement> FindBundleElementsForNode(
    const std::unordered_map<std::string, std::string>& tagIndex,
    const AnimatedTagIndex& tagIndexByNode, const DomTargetBundle& bundle, size_t scope) {
  std::vector<BundleElement> elements = {};
  std::set<std::string> addedIds = {};
  auto it = tagIndexByNode.find(bundle.primaryHost);
  if (it != tagIndexByNode.end()) {
    for (const auto& entry : it->second) {
      if (entry.first != scope || !addedIds.insert(entry.second).second) {
        continue;
      }
      auto nameIt = tagIndex.find(entry.second);
      if (nameIt != tagIndex.end()) {
        elements.push_back({entry.second, nameIt->second});
      }
    }
  }
  if (elements.empty() && scope == 0) {
    return FindBundleElements(tagIndex, bundle.domId);
  }
  return elements;
}

}  // namespace

HTMLAnimationSession::HTMLAnimationSession(PAGXDocument& doc, HTMLWriterContext& ctx, bool enabled)
    : _doc(doc), _ctx(ctx) {
  if (!enabled) {
    return;
  }
  auto index = BuildDocumentIndex(doc);
  if (index.animations.empty()) {
    return;
  }
  _active = true;

  _solidToPainter = std::move(index.solidToPainter);
  _filterToLayer = std::move(index.filterToLayer);
  _maskPathToLayer = std::move(index.maskPathToLayer);
  _timelineOffsets = std::move(index.timelineOffsets);

  ResolveAnimationTargets(index.animations);
}

// Resolves every animated object target to the DOM id its tag will carry. Layer and Group targets
// may need an id assigned; a Path used by a mask registers as coverage-mask geometry instead.
void HTMLAnimationSession::ResolveAnimationTargets(
    const std::vector<const Animation*>& animations) {
  for (auto* anim : animations) {
    for (auto* obj : anim->objects) {
      if (obj == nullptr || obj->channels.empty()) {
        continue;
      }
      Node* targetNode = ResolveTargetNode(_doc, obj->target);
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
        _ctx.animation.elementIds[layer] = layer->id;
        for (auto* ch : obj->channels) {
          if (ch == nullptr) {
            continue;
          }
          auto role = LookupChannelRole(nType, ch->name);
          if (role == TargetChannelRole::LayerVisibility &&
              ch->valueType() == ChannelValueType::Bool) {
            _ctx.animation.visibleLayers.insert(layer);
          }
          if (role == TargetChannelRole::Opacity && !layer->groupOpacity) {
            _restoredGroupOpacity.emplace_back(layer, layer->groupOpacity);
            layer->groupOpacity = true;
          }
        }
      } else if (nType == NodeType::Rectangle || nType == NodeType::Ellipse) {
        bool hasGeometryChannel = false;
        for (auto* channel : obj->channels) {
          if (channel != nullptr &&
              LookupChannelRole(nType, channel->name) != TargetChannelRole::Unknown) {
            hasGeometryChannel = true;
            break;
          }
        }
        if (hasGeometryChannel) {
          _ctx.animation.elementIds[targetNode] =
              targetNode->id.empty() ? _ctx.nextId("animGeo") : targetNode->id;
        }
      } else if (nType == NodeType::TrimPath) {
        auto* trim = static_cast<const TrimPath*>(targetNode);
        if (trim->type == TrimType::Separate && trim->start == 0.0f && trim->offset == 0.0f) {
          for (auto* channel : obj->channels) {
            if (channel != nullptr &&
                LookupChannelRole(nType, channel->name) == TargetChannelRole::TrimEnd) {
              _ctx.animation.elementIds[trim] = trim->id.empty() ? _ctx.nextId("animTrim") : trim->id;
              break;
            }
          }
        }
      } else if (nType == NodeType::Group || nType == NodeType::TextBox) {
        auto* group = static_cast<Group*>(targetNode);
        std::string domId = !group->id.empty() ? group->id : StripAtPrefix(obj->target);
        if (domId.empty()) {
          domId = _ctx.nextId("animG");
        }
        _ctx.animation.elementIds[group] = domId;
      } else if (nType == NodeType::SolidColor) {
        auto* solid = static_cast<const SolidColor*>(targetNode);
        auto it = _solidToPainter.find(solid);
        if (it != _solidToPainter.end() && it->second != nullptr) {
          std::string domId = !solid->id.empty() ? solid->id : StripAtPrefix(obj->target);
          if (domId.empty()) {
            domId = _ctx.nextId("animC");
          }
          _ctx.animation.elementIds[it->second] = domId;
        }
      } else if (nType == NodeType::Fill || nType == NodeType::Stroke) {
        std::string domId = !targetNode->id.empty() ? targetNode->id : StripAtPrefix(obj->target);
        if (domId.empty()) {
          domId = _ctx.nextId("animP");
        }
        _ctx.animation.elementIds[targetNode] = domId;
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
          auto* hostLayer = maskIt->second;
          // The masked layer id is the routing key of this bundle: without it the bundle resolves
          // to no tag and the mask keyframes would be skipped. A masked layer that is not animated
          // itself therefore still needs one, assigned here and restored by the destructor.
          if (hostLayer->id.empty()) {
            _restoredNodeIds.emplace_back(hostLayer, hostLayer->id);
            hostLayer->id = _ctx.nextId("animM");
          }
          // Coverage masks keep their paint, so they are emitted as an inline <mask> whose
          // geometry is animated instead of being reduced to a clip polygon.
          _ctx.animation.maskPaths.insert(pathNode);
        } else {
          std::string domId = !pathNode->id.empty() ? pathNode->id : StripAtPrefix(obj->target);
          if (domId.empty()) {
            domId = _ctx.nextId("animPath");
          }
          _ctx.animation.elementIds[pathNode] = domId;
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
  // The whole animation state belongs to this session; dropping it as one unit cannot leave a
  // half-cleared scope or registry behind for the next export.
  _ctx.animation.Reset();
}

std::vector<HTMLAnimationSession::AnimationMount>
HTMLAnimationSession::CollectAnimationMounts() const {
  std::vector<AnimationMount> mounts = {};
  for (auto* node : _doc.animations) {
    if (node == nullptr || node->nodeType() != NodeType::Animation) {
      continue;
    }
    auto* anim = static_cast<const Animation*>(node);
    int64_t offset = 0;
    auto it = _timelineOffsets.find(anim->id);
    if (it != _timelineOffsets.end()) {
      offset = it->second;
    }
    mounts.push_back({anim, 0, offset, true});
  }
  // Each Composition slot creates a distinct timeline. Preserve its scope, offset and play state
  // instead of collapsing mounts by Animation id: siblings can share the same source Composition.
  for (const auto& instance : _ctx.animation.instances) {
    for (const auto& driver : instance.slot->timelines) {
      if (driver == nullptr || driver->timelineType() != TimelineType::Animation) {
        continue;
      }
      auto* timeline = static_cast<const AnimationTimeline*>(driver.get());
      for (auto* node : instance.composition->animations) {
        if (node != nullptr && node->nodeType() == NodeType::Animation &&
            static_cast<const Animation*>(node)->id == StripAtPrefix(timeline->animationId)) {
          mounts.push_back({static_cast<const Animation*>(node), instance.scope,
                            timeline->evaluationOffset, timeline->playing});
          break;
        }
      }
    }
  }
  return mounts;
}

std::string HTMLAnimationSession::emitKeyframesAndWeave(std::string& bodyHtml) {
  if (!_active) {
    return {};
  }

  std::string keyframesCss;
  int animCounter = 0;

  ElementEdits edits;

  const auto mounts = CollectAnimationMounts();

  // All indexes are built once: resolving every animated tag per bundle would otherwise rescan the
  // whole body for each target.
  const auto tagIndex = BuildTagIndex(bodyHtml);
  const auto tagIndexByNode = BuildAnimatedTagIndex(_ctx);

  for (const auto& mount : mounts) {
    const auto* anim = mount.animation;
    if (anim->duration <= 0 || anim->frameRate <= 0.0f || anim->objects.empty()) {
      continue;
    }
    auto bundles = GroupObjectsIntoDomBundles(_doc, anim, _ctx, _solidToPainter, _filterToLayer,
                                              _maskPathToLayer);
    for (const auto& bundle : bundles) {
      auto elements =
          FindBundleElementsForNode(tagIndex.nameById, tagIndexByNode, bundle, mount.scope);
      if (elements.empty()) {
        continue;
      }
      BundleChannelSet chSet =
          CollectBundleChannels(_doc, bundle, _solidToPainter, _maskPathToLayer);
      if (chSet.keyTimes.empty()) {
        continue;
      }
      BundleTimingInfo timing = ComputeBundleTiming(anim, mount.offset, chSet);

      if (chSet.animatedMaskPath != nullptr) {
        auto idIt = _ctx.animation.maskPathIds.find(chSet.animatedMaskPath);
        if (idIt != _ctx.animation.maskPathIds.end()) {
          // The mask is an inline SVG subtree, so its geometry animates on its own: emit `d` stops
          // for the mask path and let the masked element keep its static style. A coverage mask
          // must never be rewritten as a clip polygon, which would drop its paint.
          const Frame windowStart = timing.bundleShiftFrames;
          const Frame windowEnd = windowStart + timing.effectiveDuration;
          std::set<Frame> stopTimes = {};
          for (Frame t : chSet.keyTimes) {
            if (t >= windowStart && t <= windowEnd) {
              stopTimes.insert(t);
            }
          }
          stopTimes.insert(windowStart);
          stopTimes.insert(windowEnd);
          std::string stops = {};
          for (Frame t : stopTimes) {
            std::string d = RebuildAnimatedPathSVGString(chSet.animatedMaskPath, chSet.maskPtX,
                                                         chSet.maskPtY, t);
            Frame relTime = (t >= windowStart) ? (t - windowStart) : 0;
            stops += "  " + FormatPercent(relTime, timing.effectiveDuration) + " { d: path(\"" + d +
                     "\"); }\n";
          }
          std::string kfName = MakeSafeKeyframesName(idIt->second, animCounter++);
          keyframesCss += "@keyframes " + kfName + " {\n" + stops + "}\n";
          keyframesCss +=
              "#" + idIt->second + " { animation: " +
              BuildAnimationShorthand(anim, kfName, timing, mount.offset, mount.scope != 0) +
              "; }\n";
          continue;
        }
      }

      // One animated node can render into tags of different kinds (a Fill painting both a box and
      // a text span), and paint channels map to a different CSS property per kind. Keyframes are
      // therefore generated per tag name, and kinds that produce identical stops share one rule.
      bool needsZeroOrigin = false;
      std::unordered_map<std::string, std::string> keyframesNameByTag = {};
      std::vector<std::pair<std::string, std::string>> emittedRules = {};
      for (const auto& element : elements) {
        auto nameIt = keyframesNameByTag.find(element.tagName);
        if (nameIt == keyframesNameByTag.end()) {
          std::string stops =
              EmitBundleKeyframeStops(bundle, chSet, timing, element.tagName, needsZeroOrigin);
          std::string kfName = {};
          for (const auto& rule : emittedRules) {
            if (rule.first == stops) {
              kfName = rule.second;
              break;
            }
          }
          if (kfName.empty()) {
            kfName = MakeSafeKeyframesName(bundle.domId, animCounter++);
            keyframesCss += "@keyframes " + kfName + " {\n" + stops + "}\n";
            emittedRules.emplace_back(stops, kfName);
          }
          nameIt = keyframesNameByTag.emplace(element.tagName, kfName).first;
        }
        if (edits.animDecls.find(element.id) == edits.animDecls.end()) {
          edits.idOrder.push_back(element.id);
        }
        edits.animDecls[element.id].push_back(
            BuildAnimationShorthand(anim, nameIt->second, timing, mount.offset, mount.scope != 0));
        edits.playStates[element.id].push_back(mount.playing);
      }
      if (needsZeroOrigin) {
        for (const auto& element : elements) {
          edits.needsZeroOrigin[element.id] = true;
        }
      }
      // Stroke width and shape width can expand SVG paint beyond its static viewBox. Other paint
      // channels (color, opacity, dash offset) do not change the viewport-sized extent.
      if (chSet.strokeWidthCh != nullptr || chSet.geometryWidthCh != nullptr) {
        for (const auto& element : elements) {
          edits.liftsSvgClip[element.id] = true;
        }
      }
    }
  }

  ApplyElementEdits(bodyHtml, tagIndex.rangeById, edits);

  return keyframesCss;
}

// One replacement of a byte range in the body, collected while resolving the recorded ids.
struct TextEdit {
  size_t start = 0;
  size_t end = 0;
  std::string text = {};
};

bool TextEditStartsBefore(const TextEdit& a, const TextEdit& b) {
  return a.start < b.start;
}

// Applies every recorded declaration and rebuilds the body once. Applying one declaration at a time
// and letting std::string::replace shift the tail costs O(targets x bodyLength).
void HTMLAnimationSession::ApplyElementEdits(
    std::string& bodyHtml,
    const std::unordered_map<std::string, std::pair<size_t, size_t>>& tagRanges,
    const ElementEdits& edits) {
  std::vector<TextEdit> textEdits = {};
  std::unordered_set<size_t> liftedSvgStarts = {};
  for (const auto& elementId : edits.idOrder) {
    auto rangeIt = tagRanges.find(elementId);
    if (rangeIt == tagRanges.end()) {
      continue;
    }
    auto declIt = edits.animDecls.find(elementId);
    if (declIt == edits.animDecls.end()) {
      continue;
    }
    const auto& decls = declIt->second;
    std::string combined;
    for (size_t i = 0; i < decls.size(); ++i) {
      if (i > 0) {
        combined += ", ";
      }
      combined += decls[i];
    }
    std::vector<bool> noPlayStates = {};
    auto playIt = edits.playStates.find(elementId);
    const auto& playStates = playIt != edits.playStates.end() ? playIt->second : noPlayStates;
    if (std::find(playStates.begin(), playStates.end(), false) != playStates.end()) {
      combined += ";animation-play-state:";
      for (size_t i = 0; i < playStates.size(); ++i) {
        if (i > 0) {
          combined += ", ";
        }
        combined += playStates[i] ? "running" : "paused";
      }
    }
    auto zeroIt = edits.needsZeroOrigin.find(elementId);
    bool needsZeroOrigin = zeroIt != edits.needsZeroOrigin.end() && zeroIt->second;
    auto liftIt = edits.liftsSvgClip.find(elementId);
    bool liftsSvgClip = liftIt != edits.liftsSvgClip.end() && liftIt->second;
    size_t tagStart = rangeIt->second.first;
    size_t tagEnd = rangeIt->second.second;
    textEdits.push_back({tagStart, tagEnd,
                         BuildWovenTag(bodyHtml.substr(tagStart, tagEnd - tagStart), combined,
                                       needsZeroOrigin)});
    // The SVG viewport is sized for the static paint, so a shape whose animation widens its painted
    // extent would be clipped; lift the clipping on the hosting <svg> while keeping its viewport.
    if (liftsSvgClip) {
      auto svgStart = FindEnclosingSvgStart(bodyHtml, tagStart);
      if (svgStart != std::string::npos && liftedSvgStarts.insert(svgStart).second) {
        auto svgEnd = bodyHtml.find('>', svgStart);
        if (svgEnd != std::string::npos) {
          auto liftedTag = BuildLiftedSvgTag(bodyHtml.substr(svgStart, svgEnd - svgStart));
          if (!liftedTag.empty()) {
            textEdits.push_back({svgStart, svgEnd, std::move(liftedTag)});
          }
        }
      }
    }
  }

  if (!textEdits.empty()) {
    std::sort(textEdits.begin(), textEdits.end(), TextEditStartsBefore);
    std::string rebuilt;
    rebuilt.reserve(bodyHtml.size() + textEdits.size() * 64);
    size_t cursor = 0;
    for (const auto& edit : textEdits) {
      if (edit.start < cursor) {
        continue;
      }
      rebuilt.append(bodyHtml, cursor, edit.start - cursor);
      rebuilt += edit.text;
      cursor = edit.end;
    }
    rebuilt.append(bodyHtml, cursor, std::string::npos);
    bodyHtml = std::move(rebuilt);
  }
}

}  // namespace pagx
