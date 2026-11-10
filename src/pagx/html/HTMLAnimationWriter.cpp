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
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/runtime/Matrix3DDecompose.h"
#include "pagx/runtime/MatrixDecompose.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/utils/ExporterUtils.h"
#include "pagx/utils/LayerTransformUtils.h"
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
      CollectKeyframeTimesAndEasing(static_cast<const TypedChannel<Matrix3D>*>(ch)->keyframes,
                                    timesOut, easingOut);
      break;
    case ChannelValueType::PAGImage:
      // An image reference has no CSS animation channel.
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

Matrix3D EvalMatrix3DChannel(const Channel* ch, Frame frame, const Matrix3D& fallback) {
  if (ch == nullptr) {
    return fallback;
  }
  KeyValue kv = ch->evaluateAt(frame);
  const auto* v = std::get_if<Matrix3D>(&kv);
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
  // The path's own placement applies inside the mask subtree, so it must be composed before the
  // mask transform: maskMatrix * pathLocal. Applying them one after another in the wrong order
  // offsets the geometry whenever both are non-identity.
  auto pathPos = pathNode->renderPosition();
  float pathScale = pathNode->renderScale();
  Matrix pathLocal = Matrix::Translate(pathPos.x, pathPos.y) * Matrix::Scale(pathScale, pathScale);
  Matrix combined = transform * pathLocal;
  if (!combined.isIdentity()) {
    for (auto& pt : pts) {
      pt = combined.mapPoint(pt);
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
  // Reversed paths wind the other way; the mask fill rule depends on it, and the static path string
  // (GetPathSVGString) applies the same reversal.
  if (pathNode->reversed) {
    PathData reversedData = PathDataFromSVGString("");
    ReversePathData(rebuilt, reversedData);
    return PathDataToSVGString(reversedData);
  }
  return PathDataToSVGString(rebuilt);
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
// without a style attribute gets one after its last attribute. `tagStr` is the opening tag without
// its closing '>', so the new attribute goes at the end, or before the '/' of a self-closing tag.
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
  size_t insertPos = woven.size();
  if (insertPos > 0 && woven[insertPos - 1] == '>') {
    insertPos -= 1;
  }
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

// True when the layer has a pivot, which the matrix3d keyframes cannot reproduce yet.
bool LayerHasAnchorFor3D(const Node* target) {
  if (target == nullptr || target->nodeType() != NodeType::Layer) {
    return false;
  }
  auto* layer = static_cast<const Layer*>(target);
  return !FloatNearlyZero(layer->anchor.x) || !FloatNearlyZero(layer->anchor.y) ||
         !FloatNearlyZero(layer->anchorZ);
}

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
  TrimStart,
  // A Repeater channel fans out to its copies: each copy recomposes the per-copy matrix per frame.
  RepeaterRotation,
  RepeaterCopies,
  RepeaterStartAlpha,
  RepeaterPositionX,
  // Text.x / Text.y are layout channels: they move the text anchor, not a transform.
  TextAnchorX,
  TextAnchorY,
  // RangeSelector range channels fan out to the glyphs of the TextModifier that owns the selector.
  RangeSelectorStart,
  RangeSelectorEnd,
  RangeSelectorOffset,
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
  StrokeMiterLimit,
  SolidColorValue,
  SolidColorAlpha,
  FilterBlurX,
  FilterBlurY,
  FilterOffsetX,
  FilterOffsetY,
  FilterColor,
  FilterAlpha,
  // Colour of the <feFlood> inside an SVG filter pipeline (inner shadow, blend).
  FilterFloodColor,
  // Polystar outline channels: every one of them changes the path, so the frame rebuilds it.
  PolystarPointCount,
  PolystarOuterRadius,
  PolystarInnerRadius,
  PolystarOuterRoundness,
  PolystarInnerRoundness,
  PolystarRotation,
  PolystarPositionX,
  PolystarPositionY,
  // Rectangle / Ellipse geometry: the shape is a box, so these map onto CSS box properties.
  GeometryHeight,
  GeometryPosX,
  GeometryPosY,
  // TrimPath offset shifts the visible run along the path (degrees, 360 = one full lap).
  TrimOffset,
  // Layer skew.x / skew.y are the CSS skew angles already, unlike the Group single-axis skew.
  LayerSkewX,
  LayerSkewY,
  RoundCornerRadius,
  // Layer 3D channels: no 2D transform function expresses them, so the frame emits a matrix3d.
  AnchorZ,
  LayerRotationX,
  LayerRotationY,
  LayerZ,
  LayerPerspective,
  LayerMatrix3D,
  FontSize,
  LetterSpacing,
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
    // The Layer spin is "rotation.z" in PAGX; a bare "rotation" never matches a Layer channel.
    {NodeType::Layer, "rotation.z", TargetChannelRole::Rotation},
    {NodeType::Layer, "skew.x", TargetChannelRole::LayerSkewX},
    {NodeType::Layer, "skew.y", TargetChannelRole::LayerSkewY},
    {NodeType::Layer, "rotation.x", TargetChannelRole::LayerRotationX},
    {NodeType::Layer, "rotation.y", TargetChannelRole::LayerRotationY},
    {NodeType::Layer, "z", TargetChannelRole::LayerZ},
    {NodeType::Layer, "perspective", TargetChannelRole::LayerPerspective},
    {NodeType::Layer, "anchor.x", TargetChannelRole::AnchorX},
    {NodeType::Layer, "anchor.y", TargetChannelRole::AnchorY},
    {NodeType::Layer, "anchor.z", TargetChannelRole::AnchorZ},
    {NodeType::Layer, "matrix3D", TargetChannelRole::LayerMatrix3D},
    {NodeType::Layer, "scale.x", TargetChannelRole::ScaleX},
    {NodeType::Layer, "scale.y", TargetChannelRole::ScaleY},

    // Shape geometry channels
    {NodeType::Polystar, "pointCount", TargetChannelRole::PolystarPointCount},
    {NodeType::Polystar, "outerRadius", TargetChannelRole::PolystarOuterRadius},
    {NodeType::Polystar, "innerRadius", TargetChannelRole::PolystarInnerRadius},
    {NodeType::Polystar, "outerRoundness", TargetChannelRole::PolystarOuterRoundness},
    {NodeType::Polystar, "innerRoundness", TargetChannelRole::PolystarInnerRoundness},
    {NodeType::Polystar, "rotation", TargetChannelRole::PolystarRotation},
    {NodeType::Polystar, "position.x", TargetChannelRole::PolystarPositionX},
    {NodeType::Polystar, "position.y", TargetChannelRole::PolystarPositionY},
    {NodeType::Rectangle, "size.width", TargetChannelRole::GeometryWidth},
    {NodeType::Rectangle, "size.height", TargetChannelRole::GeometryHeight},
    {NodeType::Rectangle, "position.x", TargetChannelRole::GeometryPosX},
    {NodeType::Rectangle, "position.y", TargetChannelRole::GeometryPosY},
    {NodeType::Ellipse, "size.height", TargetChannelRole::GeometryHeight},
    {NodeType::Ellipse, "position.x", TargetChannelRole::GeometryPosX},
    {NodeType::Ellipse, "position.y", TargetChannelRole::GeometryPosY},
    {NodeType::Rectangle, "roundness", TargetChannelRole::RectangleRoundness},
    {NodeType::Ellipse, "size.width", TargetChannelRole::GeometryWidth},
    {NodeType::TrimPath, "end", TargetChannelRole::TrimEnd},
    {NodeType::TrimPath, "start", TargetChannelRole::TrimStart},
    {NodeType::TrimPath, "offset", TargetChannelRole::TrimOffset},
    {NodeType::Repeater, "rotation", TargetChannelRole::RepeaterRotation},
    {NodeType::Repeater, "copies", TargetChannelRole::RepeaterCopies},
    {NodeType::Repeater, "startAlpha", TargetChannelRole::RepeaterStartAlpha},
    {NodeType::Repeater, "position.x", TargetChannelRole::RepeaterPositionX},
    {NodeType::RangeSelector, "start", TargetChannelRole::RangeSelectorStart},
    {NodeType::RangeSelector, "end", TargetChannelRole::RangeSelectorEnd},
    {NodeType::RangeSelector, "offset", TargetChannelRole::RangeSelectorOffset},
    {NodeType::RoundCorner, "radius", TargetChannelRole::RoundCornerRadius},

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
    {NodeType::Text, "fontSize", TargetChannelRole::FontSize},
    {NodeType::Text, "letterSpacing", TargetChannelRole::LetterSpacing},
    {NodeType::Text, "x", TargetChannelRole::TextAnchorX},
    {NodeType::Text, "y", TargetChannelRole::TextAnchorY},
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
    {NodeType::Stroke, "miterLimit", TargetChannelRole::StrokeMiterLimit},
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
    {NodeType::InnerShadowFilter, "color", TargetChannelRole::FilterFloodColor},
    {NodeType::BlendFilter, "color", TargetChannelRole::FilterFloodColor},
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
  // Set for a Repeater copy bundle: which copy this tag paints.
  int repeaterIndex = -1;
  // Set for a RangeSelector glyph bundle: index into AnimationState::rangeSelectorGlyphs.
  int rangeGlyphIndex = -1;
  // Set for a Text bundle whose x/y channel is animated: the left/top the tag was placed at.
  bool hasTextOrigin = false;
  float textOriginX = 0.0f;
  float textOriginY = 0.0f;
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
  const Channel* trimStartCh = nullptr;
  const Channel* repeaterRotationCh = nullptr;
  const Channel* floodColorCh = nullptr;
  const Channel* geometryHeightCh = nullptr;
  const Channel* geometryPosXCh = nullptr;
  const Channel* geometryPosYCh = nullptr;
  const Channel* trimOffsetCh = nullptr;
  // Polystar: every channel rewrites the outline, so one struct keeps them together.
  bool hasPolystarGeometry = false;
  const Channel* starPointCountCh = nullptr;
  const Channel* starOuterRadiusCh = nullptr;
  const Channel* starInnerRadiusCh = nullptr;
  const Channel* starOuterRoundnessCh = nullptr;
  const Channel* starInnerRoundnessCh = nullptr;
  const Channel* starRotationCh = nullptr;
  const Channel* starPosXCh = nullptr;
  const Channel* starPosYCh = nullptr;
  // Static colour of the flooding filter, used when the channel is absent from a stop.
  Color floodBaseColor = {};
  const Channel* repeaterCopiesCh = nullptr;
  const Channel* repeaterStartAlphaCh = nullptr;
  const Channel* repeaterPositionXCh = nullptr;
  const Channel* textAnchorXCh = nullptr;
  const Channel* textAnchorYCh = nullptr;
  const Channel* rangeStartCh = nullptr;
  const Channel* rangeEndCh = nullptr;
  const Channel* rangeOffsetCh = nullptr;
  const HTMLWriterContext::AnimationState::RangeSelectorGlyph* rangeGlyph = nullptr;
  // A RoundCorner on a Rectangle is exactly CSS border-radius, so it stays on the div and can share
  // the element with the shape's own geometry channels instead of taking over its `d`.
  bool roundCornerAsCss = false;
  const Repeater* repeaterTarget = nullptr;
  int repeaterIndex = -1;
  // RoundCorner rounds the geometry declared before it, so the frame rebuilds that shape's `d`.
  const Channel* roundCornerCh = nullptr;
  const RoundCorner* roundCornerTarget = nullptr;
  const Element* roundCornerShape = nullptr;
  const TrimPath* trimTarget = nullptr;
  const Channel* rotCh = nullptr;
  const Channel* scaleXCh = nullptr;
  const Channel* scaleYCh = nullptr;
  const Channel* anchorXCh = nullptr;
  const Channel* anchorYCh = nullptr;
  const Channel* skewCh = nullptr;
  const Channel* layerSkewXCh = nullptr;
  const Channel* layerSkewYCh = nullptr;
  const Channel* anchorZCh = nullptr;
  const Channel* layerRotXCh = nullptr;
  const Channel* layerRotYCh = nullptr;
  const Channel* layerZCh = nullptr;
  const Channel* layerPerspectiveCh = nullptr;
  const Channel* layerMatrix3DCh = nullptr;
  const Channel* skewAxisCh = nullptr;
  const Channel* posXCh = nullptr;
  const Channel* posYCh = nullptr;
  const Channel* fillColorCh = nullptr;
  const Channel* fillAlphaCh = nullptr;
  const Channel* strokeColorCh = nullptr;
  const Channel* strokeAlphaCh = nullptr;
  const Channel* strokeWidthCh = nullptr;
  const Channel* dashOffsetCh = nullptr;
  const Channel* miterLimitCh = nullptr;
  const Fill* fillPainter = nullptr;
  const Stroke* strokePainter = nullptr;
  // Text shaping channels that map to a single CSS property. Text content, font family and font
  // style would need a step swap, and resizing a short single-line label is the only shaping
  // animation CSS reproduces faithfully, so only these two are bound.
  const Channel* fontSizeCh = nullptr;
  const Channel* letterSpacingCh = nullptr;

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
    case TargetChannelRole::GeometryHeight:
      out.geometryHeightCh = ch;
      return true;
    case TargetChannelRole::GeometryPosX:
      out.geometryPosXCh = ch;
      return true;
    case TargetChannelRole::GeometryPosY:
      out.geometryPosYCh = ch;
      return true;
    case TargetChannelRole::RectangleRoundness:
      out.roundnessCh = ch;
      return true;
    case TargetChannelRole::RoundCornerRadius:
      out.roundCornerCh = ch;
      out.roundCornerTarget = static_cast<const RoundCorner*>(objTarget);
      return true;
    // A 3D channel is emitted as one composed matrix3d. Anchored layers need the pivot folded into
    // that matrix differently than the static path does, so they keep the previous behaviour (the
    // channel is not exported) until anchor support lands.
    case TargetChannelRole::AnchorZ:
      out.anchorZCh = ch;
      return true;
    case TargetChannelRole::LayerRotationX:
      out.layerRotXCh = ch;
      return true;
    case TargetChannelRole::LayerRotationY:
      out.layerRotYCh = ch;
      return true;
    case TargetChannelRole::LayerZ:
      out.layerZCh = ch;
      return true;
    case TargetChannelRole::LayerPerspective:
      out.layerPerspectiveCh = ch;
      return true;
    case TargetChannelRole::LayerMatrix3D:
      if (LayerHasAnchorFor3D(objTarget)) {
        return false;
      }
      out.layerMatrix3DCh = ch;
      return true;
    case TargetChannelRole::LayerSkewX:
      out.layerSkewXCh = ch;
      return true;
    case TargetChannelRole::LayerSkewY:
      out.layerSkewYCh = ch;
      return true;
    case TargetChannelRole::RangeSelectorStart:
      out.rangeStartCh = ch;
      return true;
    case TargetChannelRole::RangeSelectorEnd:
      out.rangeEndCh = ch;
      return true;
    case TargetChannelRole::RangeSelectorOffset:
      out.rangeOffsetCh = ch;
      return true;
    case TargetChannelRole::RepeaterCopies:
      out.repeaterCopiesCh = ch;
      out.repeaterTarget = static_cast<const Repeater*>(objTarget);
      return true;
    case TargetChannelRole::TextAnchorX:
      out.textAnchorXCh = ch;
      return true;
    case TargetChannelRole::TextAnchorY:
      out.textAnchorYCh = ch;
      return true;
    case TargetChannelRole::RepeaterStartAlpha:
      out.repeaterStartAlphaCh = ch;
      out.repeaterTarget = static_cast<const Repeater*>(objTarget);
      return true;
    case TargetChannelRole::RepeaterPositionX:
      out.repeaterPositionXCh = ch;
      out.repeaterTarget = static_cast<const Repeater*>(objTarget);
      return true;
    case TargetChannelRole::RepeaterRotation:
      out.repeaterRotationCh = ch;
      out.repeaterTarget = static_cast<const Repeater*>(objTarget);
      return true;
    case TargetChannelRole::TrimOffset: {
      auto* trim = static_cast<const TrimPath*>(objTarget);
      if (trim->type != TrimType::Separate) {
        return false;
      }
      out.trimOffsetCh = ch;
      out.trimTarget = trim;
      return true;
    }
    case TargetChannelRole::TrimStart: {
      auto* trim = static_cast<const TrimPath*>(objTarget);
      if (trim->type != TrimType::Separate || trim->offset != 0.0f) {
        return false;
      }
      out.trimStartCh = ch;
      out.trimTarget = trim;
      return true;
    }
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
    case TargetChannelRole::FontSize:
      out.fontSizeCh = ch;
      return true;
    case TargetChannelRole::LetterSpacing:
      out.letterSpacingCh = ch;
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
    case TargetChannelRole::StrokeMiterLimit:
      out.miterLimitCh = ch;
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
    case TargetChannelRole::PolystarPointCount:
      out.starPointCountCh = ch;
      out.hasPolystarGeometry = true;
      return true;
    case TargetChannelRole::PolystarOuterRadius:
      out.starOuterRadiusCh = ch;
      out.hasPolystarGeometry = true;
      return true;
    case TargetChannelRole::PolystarInnerRadius:
      out.starInnerRadiusCh = ch;
      out.hasPolystarGeometry = true;
      return true;
    case TargetChannelRole::PolystarOuterRoundness:
      out.starOuterRoundnessCh = ch;
      out.hasPolystarGeometry = true;
      return true;
    case TargetChannelRole::PolystarInnerRoundness:
      out.starInnerRoundnessCh = ch;
      out.hasPolystarGeometry = true;
      return true;
    case TargetChannelRole::PolystarRotation:
      out.starRotationCh = ch;
      out.hasPolystarGeometry = true;
      return true;
    case TargetChannelRole::PolystarPositionX:
      out.starPosXCh = ch;
      out.hasPolystarGeometry = true;
      return true;
    case TargetChannelRole::PolystarPositionY:
      out.starPosYCh = ch;
      out.hasPolystarGeometry = true;
      return true;
    case TargetChannelRole::FilterFloodColor:
      // Colour of the <feFlood> inside a pipeline filter: the only filter property CSS can animate.
      out.floodColorCh = ch;
      if (objTarget != nullptr && objTarget->nodeType() == NodeType::InnerShadowFilter) {
        out.floodBaseColor = static_cast<const InnerShadowFilter*>(objTarget)->color;
      } else if (objTarget != nullptr && objTarget->nodeType() == NodeType::BlendFilter) {
        out.floodBaseColor = static_cast<const BlendFilter*>(objTarget)->color;
      }
      return true;
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
  if (result.starPointCountCh != nullptr &&
      result.starPointCountCh->valueType() == ChannelValueType::Float) {
    // Everything else about a polystar is linear in the path's coordinates (position and rotation
    // ride on `transform`), but a point count step changes how many points the outline has, which
    // CSS cannot interpolate. Stop on both sides of every integer the channel crosses.
    const auto& keys = static_cast<const TypedChannel<float>*>(result.starPointCountCh)->keyframes;
    constexpr Frame kCrossingEpsilon = 1;
    if (!keys.empty()) {
      // A count that lands on an integer at the end must not be reached by morphing the whole last
      // segment: hold the previous count until the frame before the final key.
      result.keyTimes.insert(keys.back().time - kCrossingEpsilon);
    }
    for (size_t k = 0; k + 1 < keys.size(); ++k) {
      double v0 = keys[k].value;
      double v1 = keys[k + 1].value;
      if (v1 == v0) {
        continue;
      }
      double lo = std::min(v0, v1);
      double hi = std::max(v0, v1);
      for (double boundary = std::floor(lo) + 1.0; boundary < hi; boundary += 1.0) {
        double ratio = (boundary - v0) / (v1 - v0);
        Frame crossing =
            keys[k].time + static_cast<Frame>(ratio * (keys[k + 1].time - keys[k].time));
        // Hold the old point count up to the crossing, then switch: CSS would otherwise morph the
        // two outlines into each other across the whole interval.
        result.keyTimes.insert(crossing - kCrossingEpsilon);
        result.keyTimes.insert(crossing + kCrossingEpsilon);
      }
    }
  }
  if (result.starOuterRoundnessCh != nullptr || result.starInnerRoundnessCh != nullptr) {
    // Roundness bends the outline through bezier handles, so the path coordinates are not linear in
    // it; sampling every few frames keeps each segment straight enough to interpolate.
    constexpr Frame kRoundnessStep = 4;
    Frame first = *result.keyTimes.begin();
    Frame last = *result.keyTimes.rbegin();
    for (Frame t = first; t <= last; t += kRoundnessStep) {
      result.keyTimes.insert(t);
    }
  }
  // A Repeater copy bundle drives exactly one copy; the emitter needs its index to recompose the
  // per-copy matrix.
  result.repeaterIndex = bundle.repeaterIndex;
  if (result.repeaterCopiesCh != nullptr &&
      result.repeaterCopiesCh->valueType() == ChannelValueType::Float) {
    // Crossing an integer copy count appends a copy and shifts every other copy's index, which is a
    // step in the middle of an otherwise linear channel. Stops on both sides of each crossing keep
    // that step instead of letting CSS interpolate through it.
    const auto& keys = static_cast<const TypedChannel<float>*>(result.repeaterCopiesCh)->keyframes;
    // One frame after the crossing: the stop carries the new copy layout.
    constexpr Frame kCrossingEpsilon = 1;
    for (size_t k = 0; k + 1 < keys.size(); ++k) {
      // Only linear segments have a single crossing time; other segments are sampled at their keys.
      double v0 = keys[k].value;
      double v1 = keys[k + 1].value;
      if (v1 == v0) {
        continue;
      }
      // A key that lands exactly on an integer starts a new copy count immediately after it, so the
      // frame right after the key already shows the shifted layout.
      result.keyTimes.insert(keys[k].time + kCrossingEpsilon);
      double lo = std::min(v0, v1);
      double hi = std::max(v0, v1);
      for (double boundary = std::floor(lo) + 1.0; boundary < hi; boundary += 1.0) {
        double ratio = (boundary - v0) / (v1 - v0);
        Frame crossing =
            keys[k].time + static_cast<Frame>(ratio * (keys[k + 1].time - keys[k].time));
        result.keyTimes.insert(crossing);
        result.keyTimes.insert(crossing + kCrossingEpsilon);
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
  if ((chSet.textAnchorXCh != nullptr || chSet.textAnchorYCh != nullptr) && bundle.hasTextOrigin &&
      bundle.primaryHost != nullptr && bundle.primaryHost->nodeType() == NodeType::Text) {
    // Text.x / Text.y feed the layout anchor, so the tag keeps its position:absolute placement and
    // the frame moves left/top by the channel delta instead of wrapping it in a transform.
    const Text* text = static_cast<const Text*>(bundle.primaryHost);
    float x = EvalFloatChannel(chSet.textAnchorXCh, t, text->position.x);
    float y = EvalFloatChannel(chSet.textAnchorYCh, t, text->position.y);
    AppendCssDecl(decls, "left",
                  CssFloatToString(bundle.textOriginX + x - text->position.x) + "px");
    AppendCssDecl(decls, "top", CssFloatToString(bundle.textOriginY + y - text->position.y) + "px");
    return;
  }
  bool hasRepeaterChannel =
      chSet.repeaterCopiesCh != nullptr || chSet.repeaterRotationCh != nullptr ||
      chSet.repeaterStartAlphaCh != nullptr || chSet.repeaterPositionXCh != nullptr;
  if (hasRepeaterChannel && chSet.repeaterTarget != nullptr && chSet.repeaterIndex >= 0) {
    // A repeater paints one copy per repetition, so every copy recomposes its own pose and opacity
    // from the channel values at this frame. `copies` additionally decides how many copies exist and
    // shifts the remaining indices, which is why n and idx are evaluated here too.
    const Repeater* rep = chSet.repeaterTarget;
    RepeaterValues values = RepeaterValuesFrom(rep);
    values.rotation = EvalFloatChannel(chSet.repeaterRotationCh, t, rep->rotation);
    values.startAlpha = EvalFloatChannel(chSet.repeaterStartAlphaCh, t, rep->startAlpha);
    values.positionX = EvalFloatChannel(chSet.repeaterPositionXCh, t, rep->position.x);
    float copiesValue = std::max(EvalFloatChannel(chSet.repeaterCopiesCh, t, rep->copies), 0.0f);
    int n = static_cast<int>(std::ceil(copiesValue));
    float frac = copiesValue - std::floor(copiesValue);
    int i = chSet.repeaterIndex;
    int idx = (rep->order == RepeaterOrder::AboveOriginal) ? i : (n - 1 - i);
    float alpha = 0.0f;
    if (i < n) {
      alpha = 1.0f;
      float denom = std::max(static_cast<float>(n), 1.0f);
      float np = std::clamp((static_cast<float>(idx) + rep->offset) / denom, 0.0f, 1.0f);
      alpha *= values.startAlpha + (values.endAlpha - values.startAlpha) * np;
      if (idx == n - 1 && frac > 0.0f && frac < 1.0f) {
        // The trailing copy is only partially there.
        alpha *= frac;
      }
    }
    AppendCssDecl(decls, "opacity", CssFloatToString(alpha));
    Matrix m = BuildRepeaterCopyMatrixAtProg(rep, static_cast<float>(idx) + rep->offset, values);
    std::string css = MatrixTransformToCSS(m);
    // An identity copy has no matrix to print; `none` keeps the stop valid instead of emitting an
    // empty declaration the browser would drop.
    AppendCssDecl(decls, "transform", css.empty() ? "none" : css);
    needsZeroOrigin = true;
    return;
  }
  const Layer* hostLayer = nullptr;
  if (bundle.primaryHost != nullptr && bundle.primaryHost->nodeType() == NodeType::Layer) {
    hostLayer = static_cast<const Layer*>(bundle.primaryHost);
  }
  // anchor.z moves the pivot along the depth axis, so it needs the 3D list even on its own.
  bool has3DChannel = chSet.layerRotXCh != nullptr || chSet.layerRotYCh != nullptr ||
                      chSet.layerZCh != nullptr || chSet.layerPerspectiveCh != nullptr ||
                      chSet.layerMatrix3DCh != nullptr || chSet.anchorZCh != nullptr;
  if (has3DChannel && hostLayer != nullptr) {
    // PAGX composes P * T * T(anchor) * L * T(-anchor), with the projection outermost. A CSS
    // function list in the same order reproduces it exactly: `perspective()` is the projection,
    // the anchor is spelled out as a translate pair so no transform-origin conjugation applies,
    // and the pose functions interpolate the way the runtime re-composes each frame instead of
    // relying on matrix decomposition.
    std::string tf;
    auto append = [&tf](const std::string& part) {
      if (!tf.empty()) {
        tf += ' ';
      }
      tf += part;
    };
    if (chSet.layerMatrix3DCh != nullptr) {
      Matrix3D matrix = EvalMatrix3DChannel(chSet.layerMatrix3DCh, t, hostLayer->matrix3D);
      auto anchored = ApplyAnchorToMatrix(matrix, hostLayer->anchor, hostLayer->anchorZ);
      AppendCssDecl(decls, "transform", Matrix3DToCSS(anchored));
      needsZeroOrigin = true;
      return;
    }

    float persist = EvalFloatChannel(chSet.layerPerspectiveCh, t, hostLayer->perspective);
    if (persist > 0.0f) {
      append("perspective(" + CssFloatToString(persist) + "px)");
    }
    float z = EvalFloatChannel(chSet.layerZCh, t, hostLayer->z);
    if (!FloatNearlyZero(hostLayer->x) || !FloatNearlyZero(hostLayer->y) || !FloatNearlyZero(z) ||
        chSet.layerZCh != nullptr) {
      append("translate3d(" + CssFloatToString(hostLayer->x) + "px, " +
             CssFloatToString(hostLayer->y) + "px, " + CssFloatToString(z) + "px)");
    }
    // The anchor conjugates the affine part only; spelling it out keeps the projection outside.
    float anchorX = EvalFloatChannel(chSet.anchorXCh, t, hostLayer->anchor.x);
    float anchorY = EvalFloatChannel(chSet.anchorYCh, t, hostLayer->anchor.y);
    float anchorZ = EvalFloatChannel(chSet.anchorZCh, t, hostLayer->anchorZ);
    bool hasAnchor =
        !FloatNearlyZero(anchorX) || !FloatNearlyZero(anchorY) || !FloatNearlyZero(anchorZ);
    if (hasAnchor) {
      append("translate3d(" + CssFloatToString(anchorX) + "px, " + CssFloatToString(anchorY) +
             "px, " + CssFloatToString(anchorZ) + "px)");
    }
    float rx = EvalFloatChannel(chSet.layerRotXCh, t, hostLayer->rotationX);
    float ry = EvalFloatChannel(chSet.layerRotYCh, t, hostLayer->rotationY);
    if (!FloatNearlyZero(rx) || chSet.layerRotXCh != nullptr) {
      append("rotateX(" + CssFloatToString(rx) + "deg)");
    }
    if (!FloatNearlyZero(ry) || chSet.layerRotYCh != nullptr) {
      append("rotateY(" + CssFloatToString(ry) + "deg)");
    }
    if (!FloatNearlyZero(hostLayer->rotationZ)) {
      append("rotateZ(" + CssFloatToString(hostLayer->rotationZ) + "deg)");
    }
    if (!FloatNearlyZero(hostLayer->skewX) || !FloatNearlyZero(hostLayer->skewY)) {
      append("skew(" + CssFloatToString(hostLayer->skewX) + "deg, " +
             CssFloatToString(hostLayer->skewY) + "deg)");
    }
    if (!FloatNearlyZero(hostLayer->scaleX - 1.0f) || !FloatNearlyZero(hostLayer->scaleY - 1.0f)) {
      append("scale(" + CssFloatToString(hostLayer->scaleX) + ", " +
             CssFloatToString(hostLayer->scaleY) + ")");
    }
    if (hasAnchor) {
      append("translate3d(" + CssFloatToString(-anchorX) + "px, " + CssFloatToString(-anchorY) +
             "px, " + CssFloatToString(-anchorZ) + "px)");
    }
    if (tf.empty()) {
      return;
    }
    AppendCssDecl(decls, "transform", tf);
    needsZeroOrigin = true;
    return;
  }
  // A pose channel (rotation / scale / skew / anchor) folds the authored x/y into the transform, so
  // the tag drops its left/top; the translation branch below assumes left/top still carries the
  // base position and would silently lose both the base and the pose when the two are combined.
  bool hasPoseChannel =
      chSet.rotCh != nullptr || chSet.scaleXCh != nullptr || chSet.scaleYCh != nullptr ||
      chSet.posXCh != nullptr || chSet.posYCh != nullptr || chSet.anchorXCh != nullptr ||
      chSet.anchorYCh != nullptr || chSet.skewCh != nullptr || chSet.skewAxisCh != nullptr ||
      chSet.layerSkewXCh != nullptr || chSet.layerSkewYCh != nullptr;
  if (!hasPoseChannel &&
      (chSet.matrixCh != nullptr || chSet.xCh != nullptr || chSet.yCh != nullptr)) {
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
  if (hasPoseChannel || chSet.xCh != nullptr || chSet.yCh != nullptr) {
    float basePosX = 0.0f;
    float basePosY = 0.0f;
    float baseRot = 0.0f;
    float baseSX = 1.0f;
    float baseSY = 1.0f;
    float baseSkew = 0.0f;
    float baseSkewAxis = 0.0f;
    float baseSkewX = 0.0f;
    float baseSkewY = 0.0f;
    Point baseAnchor = {0.0f, 0.0f};
    bool isTextBoxHost =
        (bundle.primaryHost != nullptr && bundle.primaryHost->nodeType() == NodeType::TextBox);
    bool isLayerHost =
        (bundle.primaryHost != nullptr && bundle.primaryHost->nodeType() == NodeType::Layer);
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
    } else if (isLayerHost) {
      // A Layer keeps its authored components; left/top already carry the translation.
      auto* layer = static_cast<const Layer*>(bundle.primaryHost);
      baseRot = layer->rotationZ;
      baseSX = layer->scaleX;
      baseSY = layer->scaleY;
      baseSkewX = layer->skewX;
      baseSkewY = layer->skewY;
      baseAnchor = {layer->anchor.x, layer->anchor.y};
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
    if (isLayerHost) {
      // The pose list owns the position for a Layer, so the authored x/y is folded in here instead
      // of being emitted as left/top. An x/y channel supplies the value per frame.
      auto* layer = static_cast<const Layer*>(bundle.primaryHost);
      float layerX = EvalFloatChannel(chSet.xCh, t, layer->x);
      float layerY = EvalFloatChannel(chSet.yCh, t, layer->y);
      if (chSet.xCh != nullptr || chSet.yCh != nullptr || !FloatNearlyZero(layerX) ||
          !FloatNearlyZero(layerY)) {
        appendFunction("translate(" + CssFloatToString(layerX) + "px, " + CssFloatToString(layerY) +
                       "px)");
      }
    }
    if (!FloatNearlyZero(px) || !FloatNearlyZero(py) || chSet.posXCh != nullptr ||
        chSet.posYCh != nullptr) {
      appendFunction("translate(" + CssFloatToString(px) + "px, " + CssFloatToString(py) + "px)");
    }
    // A Layer pose translates the pivot forward before the rotation; the Group matrix only carries
    // the trailing -anchor, so only a Layer emits this half of the pair.
    if (isLayerHost && (chSet.anchorXCh != nullptr || chSet.anchorYCh != nullptr ||
                        !FloatNearlyZero(baseAnchor.x) || !FloatNearlyZero(baseAnchor.y))) {
      appendFunction("translate(" + CssFloatToString(anchorX) + "px, " + CssFloatToString(anchorY) +
                     "px)");
    }
    if (chSet.rotCh != nullptr || !FloatNearlyZero(rot)) {
      appendFunction("rotate(" + CssFloatToString(rot) + "deg)");
    }
    if (chSet.layerSkewXCh != nullptr || chSet.layerSkewYCh != nullptr) {
      // Layer skew.x / skew.y already are the CSS skew angles (the 2D shear is [[1, tan(x)],
      // [tan(y), 1]]), so they map straight onto the two CSS functions.
      float skewX = EvalFloatChannel(chSet.layerSkewXCh, t, baseSkewX);
      float skewY = EvalFloatChannel(chSet.layerSkewYCh, t, baseSkewY);
      if (chSet.layerSkewXCh != nullptr || !FloatNearlyZero(skewX)) {
        appendFunction("skewX(" + CssFloatToString(skewX) + "deg)");
      }
      if (chSet.layerSkewYCh != nullptr || !FloatNearlyZero(skewY)) {
        appendFunction("skewY(" + CssFloatToString(skewY) + "deg)");
      }
    } else if (chSet.skewCh != nullptr || !FloatNearlyZero(baseSkew)) {
      // A zero skew is an identity shear, so the skew axis only matters while there is a skew.
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
  if (chSet.miterLimitCh != nullptr) {
    const Stroke* miterStroke = chSet.strokePainter;
    float limit = EvalFloatChannel(chSet.miterLimitCh, t,
                                   miterStroke != nullptr ? miterStroke->miterLimit : 4.0f);
    AppendCssDecl(decls, "stroke-miterlimit", CssFloatToString(limit));
  }
  if (chSet.dashOffsetCh != nullptr) {
    float doff = EvalFloatChannel(chSet.dashOffsetCh, t, 0.0f);
    AppendCssDecl(decls, "stroke-dashoffset", CssFloatToString(doff));
  }
}

// Text shaping channels that map to a CSS property are emitted on the span itself. Only font size
// and letter spacing are bound: CSS interpolates them while re-laying out the text, which is what
// the runtime does when it reshapes. That holds for the short single-line labels this is used for;
// text content, font family and font style would need a step swap and stay static.
void EmitTextCssAtFrame(const DomTargetBundle& bundle, const BundleChannelSet& chSet, Frame t,
                        std::string& decls) {
  if (chSet.fontSizeCh == nullptr && chSet.letterSpacingCh == nullptr) {
    return;
  }
  const Text* text = nullptr;
  if (bundle.primaryHost != nullptr && bundle.primaryHost->nodeType() == NodeType::Text) {
    text = static_cast<const Text*>(bundle.primaryHost);
  }
  if (chSet.fontSizeCh != nullptr) {
    float size = EvalFloatChannel(chSet.fontSizeCh, t, text != nullptr ? text->fontSize : 0.0f);
    AppendCssDecl(decls, "font-size", CssFloatToString(size) + "px");
  }
  if (chSet.letterSpacingCh != nullptr) {
    float spacing =
        EvalFloatChannel(chSet.letterSpacingCh, t, text != nullptr ? text->letterSpacing : 0.0f);
    AppendCssDecl(decls, "letter-spacing", CssFloatToString(spacing) + "px");
  }
}

// `flood-color` is a CSS property on <feFlood>, so a filter colour can ride on that element even
// though the pipeline's geometry attributes (dx, dy, stdDeviation) are not animatable.
void EmitFloodColorCssAtFrame(const BundleChannelSet& chSet, const std::string& tagName, Frame t,
                              std::string& decls) {
  // The tag index lower-cases element names.
  if (chSet.floodColorCh == nullptr || (tagName != "feFlood" && tagName != "feflood")) {
    return;
  }
  Color color = EvalColorChannel(chSet.floodColorCh, t, chSet.floodBaseColor);
  AppendCssDecl(decls, "flood-color", ColorToSVGHex(color));
  if (color.alpha < 1.0f) {
    AppendCssDecl(decls, "flood-opacity", CssFloatToString(color.alpha));
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
                            const std::string& tagName, Frame t, std::string& decls,
                            bool& needsZeroOrigin) {
  if (bundle.primaryHost == nullptr) {
    return;
  }
  if (chSet.roundCornerCh != nullptr && chSet.roundCornerTarget != nullptr &&
      chSet.roundCornerShape != nullptr) {
    // The radius reshapes the outline, so every stop rewrites the path with that frame's radius.
    // Geometry channels on the same shape feed the same rewrite: dropping them here would leave the
    // outline at its authored size while the reference grows.
    GeometryOverrides overrides = {};
    NodeType shapeType = chSet.roundCornerShape->nodeType();
    if (shapeType == NodeType::Rectangle) {
      auto* rect = static_cast<const Rectangle*>(chSet.roundCornerShape);
      auto size = rect->renderSize();
      if (chSet.geometryWidthCh != nullptr) {
        overrides.hasSize = true;
        overrides.width = EvalFloatChannel(chSet.geometryWidthCh, t, size.width);
        overrides.height = size.height;
      }
      if (chSet.roundnessCh != nullptr) {
        overrides.hasRoundness = true;
        overrides.roundness = EvalFloatChannel(chSet.roundnessCh, t, rect->roundness);
      }
    }
    PathData raw = PathDataFromSVGString("");
    GeoToPathData(chSet.roundCornerShape, shapeType, raw, &overrides);
    float radius = EvalFloatChannel(chSet.roundCornerCh, t, chSet.roundCornerTarget->radius);
    PathData rounded = PathDataFromSVGString("");
    ApplyRoundCorner(raw, radius, rounded);
    AppendCssDecl(decls, "d", "path(\"" + PathDataToSVGString(rounded) + "\")");
    return;
  }
  auto type = bundle.primaryHost->nodeType();
  if (type == NodeType::Polystar) {
    if (chSet.hasPolystarGeometry) {
      auto* star = static_cast<const Polystar*>(bundle.primaryHost);
      PolystarValues values = PolystarValuesFrom(star);
      // The runtime floors pointCount (5.1 and 5.9 draw the same polygon), so the frame floors it
      // too instead of building a partial vertex the reference never shows.
      values.pointCount =
          std::floor(EvalFloatChannel(chSet.starPointCountCh, t, values.pointCount));
      values.outerRadius = EvalFloatChannel(chSet.starOuterRadiusCh, t, values.outerRadius);
      values.innerRadius = EvalFloatChannel(chSet.starInnerRadiusCh, t, values.innerRadius);
      values.outerRoundness =
          EvalFloatChannel(chSet.starOuterRoundnessCh, t, values.outerRoundness);
      values.innerRoundness =
          EvalFloatChannel(chSet.starInnerRoundnessCh, t, values.innerRoundness);
      values.rotation = EvalFloatChannel(chSet.starRotationCh, t, values.rotation);
      // The position channel is authored in the element's own space, while the outline is built
      // from renderPosition() (which folds in the layout slot), so the frame applies the delta.
      values.centerX += EvalFloatChannel(chSet.starPosXCh, t, star->position.x) - star->position.x;
      values.centerY += EvalFloatChannel(chSet.starPosYCh, t, star->position.y) - star->position.y;
      // Position and rotation are a rigid move of the whole outline, so they ride on `transform`
      // and leave `d` untouched: two stops then describe a linear slide exactly, instead of the
      // sampled per-point path rewrite a baked outline would need.
      double centerX = values.centerX;
      double centerY = values.centerY;
      double rotation = values.rotation;
      values.centerX = 0.0f;
      values.centerY = 0.0f;
      values.rotation = 0.0f;
      std::string transform;
      if (!FloatNearlyZero(static_cast<float>(centerX)) ||
          !FloatNearlyZero(static_cast<float>(centerY))) {
        transform = "translate(" + CssFloatToString(static_cast<float>(centerX)) + "px, " +
                    CssFloatToString(static_cast<float>(centerY)) + "px)";
      }
      if (!FloatNearlyZero(static_cast<float>(rotation))) {
        if (!transform.empty()) {
          transform += ' ';
        }
        transform += "rotate(" + CssFloatToString(static_cast<float>(rotation)) + "deg)";
      }
      if (!transform.empty()) {
        AppendCssDecl(decls, "transform", transform);
        needsZeroOrigin = true;
      }
      std::string d = BuildPolystarPath(star, values);
      if (!d.empty()) {
        AppendCssDecl(decls, "d", "path(\"" + d + "\")");
      }
    }
    return;
  }
  if (type != NodeType::Rectangle && type != NodeType::Ellipse) {
    return;
  }
  if (chSet.roundCornerAsCss && type == NodeType::Rectangle && chSet.roundCornerTarget != nullptr) {
    auto* rect = static_cast<const Rectangle*>(bundle.primaryHost);
    auto size = rect->renderSize();
    float width = EvalFloatChannel(chSet.geometryWidthCh, t, size.width);
    // size.height has no channel binding yet; the clamp only needs the value used for layout.
    float height = size.height;
    float radius = EvalFloatChannel(chSet.roundCornerCh, t, chSet.roundCornerTarget->radius);
    float roundness = EvalFloatChannel(chSet.roundnessCh, t, rect->roundness);
    // PAGX rounds the corners by `radius` on top of the shape's own roundness, clamped to half the
    // shorter side; CSS border-radius takes the two as a single length.
    float corner = std::min(roundness + radius, std::min(width, height) * 0.5f);
    AppendCssDecl(decls, tagName == "rect" ? "rx" : "border-radius",
                  CssFloatToString(corner) + "px");
    if (tagName == "rect") {
      AppendCssDecl(decls, "ry", CssFloatToString(corner) + "px");
    }
    if (chSet.geometryWidthCh != nullptr) {
      AppendCssDecl(decls, tagName == "rect" ? "x" : "left",
                    CssFloatToString(rect->renderPosition().x - width * 0.5f) + "px");
      AppendCssDecl(decls, "width", CssFloatToString(width) + "px");
    }
    return;
  }
  Point center = {};
  float staticWidth = 0.0f;
  float staticHeight = 0.0f;
  if (type == NodeType::Rectangle) {
    auto* rect = static_cast<const Rectangle*>(bundle.primaryHost);
    center = rect->renderPosition();
    staticWidth = rect->renderSize().width;
    staticHeight = rect->renderSize().height;
    // A position channel is authored in the node's own space while renderPosition() folds in the
    // layout slot, so the frame applies the delta.
    center.x += EvalFloatChannel(chSet.geometryPosXCh, t, rect->position.x) - rect->position.x;
    center.y += EvalFloatChannel(chSet.geometryPosYCh, t, rect->position.y) - rect->position.y;
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
    staticHeight = ellipse->renderSize().height;
    center.x +=
        EvalFloatChannel(chSet.geometryPosXCh, t, ellipse->position.x) - ellipse->position.x;
    center.y +=
        EvalFloatChannel(chSet.geometryPosYCh, t, ellipse->position.y) - ellipse->position.y;
  }
  if (chSet.geometryWidthCh == nullptr && chSet.geometryHeightCh == nullptr &&
      chSet.geometryPosXCh == nullptr && chSet.geometryPosYCh == nullptr) {
    return;
  }
  float width = EvalFloatChannel(chSet.geometryWidthCh, t, staticWidth);
  float height = EvalFloatChannel(chSet.geometryHeightCh, t, staticHeight);
  if (tagName == "ellipse") {
    AppendCssDecl(decls, "cx", CssFloatToString(center.x) + "px");
    AppendCssDecl(decls, "cy", CssFloatToString(center.y) + "px");
    AppendCssDecl(decls, "rx", CssFloatToString(width * 0.5f) + "px");
    AppendCssDecl(decls, "ry", CssFloatToString(height * 0.5f) + "px");
    return;
  }
  // PAGX sizes shapes around their center. The CSS div/rect has a top-left origin, so the left/x
  // coordinate must move by half of the width delta to keep that center fixed.
  AppendCssDecl(decls, tagName == "rect" ? "x" : "left",
                CssFloatToString(center.x - width * 0.5f) + "px");
  AppendCssDecl(decls, tagName == "rect" ? "y" : "top",
                CssFloatToString(center.y - height * 0.5f) + "px");
  AppendCssDecl(decls, "width", CssFloatToString(width) + "px");
  AppendCssDecl(decls, "height", CssFloatToString(height) + "px");
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
  EmitGeometryCssAtFrame(bundle, chSet, tagName, t, decls, needsZeroOrigin);
  if ((chSet.trimEndCh != nullptr || chSet.trimStartCh != nullptr ||
       chSet.trimOffsetCh != nullptr) &&
      chSet.trimTarget != nullptr) {
    // The visible run is [start, end]; pathLength="1" keeps both in path fractions.
    float start = EvalFloatChannel(chSet.trimStartCh, t, chSet.trimTarget->start);
    float end = EvalFloatChannel(chSet.trimEndCh, t, chSet.trimTarget->end);
    float visibleLength = std::clamp(end - start, 0.0f, 1.0f);
    AppendCssDecl(decls, "stroke-dasharray",
                  CssFloatToString(visibleLength) + " " + CssFloatToString(1.0f - visibleLength));
    // The dash pattern starts at the path origin, so the shift places the visible run at `start`.
    // `offset` slides the whole run along the path and is authored in degrees (360 = one lap).
    float offsetFrac = EvalFloatChannel(chSet.trimOffsetCh, t, chSet.trimTarget->offset) / 360.0f;
    if (chSet.trimStartCh != nullptr || chSet.trimOffsetCh != nullptr || !FloatNearlyZero(start) ||
        !FloatNearlyZero(offsetFrac)) {
      AppendCssDecl(decls, "stroke-dashoffset", CssFloatToString(-(start + offsetFrac)));
    }
    // A zero-length dash still paints a dot with round caps. CSS visibility stays visible until
    // the end of a visible-to-hidden transition, so only the empty end frame is suppressed.
    AppendCssDecl(decls, "visibility", visibleLength == 0.0f ? "hidden" : "visible");
  }
  EmitPaintCssAtFrame(chSet, tagName, t, decls);
  EmitTextCssAtFrame(bundle, chSet, t, decls);
  EmitFilterCssAtFrame(bundle, chSet, t, decls);
  EmitFloodColorCssAtFrame(chSet, tagName, t, decls);

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

// Two targets can share one DOM tag: a RoundCorner points its animation at the shape it rounds, so
// the shape's own geometry channels arrive as a second bundle with the same id. Merging them keeps
// one channel set per tag, which is what lets the geometry emitter rebuild `d` from both the shape
// channels and the modifier.
void MergeBundlesByDomId(std::vector<DomTargetBundle>& bundles) {
  std::vector<DomTargetBundle> merged = {};
  for (auto& bundle : bundles) {
    auto same = std::find_if(merged.begin(), merged.end(), [&bundle](const DomTargetBundle& other) {
      return !other.domId.empty() && other.domId == bundle.domId;
    });
    if (same == merged.end()) {
      merged.push_back(std::move(bundle));
      continue;
    }
    same->objects.insert(same->objects.end(), bundle.objects.begin(), bundle.objects.end());
    if (same->primaryHost == nullptr || same->primaryHost->nodeType() == NodeType::RoundCorner) {
      // The geometry node is the one whose tag is being rewritten.
      same->primaryHost = bundle.primaryHost;
    }
  }
  bundles = std::move(merged);
}

std::vector<DomTargetBundle> GroupObjectsIntoDomBundles(
    PAGXDocument& doc, const Animation* anim, HTMLWriterContext& ctx,
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
    if (nType == NodeType::InnerShadowFilter || nType == NodeType::BlendFilter) {
      auto idIt = ctx.animation.elementIds.find(targetNode);
      if (idIt != ctx.animation.elementIds.end()) {
        DomTargetBundle floodBundle;
        floodBundle.domId = idIt->second;
        floodBundle.primaryHost = targetNode;
        floodBundle.objects.push_back(obj);
        bundles.push_back(std::move(floodBundle));
        continue;
      }
    }
    if (nType == NodeType::Text) {
      auto originIt = ctx.animation.textOrigins.find(targetNode);
      if (originIt != ctx.animation.textOrigins.end()) {
        DomTargetBundle textBundle;
        textBundle.domId = targetNode->id;
        textBundle.primaryHost = targetNode;
        textBundle.objects.push_back(obj);
        textBundle.hasTextOrigin = true;
        textBundle.textOriginX = originIt->second.first;
        textBundle.textOriginY = originIt->second.second;
        bundles.push_back(std::move(textBundle));
        continue;
      }
    }
    if (nType == NodeType::RangeSelector) {
      const auto& glyphs = ctx.animation.rangeSelectorGlyphs;
      for (size_t index = 0; index < glyphs.size(); ++index) {
        if (glyphs[index].selector != targetNode) {
          continue;
        }
        DomTargetBundle glyphBundle;
        glyphBundle.domId = glyphs[index].id;
        glyphBundle.primaryHost = targetNode;
        glyphBundle.objects.push_back(obj);
        glyphBundle.rangeGlyphIndex = static_cast<int>(index);
        bundles.push_back(std::move(glyphBundle));
      }
      continue;
    }
    if (nType == NodeType::Repeater) {
      for (const auto& copy : ctx.animation.repeaterCopies) {
        if (copy.repeater != targetNode) {
          continue;
        }
        DomTargetBundle copyBundle;
        copyBundle.domId = copy.id;
        copyBundle.primaryHost = targetNode;
        copyBundle.objects.push_back(obj);
        copyBundle.repeaterIndex = copy.index;
        bundles.push_back(std::move(copyBundle));
      }
      continue;
    }
    if (nType == NodeType::Layer) {
      domId = targetNode->id;
    } else if (nType == NodeType::Group || nType == NodeType::TextBox || nType == NodeType::Text ||
               nType == NodeType::Fill || nType == NodeType::Stroke ||
               nType == NodeType::Rectangle || nType == NodeType::Ellipse ||
               nType == NodeType::Polystar) {
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
    } else if (nType == NodeType::TrimPath || nType == NodeType::RoundCorner) {
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

// True when every key of a float channel interpolates linearly or holds. Between two keys such a
// channel is then linear in time, which keeps a RangeSelector factor rational in time.
bool IsLinearOrHoldFloatChannel(const Channel* ch) {
  if (ch == nullptr || ch->valueType() != ChannelValueType::Float) {
    return false;
  }
  for (const auto& key : static_cast<const TypedChannel<float>*>(ch)->keyframes) {
    if (key.interpolation == KeyframeInterpolationType::Bezier) {
      return false;
    }
  }
  return true;
}

// Value of a linear / hold float channel at a fractional frame. A glyph's factor crosses the
// selector edges between keys, so its stops do not land on whole frames.
double EvalLinearFloatAt(const Channel* ch, double t, double fallback) {
  if (ch == nullptr || ch->valueType() != ChannelValueType::Float) {
    return fallback;
  }
  const auto& keys = static_cast<const TypedChannel<float>*>(ch)->keyframes;
  if (keys.empty()) {
    return fallback;
  }
  if (t <= static_cast<double>(keys.front().time)) {
    return keys.front().value;
  }
  for (size_t k = 0; k + 1 < keys.size(); ++k) {
    double t1 = static_cast<double>(keys[k + 1].time);
    if (t >= t1) {
      continue;
    }
    if (keys[k].interpolation != KeyframeInterpolationType::Linear) {
      return keys[k].value;
    }
    double t0 = static_cast<double>(keys[k].time);
    double u = t1 > t0 ? (t - t0) / (t1 - t0) : 1.0;
    return keys[k].value + (keys[k + 1].value - keys[k].value) * u;
  }
  return keys.back().value;
}

using RangeSelectorGlyph = HTMLWriterContext::AnimationState::RangeSelectorGlyph;

struct RangeValues {
  double start = 0.0;
  double end = 0.0;
  double offset = 0.0;
};

RangeValues RangeValuesAt(const BundleChannelSet& chSet, double t) {
  const RangeSelector* selector = chSet.rangeGlyph->selector;
  return {EvalLinearFloatAt(chSet.rangeStartCh, t, selector->start),
          EvalLinearFloatAt(chSet.rangeEndCh, t, selector->end),
          EvalLinearFloatAt(chSet.rangeOffsetCh, t, selector->offset)};
}

// The glyph's combined factor at time t, clamped exactly like the static export.
double RangeGlyphFactorAt(const BundleChannelSet& chSet, double t) {
  const RangeSelectorGlyph* glyph = chSet.rangeGlyph;
  RangeValues values = RangeValuesAt(chSet, t);
  float factor = ComputeRangeSelectorFactor(
      glyph->selector, glyph->glyphIndex, glyph->totalGlyphs, static_cast<float>(values.start),
      static_cast<float>(values.end), static_cast<float>(values.offset));
  return std::clamp(static_cast<double>(factor), -1.0, 1.0);
}

constexpr int kRangeGlyphEdgeCount = 7;

// The selector range edges against the glyph's own edges, as signed distances in text fractions.
// The factor only changes formula where one of them changes sign, and between keys each of them is
// linear in time, so their zeros are the only interior breakpoints.
void RangeGlyphEdgeDistances(const BundleChannelSet& chSet, double t,
                             double out[kRangeGlyphEdgeCount]) {
  const RangeSelectorGlyph* glyph = chSet.rangeGlyph;
  RangeValues values = RangeValuesAt(chSet, t);
  double count = static_cast<double>(glyph->totalGlyphs);
  double scale = glyph->selector->unit == SelectorUnit::Index ? 1.0 / count : 1.0;
  double rangeStart = (values.start + values.offset) * scale;
  double rangeEnd = (values.end + values.offset) * scale;
  double glyphStart = static_cast<double>(glyph->glyphIndex) / count;
  double glyphEnd = static_cast<double>(glyph->glyphIndex + 1) / count;
  double glyphCenter = (glyphStart + glyphEnd) * 0.5;
  out[0] = rangeStart - glyphStart;
  out[1] = rangeStart - glyphCenter;
  out[2] = rangeStart - glyphEnd;
  out[3] = rangeEnd - glyphStart;
  out[4] = rangeEnd - glyphCenter;
  out[5] = rangeEnd - glyphEnd;
  out[6] = rangeStart - rangeEnd;
}

// Which edge distances change the factor formula for a shape. A ramp is (center - start) /
// (end - start) clamped to [0, 1], so it only switches where a range edge passes the glyph center;
// Square measures the overlap of the glyph span, so it switches at the glyph's start and end.
bool RangeGlyphEdgeMatters(SelectorShape shape, int edge) {
  if (edge == 6) {
    return true;
  }
  if (shape == SelectorShape::Square) {
    return edge == 0 || edge == 2 || edge == 3 || edge == 5;
  }
  return edge == 1 || edge == 4;
}

// y of a CSS cubic-bezier(x1, y1, x2, y2) at progress x. x(s) is monotonic for x1, x2 in [0, 1],
// so Newton steps are kept inside a shrinking bisection bracket.
double CubicBezierEase(double x1, double y1, double x2, double y2, double x) {
  double lo = 0.0;
  double hi = 1.0;
  double s = x;
  for (int i = 0; i < 32; ++i) {
    double ms = 1.0 - s;
    double xs = 3.0 * ms * ms * s * x1 + 3.0 * ms * s * s * x2 + s * s * s - x;
    if (std::abs(xs) < 1e-10) {
      break;
    }
    if (xs > 0.0) {
      hi = s;
    } else {
      lo = s;
    }
    double dx = 3.0 * ms * ms * x1 + 6.0 * ms * s * (x2 - x1) + 3.0 * s * s * (1.0 - x2);
    double next = std::abs(dx) > 1e-9 ? s - xs / dx : (lo + hi) * 0.5;
    s = (next <= lo || next >= hi) ? (lo + hi) * 0.5 : next;
  }
  double ms = 1.0 - s;
  return 3.0 * ms * ms * s * y1 + 3.0 * ms * s * s * y2 + s * s * s;
}

struct CubicBezierCurve {
  double x1 = 0.0;
  double y1 = 0.0;
  double x2 = 1.0;
  double y2 = 1.0;
};

constexpr int kEaseSamples = 31;

// A monotonic rational piece normalized to [0, 1] on both axes is u / (u + r * (1 - u)): its slope
// is 1 / r at the start and r at the end. The easing keeps those two slopes and only searches the
// two handle lengths, a small deterministic 2D search.
double RationalEaseError(double r, double a, double b) {
  double worst = 0.0;
  for (int i = 1; i <= kEaseSamples; ++i) {
    double u = static_cast<double>(i) / (kEaseSamples + 1);
    double target = u / (u + r * (1.0 - u));
    worst = std::max(worst, std::abs(CubicBezierEase(a, a / r, 1.0 - b, 1.0 - b * r, u) - target));
  }
  return worst;
}

CubicBezierCurve FitRationalEase(double r) {
  double bestA = 1.0 / 3.0;
  double bestB = 1.0 / 3.0;
  double bestError = RationalEaseError(r, bestA, bestB);
  for (int i = 1; i <= 20; ++i) {
    for (int j = 1; j <= 20; ++j) {
      double a = i / 20.0;
      double b = j / 20.0;
      double error = RationalEaseError(r, a, b);
      if (error < bestError) {
        bestError = error;
        bestA = a;
        bestB = b;
      }
    }
  }
  const double moves[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  for (double step = 0.025; step > 1e-4; step *= 0.5) {
    bool improved = true;
    while (improved) {
      improved = false;
      for (const auto& move : moves) {
        double a = bestA + move[0] * step;
        double b = bestB + move[1] * step;
        if (a <= 0.0 || a > 1.0 || b <= 0.0 || b > 1.0) {
          continue;
        }
        double error = RationalEaseError(r, a, b);
        if (error < bestError) {
          bestError = error;
          bestA = a;
          bestB = b;
          improved = true;
        }
      }
    }
  }
  return {bestA, bestA / r, 1.0 - bestB, 1.0 - bestB * r};
}

struct RangeGlyphStop {
  double time = 0.0;
  double factor = 0.0;
  std::string easing = {};
};

// Turns a pixel (and opacity) tolerance into a factor tolerance from the largest change any emitted
// value makes per unit of factor.
double RangeGlyphFactorTolerance(const RangeSelectorGlyph& glyph) {
  constexpr double kPixelTolerance = 0.03;
  constexpr double kAlphaTolerance = 0.004;
  constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;
  const TextModifier* modifier = glyph.modifier;
  double pixels = std::max({std::abs(modifier->position.x), std::abs(modifier->position.y),
                            std::abs(modifier->anchor.x), std::abs(modifier->anchor.y)});
  pixels += std::abs(modifier->rotation) * kDegreesToRadians * glyph.fontSize;
  pixels += std::max(std::abs(modifier->scale.x - 1.0f), std::abs(modifier->scale.y - 1.0f)) *
            glyph.fontSize;
  pixels += std::abs(modifier->skew) * kDegreesToRadians * glyph.fontSize;
  double tolerance = pixels > 0.0 ? kPixelTolerance / pixels : 1.0;
  if (modifier->alpha < 1.0f) {
    tolerance = std::min(tolerance, kAlphaTolerance / (1.0 - modifier->alpha));
  }
  return tolerance;
}

std::string FormatCubicBezier(const CubicBezierCurve& curve) {
  return "cubic-bezier(" + CssFloatToString(static_cast<float>(curve.x1)) + ", " +
         CssFloatToString(static_cast<float>(curve.y1)) + ", " +
         CssFloatToString(static_cast<float>(curve.x2)) + ", " +
         CssFloatToString(static_cast<float>(curve.y2)) + ")";
}

// Appends the stops covering [t0, t1]. A monotonic rational piece is one stop eased by a fitted
// cubic-bezier; a piece the fit cannot reach within the tolerance is split in half.
void AppendRangeGlyphPiece(const BundleChannelSet& chSet, double t0, double t1, double tolerance,
                           int depth, std::vector<RangeGlyphStop>& stops) {
  constexpr int kMaxDepth = 6;
  double span = t1 - t0;
  double f0 = RangeGlyphFactorAt(chSet, t0);
  // A hold key jumps at t1: the value just before it is what this piece shows.
  double f1 = RangeGlyphFactorAt(chSet, t1 - span * 1e-6);
  if (std::abs(f1 - f0) < 1e-9) {
    double after = RangeGlyphFactorAt(chSet, t1);
    stops.push_back({t0, f0, std::abs(after - f1) > 1e-9 ? "steps(1, end)" : "linear"});
    return;
  }
  double h = (RangeGlyphFactorAt(chSet, t0 + span * 0.5) - f0) / (f1 - f0);
  if (h > 1e-6 && h < 1.0 - 1e-6) {
    double r = 1.0 / h - 1.0;
    bool linear = std::abs(r - 1.0) < 1e-6;
    CubicBezierCurve curve = {1.0 / 3.0, 1.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0};
    if (!linear) {
      curve = FitRationalEase(r);
    }
    double worst = 0.0;
    for (int i = 1; i <= 16; ++i) {
      double u = i / 17.0;
      double eased = CubicBezierEase(curve.x1, curve.y1, curve.x2, curve.y2, u);
      double actual = RangeGlyphFactorAt(chSet, t0 + span * u);
      worst = std::max(worst, std::abs(f0 + (f1 - f0) * eased - actual));
    }
    if (worst <= tolerance || depth >= kMaxDepth) {
      stops.push_back({t0, f0, linear ? "linear" : FormatCubicBezier(curve)});
      return;
    }
  }
  if (depth >= kMaxDepth) {
    stops.push_back({t0, f0, "linear"});
    return;
  }
  double split = t0 + span * 0.5;
  AppendRangeGlyphPiece(chSet, t0, split, tolerance, depth + 1, stops);
  AppendRangeGlyphPiece(chSet, split, t1, tolerance, depth + 1, stops);
}

// The glyph's style for one factor value. The function list depends only on which modifier
// properties are set, never on the factor, and every argument is linear in the factor, so CSS
// interpolates exactly what the runtime does and the easing is the only curve to reproduce.
std::string RangeGlyphDecls(const RangeSelectorGlyph& glyph, double factor) {
  const TextModifier* modifier = glyph.modifier;
  float f = static_cast<float>(factor);
  std::string transform = "translate(" + CssFloatToString(glyph.glyphX) + "px, " +
                          CssFloatToString(glyph.glyphY) + "px)";
  if (!FloatNearlyZero(modifier->position.x) || !FloatNearlyZero(modifier->position.y)) {
    transform += " translate(" + CssFloatToString(modifier->position.x * f) + "px, " +
                 CssFloatToString(modifier->position.y * f) + "px)";
  }
  bool hasRotation = !FloatNearlyZero(modifier->rotation);
  bool hasScale =
      !FloatNearlyZero(modifier->scale.x - 1.0f) || !FloatNearlyZero(modifier->scale.y - 1.0f);
  bool hasSkew = !FloatNearlyZero(modifier->skew);
  if (hasRotation || hasScale || hasSkew) {
    float pivotX = glyph.pivotX + modifier->anchor.x * f;
    float pivotY = modifier->anchor.y * f;
    transform +=
        " translate(" + CssFloatToString(pivotX) + "px, " + CssFloatToString(pivotY) + "px)";
    if (hasRotation) {
      transform += " rotate(" + CssFloatToString(modifier->rotation * f) + "deg)";
    }
    if (hasScale) {
      transform += " scale(" + CssFloatToString(1.0f + (modifier->scale.x - 1.0f) * f) + ", " +
                   CssFloatToString(1.0f + (modifier->scale.y - 1.0f) * f) + ")";
    }
    if (hasSkew) {
      bool hasAxis = !FloatNearlyZero(modifier->skewAxis);
      if (hasAxis) {
        transform += " rotate(" + CssFloatToString(modifier->skewAxis) + "deg)";
      }
      transform += " skewX(" + CssFloatToString(-modifier->skew * f) + "deg)";
      if (hasAxis) {
        transform += " rotate(" + CssFloatToString(-modifier->skewAxis) + "deg)";
      }
    }
    transform +=
        " translate(" + CssFloatToString(-pivotX) + "px, " + CssFloatToString(-pivotY) + "px)";
  }
  std::string decls = {};
  AppendCssDecl(decls, "transform", transform);
  if (modifier->alpha < 1.0f) {
    AppendCssDecl(decls, "opacity", CssFloatToString(1.0f + (modifier->alpha - 1.0f) * f));
  }
  return decls;
}

// Stops for one glyph of an animated RangeSelector. The timeline is cut at every key and wherever a
// range edge crosses a glyph edge; inside each cut the factor is one rational function of time, so
// it is a stop pair eased by a fitted cubic-bezier rather than a per-frame sample.
std::string EmitRangeSelectorGlyphStops(const BundleChannelSet& chSet,
                                        const BundleTimingInfo& timing) {
  if (timing.effectiveDuration <= 0) {
    return {};
  }
  const double windowStart = static_cast<double>(timing.bundleShiftFrames);
  const double windowEnd = windowStart + static_cast<double>(timing.effectiveDuration);
  std::set<double> cuts = {windowStart, windowEnd};
  for (Frame key : chSet.keyTimes) {
    double t = static_cast<double>(key);
    if (t > windowStart && t < windowEnd) {
      cuts.insert(t);
    }
  }
  const std::vector<double> keyCuts(cuts.begin(), cuts.end());
  for (size_t k = 0; k + 1 < keyCuts.size(); ++k) {
    double t0 = keyCuts[k];
    double t1 = keyCuts[k + 1];
    double d0[kRangeGlyphEdgeCount];
    double d1[kRangeGlyphEdgeCount];
    RangeGlyphEdgeDistances(chSet, t0, d0);
    RangeGlyphEdgeDistances(chSet, t1 - (t1 - t0) * 1e-9, d1);
    for (int e = 0; e < kRangeGlyphEdgeCount; ++e) {
      if (!RangeGlyphEdgeMatters(chSet.rangeGlyph->selector->shape, e)) {
        continue;
      }
      if ((d0[e] < 0.0 && d1[e] > 0.0) || (d0[e] > 0.0 && d1[e] < 0.0)) {
        cuts.insert(t0 + (t1 - t0) * d0[e] / (d0[e] - d1[e]));
      }
    }
  }
  const std::vector<double> times(cuts.begin(), cuts.end());
  const double tolerance = RangeGlyphFactorTolerance(*chSet.rangeGlyph);
  std::vector<RangeGlyphStop> pieces = {};
  for (size_t k = 0; k + 1 < times.size(); ++k) {
    if (times[k + 1] - times[k] < 1e-6) {
      continue;
    }
    AppendRangeGlyphPiece(chSet, times[k], times[k + 1], tolerance, 0, pieces);
  }
  pieces.push_back({windowEnd, RangeGlyphFactorAt(chSet, windowEnd), {}});
  // A stop whose value equals both neighbours only repeats a flat stretch.
  std::vector<RangeGlyphStop> kept = {};
  for (size_t k = 0; k < pieces.size(); ++k) {
    bool flatInterior = k > 0 && k + 1 < pieces.size() &&
                        std::abs(pieces[k].factor - kept.back().factor) < 1e-9 &&
                        std::abs(pieces[k].factor - pieces[k + 1].factor) < 1e-9;
    if (flatInterior) {
      kept.back().easing = "linear";
      continue;
    }
    kept.push_back(pieces[k]);
  }
  pieces = std::move(kept);

  std::string stops = {};
  const double duration = static_cast<double>(timing.effectiveDuration);
  for (const auto& piece : pieces) {
    double percent = std::clamp((piece.time - windowStart) * 100.0 / duration, 0.0, 100.0);
    std::string decls = RangeGlyphDecls(*chSet.rangeGlyph, piece.factor);
    if (!piece.easing.empty()) {
      AppendCssDecl(decls, "animation-timing-function", piece.easing);
    }
    stops += "  " + CssFloatToString(static_cast<float>(percent)) + "% { " + decls + "; }\n";
  }
  return stops;
}

std::string EmitBundleKeyframeStops(const DomTargetBundle& bundle, const BundleChannelSet& chSet,
                                    const BundleTimingInfo& timing, const std::string& tagName,
                                    bool& outNeedsZeroOrigin) {
  if (chSet.rangeGlyph != nullptr) {
    return EmitRangeSelectorGlyphStops(chSet, timing);
  }
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
// Collects the shapes a RoundCorner rounds: every shape declared before it in the same element list.
// A nested Group opens its own scope, so the scan only looks at direct children.
bool CollectShapesBeforeRoundCorner(const std::vector<Element*>& elements,
                                    const RoundCorner* target,
                                    std::vector<const Element*>& shapes) {
  for (auto* element : elements) {
    if (element == target) {
      return true;
    }
    if (element == nullptr) {
      continue;
    }
    auto type = element->nodeType();
    if (type == NodeType::Rectangle || type == NodeType::Ellipse || type == NodeType::Path ||
        type == NodeType::Polystar) {
      shapes.push_back(element);
    }
  }
  return false;
}

bool FindRoundCornerShapesInElements(const std::vector<Element*>& elements,
                                     const RoundCorner* target,
                                     std::vector<const Element*>& shapes) {
  for (auto* element : elements) {
    if (element == nullptr || element->nodeType() != NodeType::Group) {
      continue;
    }
    auto* group = static_cast<const Group*>(element);
    // Collect into a scratch list: a container that does not own the modifier must not contribute
    // its shapes to the result.
    std::vector<const Element*> groupShapes = {};
    if (CollectShapesBeforeRoundCorner(group->elements, target, groupShapes)) {
      shapes.insert(shapes.end(), groupShapes.begin(), groupShapes.end());
      return true;
    }
    if (FindRoundCornerShapesInElements(group->elements, target, shapes)) {
      return true;
    }
  }
  return false;
}

bool FindRoundCornerShapes(const std::vector<Layer*>& layers, const RoundCorner* target,
                           std::vector<const Element*>& shapes) {
  for (auto* layer : layers) {
    if (layer == nullptr) {
      continue;
    }
    std::vector<const Element*> layerShapes = {};
    if (CollectShapesBeforeRoundCorner(layer->contents, target, layerShapes)) {
      shapes.insert(shapes.end(), layerShapes.begin(), layerShapes.end());
      return true;
    }
    if (FindRoundCornerShapesInElements(layer->contents, target, shapes)) {
      return true;
    }
    if (layer->composition != nullptr &&
        FindRoundCornerShapes(layer->composition->layers, target, shapes)) {
      return true;
    }
    if (FindRoundCornerShapes(layer->children, target, shapes)) {
      return true;
    }
  }
  return false;
}

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
      if (nType == NodeType::InnerShadowFilter || nType == NodeType::BlendFilter) {
        for (auto* ch : obj->channels) {
          if (ch != nullptr &&
              LookupChannelRole(nType, ch->name) == TargetChannelRole::FilterFloodColor) {
            // The filter def is written while the body is emitted, so decide the id here, before it;
            // the def emitter picks it up from this map.
            std::string floodId = _ctx.nextId("flood");
            _ctx.animation.filterFloodIds[targetNode] = floodId;
            _ctx.animation.elementIds[targetNode] = floodId;
          }
        }
      }
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
          // A pose channel makes the frame emit a full transform list that also carries the
          // authored x/y, so the tag drops its left/top. A pure x/y or matrix animation keeps the
          // previous split (left/top plus a delta translate), so those roles are excluded.
          if (role == TargetChannelRole::Rotation || role == TargetChannelRole::ScaleX ||
              role == TargetChannelRole::ScaleY || role == TargetChannelRole::LayerSkewX ||
              role == TargetChannelRole::LayerSkewY || role == TargetChannelRole::AnchorX ||
              role == TargetChannelRole::AnchorY || role == TargetChannelRole::AnchorZ ||
              role == TargetChannelRole::LayerRotationX ||
              role == TargetChannelRole::LayerRotationY || role == TargetChannelRole::LayerZ ||
              role == TargetChannelRole::LayerPerspective ||
              role == TargetChannelRole::LayerMatrix3D) {
            _ctx.animation.poseDrivenLayers.insert(layer);
          }
          if (role == TargetChannelRole::LayerMatrix) {
            // The runtime lets a matrix channel own the translation, so the tag must not carry the
            // layer's x/y as well; see LayerPositionForCSS.
            _ctx.animation.matrixDrivenLayers.insert(layer);
          }
          if (role == TargetChannelRole::LayerVisibility &&
              ch->valueType() == ChannelValueType::Bool) {
            _ctx.animation.visibleLayers.insert(layer);
          }
          if (role == TargetChannelRole::Opacity && !layer->groupOpacity) {
            _restoredGroupOpacity.emplace_back(layer, layer->groupOpacity);
            layer->groupOpacity = true;
          }
        }
      } else if (nType == NodeType::RangeSelector) {
        auto* selector = static_cast<RangeSelector*>(targetNode);
        bool bound = false;
        bool exact = true;
        for (auto* ch : obj->channels) {
          if (ch == nullptr) {
            continue;
          }
          auto role = LookupChannelRole(nType, ch->name);
          if (role == TargetChannelRole::RangeSelectorStart ||
              role == TargetChannelRole::RangeSelectorEnd ||
              role == TargetChannelRole::RangeSelectorOffset) {
            bound = true;
            exact = exact && IsLinearOrHoldFloatChannel(ch);
          }
        }
        // Eased keys would compose the bezier with the rational factor curve, which one fitted
        // easing per piece cannot reproduce; such a selector keeps its static pose.
        if (bound && exact) {
          _ctx.animation.animatedRangeSelectors.insert(selector);
          if (selector->id.empty()) {
            _restoredNodeIds.emplace_back(selector, selector->id);
            selector->id = _ctx.nextId("animRS");
          }
        }
      } else if (nType == NodeType::Repeater) {
        auto* repeater = static_cast<Repeater*>(targetNode);
        for (auto* ch : obj->channels) {
          if (ch == nullptr) {
            continue;
          }
          auto repeaterRole = LookupChannelRole(nType, ch->name);
          if (repeaterRole == TargetChannelRole::RepeaterRotation ||
              repeaterRole == TargetChannelRole::RepeaterCopies ||
              repeaterRole == TargetChannelRole::RepeaterStartAlpha ||
              repeaterRole == TargetChannelRole::RepeaterPositionX) {
            // The repeater itself renders nothing: its copies do. Record the node so writeRepeater
            // hands every copy an id, and keep the base id stable across mounts.
            _ctx.animation.animatedRepeaters.insert(repeater);
            if (repeater->id.empty()) {
              _restoredNodeIds.emplace_back(repeater, repeater->id);
              repeater->id = _ctx.nextId("animRep");
            }
            if (repeaterRole == TargetChannelRole::RepeaterCopies &&
                ch->valueType() == ChannelValueType::Float) {
              float maxCopies = repeater->copies;
              for (const auto& key : static_cast<const TypedChannel<float>*>(ch)->keyframes) {
                maxCopies = std::max(maxCopies, key.value);
              }
              int emitted = static_cast<int>(std::ceil(std::max(maxCopies, 0.0f)));
              auto& slot = _ctx.animation.repeaterMaxCopies[repeater];
              slot = std::max(slot, emitted);
            }
          }
        }
      } else if (nType == NodeType::Rectangle || nType == NodeType::Ellipse ||
                 nType == NodeType::Polystar) {
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
        // A CSS dash animation models a separate trim on a single path; `start` and `end` keep the
        // authored constraints, while an animated `offset` is exactly what makes the shift move, so
        // it may be non-zero here.
        if (trim->type == TrimType::Separate && (trim->offset == 0.0f || trim->start == 0.0f)) {
          for (auto* channel : obj->channels) {
            auto role = channel != nullptr ? LookupChannelRole(nType, channel->name)
                                           : TargetChannelRole::Unknown;
            if (role == TargetChannelRole::TrimEnd || role == TargetChannelRole::TrimStart ||
                role == TargetChannelRole::TrimOffset) {
              _ctx.animation.elementIds[trim] =
                  trim->id.empty() ? _ctx.nextId("animTrim") : trim->id;
              break;
            }
          }
        }
      } else if (nType == NodeType::RoundCorner) {
        auto* roundCorner = static_cast<RoundCorner*>(targetNode);
        std::vector<const Element*> shapes = {};
        if (FindRoundCornerShapes(_doc.layers, roundCorner, shapes) && shapes.size() == 1) {
          // The rounded geometry has to be emitted as an inline <path> the keyframes can rewrite,
          // so it claims an id like an animated geometry node.
          const Element* shape = shapes.front();
          // Prefer the modifier's own id so the rewritten <path> is recognisable in the output.
          std::string id = !roundCorner->id.empty()
                               ? roundCorner->id
                               : (!shape->id.empty() ? shape->id : _ctx.nextId("animRC"));
          _ctx.animation.roundCornerShapes[roundCorner] = shape;
          _ctx.animation.elementIds[shape] = id;
          // The bundle is keyed by the modifier, which renders no tag of its own: route it to the
          // shape's tag through the same id.
          _ctx.animation.elementIds[roundCorner] = id;
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
      } else if (nType == NodeType::Text) {
        auto* text = static_cast<Text*>(targetNode);
        // A Text renders as a span that can carry font-size / letter-spacing keyframes. The id is
        // not restored afterwards: it only has to be unique within this export.
        _ctx.animation.elementIds[text] = !text->id.empty() ? text->id : _ctx.nextId("animT");
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

std::vector<HTMLAnimationSession::AnimationMount> HTMLAnimationSession::CollectAnimationMounts()
    const {
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
    MergeBundlesByDomId(bundles);
    for (const auto& bundle : bundles) {
      auto elements =
          FindBundleElementsForNode(tagIndex.nameById, tagIndexByNode, bundle, mount.scope);
      if (elements.empty()) {
        continue;
      }
      BundleChannelSet chSet =
          CollectBundleChannels(_doc, bundle, _solidToPainter, _maskPathToLayer);
      if (bundle.rangeGlyphIndex >= 0) {
        chSet.rangeGlyph = &_ctx.animation.rangeSelectorGlyphs[bundle.rangeGlyphIndex];
      }
      if (chSet.roundCornerTarget != nullptr) {
        auto shapeIt = _ctx.animation.roundCornerShapes.find(chSet.roundCornerTarget);
        if (shapeIt != _ctx.animation.roundCornerShapes.end()) {
          if (shapeIt->second->nodeType() == NodeType::Rectangle) {
            // border-radius runs on the same div as left/width, so a geometry channel can animate
            // next to it; the path rewrite below cannot interpolate when the command list changes.
            chSet.roundCornerAsCss = true;
          } else {
            chSet.roundCornerShape = shapeIt->second;
          }
        }
      }
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
      if (chSet.strokeWidthCh != nullptr || chSet.geometryWidthCh != nullptr ||
          chSet.hasPolystarGeometry) {
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
    textEdits.push_back(
        {tagStart, tagEnd,
         BuildWovenTag(bodyHtml.substr(tagStart, tagEnd - tagStart), combined, needsZeroOrigin)});
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
