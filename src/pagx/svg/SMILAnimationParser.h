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
#include <unordered_map>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Keyframe.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LayerFilter.h"
#include "pagx/nodes/Node.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/Point.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

class SVGParserContext;

/**
 * Bezier control points parsed from an SVG SMIL keySplines value. Each entry describes one
 * interpolation segment between adjacent keyframes: P1 is the outgoing handle of the start
 * keyframe and P2 is the incoming handle of the end keyframe. P0 is implicitly (0,0) and
 * P3 is implicitly (1,1).
 */
struct BezierControlPoints {
  double p1x = 0.0;
  double p1y = 0.0;
  double p2x = 1.0;
  double p2y = 1.0;
};

/**
 * SMIL animation elements collected for a single SVG graphics element, grouped by tag name so
 * the parser can dispatch each kind to its dedicated handler.
 */
struct SMILAnimationGroup {
  std::vector<std::shared_ptr<DOMNode>> animates = {};
  std::vector<std::shared_ptr<DOMNode>> animateTransforms = {};
  std::vector<std::shared_ptr<DOMNode>> animateMotions = {};
  std::vector<std::shared_ptr<DOMNode>> sets = {};
};

/**
 * Information about a PAGX node that is the target of one or more SMIL animations, populated
 * during convertToLayer and consumed by buildAnimation to create AnimationObject + Channel nodes.
 *
 * Target routing: a single SVG element (e.g. <rect>) may carry <animate> elements that drive
 * different PAGX nodes. For example, <animate attributeName="opacity"> drives the Layer's alpha
 * channel, while <animate attributeName="stroke-dashoffset"> drives a Stroke content node's
 * dashOffset channel. PAGX requires each AnimationObject to target exactly one node, so
 * buildAnimation groups channels by their resolved targetId and creates one AnimationObject per
 * target. The targetId is resolved by resolveAnimateTarget, which maps the SVG attributeName to
 * the correct PAGX node (Layer, Fill, Stroke, Ellipse, or Rectangle) and allocates an id for
 * content nodes that don't already have one.
 *
 * Transform animations (<animateTransform>, <animateMotion>) bake Matrix keyframes onto the
 * Layer's runtime "matrix" channel, so they target the Layer node itself (targetId) — no Group
 * host node is created.
 */
struct AnimatedNodeInfo {
  Layer* targetLayer = nullptr;
  std::string targetId = {};

  // Filter node targeted by <animate> on fe* primitives inside an SVG <filter>. Populated by
  // registerAnimatedElement when the element is a feGaussianBlur/feOffset recorded in
  // _feToFilterMap. Null for shape/<g> elements whose animations target Layer/content nodes.
  LayerFilter* targetFilter = nullptr;

  std::unordered_map<Node*, std::string> contentNodeIds = {};

  std::shared_ptr<DOMNode> domElement = nullptr;
};

/**
 * Resolved target for an <animate>/<set> element: the PAGX node to drive, the channel name that
 * the runtime Writer expects, and the value type used to instantiate the correct TypedChannel<T>.
 *
 * The mapping from SVG attributeName to PAGX (node, channel) is not always 1:1 with the Layer:
 * - opacity → Layer.alpha
 * - fill / fill-opacity → Fill.color / Fill.alpha
 * - stroke / stroke-width / stroke-dashoffset / … → Stroke.color / Stroke.width / Stroke.dashOffset / …
 * - cx / cy / r / rx / ry → Ellipse.position.x / position.y / size.width / size.height
 * - x / y / width / height / rx / ry → Rectangle.position / size / roundness
 * - display / visibility → Layer.visible
 * When the target is a Fill/Stroke/Shape content node, resolveAnimateTarget allocates an id for
 * it (via ensureContentNodeId) so the AnimationObject can reference it by id.
 */
struct ChannelTarget {
  Node* node = nullptr;
  std::string nodeId = {};
  std::string channelName = {};
  ChannelValueType valueType = ChannelValueType::Float;
};

/**
 * SMILAnimationParser converts SVG SMIL animation elements (<animate>, <animateTransform>,
 * <animateMotion>, <set>) into PAGX Animation/AnimationObject/Channel/Keyframe nodes. All methods
 * are static; the parser holds no state. A SVGParserContext reference is passed to each entry
 * point so the parser can reuse the context's attribute/color/length/transform helpers and stay
 * in sync with the document's id allocator.
 */
class SMILAnimationParser {
 public:
  /**
   * Builds a single Animation node for the whole SVG document. Iterates over every animated
   * element, generates Channels/Keyframes from each SMIL element, precomputes complex semantics
   * (additive/accumulate/paced/repeatCount), and appends the resulting Animation to
   * doc->animations. Returns nullptr when no animations are present.
   *
   * AnimationObject creation: channels are grouped by their resolved target node id so that each
   * AnimationObject drives exactly one PAGX node. A single SVG element may produce multiple
   * AnimationObjects — for example, <animate attributeName="opacity"> targets the Layer while
   * <animate attributeName="stroke-dashoffset"> targets the Stroke node. <animateTransform> and
   * <animateMotion> always target the Group node created by registerAnimatedElement. Channels
   * sharing the same target and channel name are merged when additive="sum" is set.
   */
  static Animation* buildAnimation(
      SVGParserContext& ctx, PAGXDocument* doc,
      const std::unordered_map<const DOMNode*, SMILAnimationGroup>& smilAnimations,
      const std::unordered_map<const DOMNode*, AnimatedNodeInfo>& animatedNodeMap, float frameRate);

  /**
   * Parses a SMIL clock value into seconds. Supports "2s", "500ms", "2.5" (bare seconds),
   * "00:00:02.5" (HH:MM:SS), "2min", "1h", and "indefinite" (returned as -1). Returns 0 on
   * unparseable input.
   */
  static double parseSMILClockValue(const std::string& value);

  /**
   * Parses a semicolon-separated list of normalized time offsets ("0;0.5;1") into a vector of
   * doubles in [0,1]. Returns an empty vector when the input is empty or invalid.
   */
  static std::vector<double> parseKeyTimes(const std::string& value);

  /**
   * Parses a semicolon-separated list of cubic bezier control point quadruples
   * ("0.42 0 0.58 1; 0.42 0 0.58 1") into one BezierControlPoints per interpolation segment.
   * Returns an empty vector when the input is empty or invalid.
   */
  static std::vector<BezierControlPoints> parseKeySplines(const std::string& value);

 private:
  SMILAnimationParser() = default;

  // Resolves an <animate>/<set> attributeName to the target PAGX node, channel name, and value
  // type. Searches Layer contents for Fill / Stroke / Shape nodes and allocates ids for newly
  // targeted content nodes.
  static ChannelTarget resolveAnimateTarget(SVGParserContext& ctx, PAGXDocument* doc,
                                            const std::string& attributeName,
                                            const AnimatedNodeInfo& nodeInfo);

  // Parses a single <animate> element into a list of Channels. Handles from/to/by/values,
  // keyTimes/keySplines/calcMode, begin/dur/repeatCount/fill, additive/accumulate.
  // outTargetId receives the PAGX node id that the channels drive (may differ from the Layer id
  // when the animate targets a Fill/Stroke/Shape content node).
  static std::vector<Channel*> parseAnimate(SVGParserContext& ctx, PAGXDocument* doc,
                                            const std::shared_ptr<DOMNode>& animElement,
                                            const AnimatedNodeInfo& nodeInfo, float frameRate,
                                            Frame& outEndFrame, std::string& outTargetId);

  // Parses a single <set> element into a single Hold keyframe on each targeted channel.
  // outTargetId receives the PAGX node id that the channel drives.
  static std::vector<Channel*> parseSet(SVGParserContext& ctx, PAGXDocument* doc,
                                        const std::shared_ptr<DOMNode>& setElement,
                                        const AnimatedNodeInfo& nodeInfo, float frameRate,
                                        Frame& outEndFrame, std::string& outTargetId);

  // Parses a single <animateTransform> element into a single Matrix channel driving the Layer's
  // runtime "matrix" channel. Each keyframe's transform params (translate/scale/rotate/skewX/
  // skewY, including rotate's "angle cx cy" form) are baked into a full Matrix. additive="sum"
  // pre-composes the layer's static matrix; fill="remove" reverts to it.
  static std::vector<Channel*> parseAnimateTransform(SVGParserContext& ctx, PAGXDocument* doc,
                                                     const std::shared_ptr<DOMNode>& animElement,
                                                     const AnimatedNodeInfo& nodeInfo,
                                                     float frameRate, Frame& outEndFrame);

  // Parses a single <animateMotion> element into a single Matrix channel driving the Layer's
  // runtime "matrix" channel. Samples the referenced path via PathMeasure with an adaptive
  // density (clamp(totalLength/10, 32, 256)) and bakes Translate(pos) * Rotate(angle) per
  // keyframe. Handles rotate="auto"/"auto-reverse"/<number> and keyPoints.
  static std::vector<Channel*> parseAnimateMotion(SVGParserContext& ctx, PAGXDocument* doc,
                                                  const std::shared_ptr<DOMNode>& animElement,
                                                  const AnimatedNodeInfo& nodeInfo, float frameRate,
                                                  Frame& outEndFrame);

  // Finds the first element of the given NodeType in the target Layer's contents.
  static Element* findContentNode(const AnimatedNodeInfo& nodeInfo, NodeType type);

  // Ensures the given content node has an id, allocating one via ctx when missing, and records
  // the mapping in nodeInfo.contentNodeIds.
  static std::string ensureContentNodeId(SVGParserContext& ctx, PAGXDocument* doc, Element* node,
                                         const AnimatedNodeInfo& nodeInfo, const char* prefix);

  // Parses a single SMIL value string into a KeyValue based on the target channel's value type.
  static KeyValue parseValue(SVGParserContext& ctx, const std::string& str,
                             ChannelValueType valueType);
};

}  // namespace pagx
