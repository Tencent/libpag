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

#include <string>
#include <vector>
#include "pagx/types/Matrix.h"

namespace pagx {

class ColorSource;
class ColorStop;
class Fill;
class Group;
class Layer;
class Node;
class PAGXDocument;
class Stroke;
class SVGAnimationPlan;
class XMLBuilder;

/**
 * One animated Group on the path from a Layer to a geometry element. `prefix` is the accumulated
 * transform right before the group's own matrix, captured during the element walk so the total
 * transform can be recomputed per frame with the group's matrix swapped out.
 */
struct SVGGroupTransformStep {
  const Group* group = nullptr;
  Matrix prefix = {};
};

/**
 * SVGAnimationWriter turns the channel bindings collected by SVGAnimationPlan into SMIL elements.
 * Every Write* method appends children to an element the caller has already opened, and returns
 * false when the target carries no exportable animation so the caller can keep using the terse
 * self-closing form.
 */
class SVGAnimationWriter {
 public:
  explicit SVGAnimationWriter(const SVGAnimationPlan* plan,
                              std::vector<std::string>* warnings = nullptr,
                              PAGXDocument* document = nullptr);

  const SVGAnimationPlan* plan() const {
    return _plan;
  }

  SVGAnimationWriter(const SVGAnimationWriter&) = delete;
  SVGAnimationWriter& operator=(const SVGAnimationWriter&) = delete;

  /**
   * Writes the Layer channels that live on the group that carries the mask / filter / blend
   * attributes: opacity and visibility. Must be called right after that <g> was opened.
   */
  bool WriteLayerOuterAnimates(XMLBuilder& out, const Layer* layer);

  /**
   * Writes the Layer transform animation. Must be called right after the <g> that carries the
   * layer transform was opened, which is the inner <g> of a masked layer and the outer one
   * otherwise.
   */
  bool WriteLayerTransformAnimates(XMLBuilder& out, const Layer* layer);

  /**
   * Writes the Fill / Stroke paint animations inside the geometry element that carries them.
   * `alphaMultiplier` is the accumulated Layer / Group alpha already applied to the static
   * emission, so the animated opacity values stay on the same scale.
   */
  bool WritePainterAnimates(XMLBuilder& out, const Fill* fill, const Stroke* stroke,
                            float alphaMultiplier);

  /**
   * Returns true when the layer needs an SMIL host <g> beyond what its static properties require.
   */
  bool HasLayerAnimates(const Layer* layer) const;

  /**
   * Returns the layer transform at frame 0 of its transform animation, used as the static baseline
   * that applies before the SMIL timeline starts. Falls back to the static transform when the layer
   * has no transform animation.
   */
  Matrix LayerTransformBaseMatrix(const Layer* layer) const;

  /**
   * Returns true when the Fill or Stroke painter carries at least one exportable animation.
   */
  bool HasPainterAnimates(const Fill* fill, const Stroke* stroke) const;

  /**
   * Returns true when the Group owns an animated transform channel.
   */
  bool HasGroupAnimates(const Group* group) const;

  /**
   * Emits the transform animation for a geometry element that sits under animated Groups. Group
   * transforms are flattened into each descendant element, so their animation is pushed down to the
   * element instead of being hosted by a wrapper <g>, which the deferred painter scope could not
   * contain. `steps` describes the animated ancestor chain captured during the walk.
   */
  bool WriteElementTransformAnimates(XMLBuilder& out, const SVGGroupTransformStep* steps,
                                     size_t count, const Matrix& staticTransform);

  /**
   * Returns true when the gradient color stop owns an exportable offset or colour animation.
   */
  bool HasStopAnimates(const ColorStop* stop) const;

  /**
   * Returns true when the stop's colour channel is animated, which means the stop's `stop-color`
   * and `stop-opacity` are driven by SMIL instead of a static declaration.
   */
  bool HasStopColorAnimates(const ColorStop* stop) const;

  /**
   * Returns true when the stop colour track carries a Display P3 keyframe. SMIL values are sRGB,
   * so the caller has to report the gamut loss instead of writing a P3 CSS declaration that the
   * animation would bypass.
   */
  bool StopColorTrackIsWideGamut(const ColorStop* stop) const;

  /**
   * Emits the SMIL children of a <stop> element: the `offset` track plus the `stop-color` /
   * `stop-opacity` pair. Returns false when the stop has no exportable animation.
   */
  bool WriteStopAnimates(XMLBuilder& out, const ColorStop* stop);

  /**
   * Returns true when a linear or radial gradient drives at least one geometry channel
   * (endpoints, centre, radius).
   */
  bool HasGradientGeometryAnimates(const Node* gradient) const;

  /**
   * Emits the geometry tracks of a linear gradient (`x1` / `y1` / `x2` / `y2`) or a radial
   * gradient (`cx` / `cy` / `r`). Must be called inside the open gradient def, before its stops.
   */
  bool WriteGradientGeometryAnimates(XMLBuilder& out, const Node* gradient);

  /**
   * Returns true when the Path carries at least one `point{N}.x` / `point{N}.y` channel. These
   * channels are a table-external family: they address the PathData point array directly and are
   * not part of the reflective node channel registry.
   */
  bool HasPathPointAnimates(const Node* path) const;

  /**
   * Emits the `d` track for a Path whose point channels are animated. `renderPosition` and
   * `renderScale` are the static placement the exporter bakes into every frame, matching the
   * authored-point coordinate space the keyframe values live in.
   */
  bool WritePathDataAnimates(XMLBuilder& out, const Node* path, const Point& renderPosition,
                             float renderScale);

  /**
   * Returns true when the Text drives an `x` / `y` channel. Those channels translate the shaped
   * run without reshaping it.
   */
  bool HasTextPositionAnimates(const Node* text) const;

  /**
   * Emits a translate track that carries the animated Text position relative to its authored
   * position. The caller hosts it on a throwaway <g> wrapping the emitted text.
   */
  bool WriteTextPositionAnimates(XMLBuilder& out, const Node* text);

  /**
   * Returns true when every channel bound to the Polystar is a roundness track, which is the
   * geometry subset whose content bounds (and therefore layout placement) stay static.
   */
  bool HasPolystarDataAnimates(const Node* polystar) const;

  /**
   * Emits a `d` track for a Polystar whose inner/outer roundness is animated. Every frame uses the
   * same cubic verb list (zero-roundness cubics collapse onto the straight edge) so SMIL can
   * interpolate the path.
   */
  bool WritePolystarDataAnimates(XMLBuilder& out, const Node* polystar);

  /**
   * Emits the opacity animation for a geometry element that sits under animated Group alpha
   * channels. The group alpha is already folded into `baseAlpha`, so the emitted values are the
   * animated ratio applied on top of it.
   */
  bool WriteElementOpacityAnimates(XMLBuilder& out, const SVGGroupTransformStep* steps,
                                   size_t count, float baseAlpha, const char* attributeName);

  /**
   * Returns true when any Group on the path drives an alpha channel.
   */
  bool HasElementOpacityAnimates(const SVGGroupTransformStep* steps, size_t count) const;

  /**
   * Returns true when the painter's opacity (its own alpha or the color's alpha) is animated, which
   * means an ancestor Group opacity animation cannot share the same attribute.
   */
  bool HasPainterOpacityAnimates(const Fill* fill, const Stroke* stroke) const;

  /**
   * Returns true when the shape's own geometry channels (position / size / roundness) are animated.
   */
  bool HasShapeGeometryAnimates(const Node* element) const;

  /**
   * Emits the geometry attribute animations for a Rectangle or Ellipse (x / y / width / height /
   * rx / ry / cx / cy / r). `stroke` carries the stroke-alignment inset the static writer applies,
   * so the animated values land on the same geometry.
   */
  bool WriteShapeGeometryAnimates(XMLBuilder& out, const Node* element, const Stroke* stroke);

  /**
   * Emits the SMIL children for the filter / style primitive that is currently open in <defs>:
   * `feGaussianBlur` (stdDeviation), `feOffset` (dx / dy) and `feFlood` (flood-color /
   * flood-opacity). Each returns false when the node has no animation for that primitive.
   */
  bool WriteFilterBlurAnimates(XMLBuilder& out, const Node* node);

  bool WriteFilterOffsetAnimates(XMLBuilder& out, const Node* node);

  bool WriteFilterColorAnimates(XMLBuilder& out, const Node* node);

  /**
   * True when the filter / style node drives at least one of the primitives above, so the caller
   * must open the element instead of self-closing it.
   */
  bool HasFilterAnimates(const Node* node) const;

  bool HasFilterBlurAnimates(const Node* node) const;

  bool HasFilterOffsetAnimates(const Node* node) const;

  /**
   * Largest value the channel reaches across its keyframes, or `fallback` when the channel is not
   * animated. Filter regions are emitted once for the whole element, so they must cover the whole
   * animation rather than only frame 0.
   */
  float MaxChannelValue(const Node* node, const char* channelName, float fallback) const;

  /**
   * Largest absolute value the channel reaches across its keyframes, used for offsets whose sign
   * decides which side of the bbox grows.
   */
  float MaxChannelMagnitude(const Node* node, const char* channelName, float fallback) const;

 private:
  const SVGAnimationPlan* _plan = nullptr;
  std::vector<std::string>* _warnings = nullptr;
  // Used to allocate transient Polystar snapshots while sampling dynamic geometry. The document
  // owns them until the exporter's ModifierResolver cleans up its generated nodes.
  PAGXDocument* _document = nullptr;

  bool WriteVisibilityAnimate(XMLBuilder& out, const Node* target);

  bool WriteChannelAnimate(XMLBuilder& out, const Node* target, const char* channelName,
                           const char* attributeName);

  bool WriteChannelAnimateClamped(XMLBuilder& out, const Node* target, const char* channelName,
                                  const char* attributeName, float minValue, float maxValue);

  bool WritePaintAnimates(XMLBuilder& out, const Node* painter, const char* paintAttribute,
                          float alphaMultiplier);

  static float StaticColorAlpha(const ColorSource* source);

  static bool IsSolidColorPainter(const Node* painter);

  void AddWarning(std::string message) const;
};

}  // namespace pagx
