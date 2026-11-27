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

#include <algorithm>
#include <cmath>
#include <string>
#include "base/utils/MathUtil.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/utils/ExporterUtils.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using pag::FloatNearlyZero;

void HTMLWriter::writeGroup(HTMLBuilder& out, const Group* group, float alpha, bool distribute,
                            const Matrix& parentMatrix,
                            const std::vector<Element*>* propagatedPainters) {
  RecursionGuard guard(_ctx);
  if (guard.overflowed()) {
    return;
  }
  Matrix gm = BuildGroupMatrix(group);
  if (!parentMatrix.isIdentity()) {
    gm = parentMatrix * gm;
  }
  std::string style = "position:relative";
  if (!gm.isIdentity()) {
    style += ";transform:" + MatrixTransformToCSS(gm);
    style += ";transform-origin:0 0";
  }
  if (group->alpha < 1.0f) {
    // A distributing layer hands its alpha to the paints themselves, so the wrapper keeps only the
    // group's own alpha instead of multiplying it twice.
    style += ";opacity:" + CssFloatToString(group->alpha);
  }
  out.openTag("div");
  auto animId = _ctx->claimAnimatedDomId({group});
  if (!animId.empty()) {
    out.addAttr("id", animId);
  }
  out.addAttr("class", "pagx-group");
  out.addAttr("style", style);
  out.closeTagStart();
  const Padding* groupPadding = group->padding.isZero() ? nullptr : &group->padding;
  const auto* elements = &group->elements;
  std::vector<Element*> paintedElements = {};
  if (propagatedPainters != nullptr && !propagatedPainters->empty()) {
    // These shape-only children normally propagate to the parent Fill/Stroke. Keep their authored
    // coordinates inside the animated wrapper and append only their parent's matching painters.
    paintedElements = group->elements;
    paintedElements.insert(paintedElements.end(), propagatedPainters->begin(),
                           propagatedPainters->end());
    elements = &paintedElements;
  }
  // The layer's alpha has to reach the paints inside a retained group: with `groupOpacity=false`
  // each paint fades on its own, so an overlap keeps the extra opacity the second paint adds.
  writeElements(out, *elements, alpha, distribute, false, LayerPlacement::Background, groupPadding);
  // Recurse through nested Group / TextBox containers so a foreground painter inside a child
  // Group still triggers the second pass — the shallow flat-list scan would otherwise leave it
  // suppressed by both passes' filters.
  bool hasForegroundPainter = HasForegroundPainter(*elements);
  if (hasForegroundPainter) {
    writeElements(out, *elements, alpha, distribute, false, LayerPlacement::Foreground,
                  groupPadding);
  }
  out.closeTag();
}

void HTMLWriter::writeRepeater(HTMLBuilder& out, const Repeater* rep,
                               const std::vector<GeoInfo>& geos, const Fill* fill,
                               const Stroke* stroke, float alpha, const TrimPath* trim,
                               bool applyCopyAlphaDecay) {
  // An animated repeater can start at 0 and grow, so the authored count alone cannot decide whether
  // there is anything to emit: the pre-scan recorded how many copies the channel reaches, and those
  // tags have to exist before the first of them appears.
  const bool animated = _ctx->animation.animatedRepeaters.count(rep) > 0;
  int reachableCopies = 0;
  if (animated) {
    auto maxIt = _ctx->animation.repeaterMaxCopies.find(rep);
    if (maxIt != _ctx->animation.repeaterMaxCopies.end()) {
      reachableCopies = maxIt->second;
    }
  }
  if ((rep->copies <= 0 || !std::isfinite(rep->copies)) && reachableCopies <= 0) {
    return;
  }
  float originOffsetX = _ctx->repeaterOriginOffsetX;
  float originOffsetY = _ctx->repeaterOriginOffsetY;
  _ctx->repeaterOriginOffsetX = 0;
  _ctx->repeaterOriginOffsetY = 0;
  float savedOffX = _ctx->savedChildLayerOffsetX;
  float savedOffY = _ctx->savedChildLayerOffsetY;
  _ctx->savedChildLayerOffsetX = 0;
  _ctx->savedChildLayerOffsetY = 0;
  int n = static_cast<int>(std::ceil(rep->copies));
  float frac = rep->copies - std::floor(rep->copies);
  // An animated repeater channel is fanned out per copy, so every copy needs a tag of its own; the
  // weaver later attaches one keyframes rule to each of them.
  const int authoredCopies = n;
  if (animated) {
    // Emit every copy the channel can reach, so a copy that appears mid-playback already has a tag.
    n = std::max(n, reachableCopies);
    if (applyCopyAlphaDecay) {
      // The per-frame emitter recomposes the copies from the channels, so it needs the same
      // "painter before the Repeater" decision this static pass just made.
      _ctx->animation.repeaterDecays.insert(rep);
    }
  }
  for (int i = 0; i < n; i++) {
    int idx = (rep->order == RepeaterOrder::AboveOriginal) ? i : (n - 1 - i);
    Matrix m = BuildRepeaterCopyMatrix(rep, i);
    if (!FloatNearlyZero(originOffsetX) || !FloatNearlyZero(originOffsetY)) {
      m = Matrix::Translate(-originOffsetX, -originOffsetY) * m;
    }
    float ca = 1.0f;
    if (applyCopyAlphaDecay) {
      float denom = std::max(std::ceil(rep->copies), 1.0f);
      float np = std::clamp((static_cast<float>(idx) + rep->offset) / denom, 0.0f, 1.0f);
      ca = rep->startAlpha + (rep->endAlpha - rep->startAlpha) * np;
      if (idx == n - 1 && frac > 0 && frac < 1.0f) {
        ca *= frac;
      }
    } else if (idx == n - 1 && frac > 0 && frac < 1.0f) {
      ca = frac;
    }
    float ea = ca * alpha;
    if (animated && i >= authoredCopies) {
      // Copies past the authored count only exist once the channel reaches them.
      ea = 0.0f;
    }
    std::string copyId = {};
    if (animated) {
      copyId = rep->id + "_copy" + std::to_string(i);
      // The same Composition can be mounted several times, and each mount paints its own copies:
      // the ids have to stay unique or every instance would target the first one's tag.
      if (_ctx->animation.scope != 0) {
        copyId += "__" + std::to_string(_ctx->animation.scope);
      }
    }
    if (animated || !m.isIdentity() || ea < 1.0f) {
      out.openTag("div");
      if (!copyId.empty()) {
        out.addAttr("id", copyId);
        _ctx->animation.repeaterCopies.push_back({rep, i, copyId, _ctx->animation.scope});
      }
      std::string s = "position:absolute";
      if (!m.isIdentity()) {
        s += ";transform:" + MatrixTransformToCSS(m);
        s += ";transform-origin:0 0";
      }
      if (ea < 1.0f) {
        s += ";opacity:" + CssFloatToString(ea);
      }
      out.addAttr("style", s);
      out.closeTagStart();
      renderGeo(out, geos, fill, stroke, 1.0f, false, trim, false);
      // Animation export can defer SVG emission. Flush before closing this copy so its
      // transform and opacity apply to the SVG, while the child-layer offset is still zero.
      flushPendingSVGDraws(out);
      out.closeTag();
    } else {
      renderGeo(out, geos, fill, stroke, 1.0f, false, trim, false);
      flushPendingSVGDraws(out);
    }
  }
  _ctx->savedChildLayerOffsetX = savedOffX;
  _ctx->savedChildLayerOffsetY = savedOffY;
}

void HTMLWriter::writeComposition(HTMLBuilder& out, const Composition* comp, float alpha,
                                  bool distribute) {
  RecursionGuard guard(_ctx);
  if (guard.overflowed()) {
    return;
  }
  if (_ctx->visitedCompositions.count(comp)) {
    return;
  }
  _ctx->visitedCompositions.insert(comp);
  bool needContainer = comp->width > 0 && comp->height > 0;
  if (needContainer) {
    out.openTag("div");
    out.addAttr("style", "position:relative;width:" + CssFloatToString(comp->width) +
                             "px;height:" + CssFloatToString(comp->height) + "px");
    out.closeTagStart();
  }
  for (auto* layer : comp->layers) {
    writeLayer(out, layer, alpha, distribute);
  }
  if (needContainer) {
    out.closeTag();
  }
  _ctx->visitedCompositions.erase(comp);
}

}  // namespace pagx
