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
class Fill;
class Group;
class Layer;
class Node;
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
                              std::vector<std::string>* warnings = nullptr);

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

 private:
  const SVGAnimationPlan* _plan = nullptr;
  std::vector<std::string>* _warnings = nullptr;

  bool WriteVisibilityAnimate(XMLBuilder& out, const Node* target);

  bool WriteChannelAnimate(XMLBuilder& out, const Node* target, const char* channelName,
                           const char* attributeName);

  bool WritePaintAnimates(XMLBuilder& out, const Node* painter, const char* paintAttribute,
                          float alphaMultiplier);

  static float StaticColorAlpha(const ColorSource* source);

  static bool IsSolidColorPainter(const Node* painter);

  void AddWarning(std::string message) const;
};

}  // namespace pagx
