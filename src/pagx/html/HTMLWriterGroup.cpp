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
                            const Matrix& parentMatrix) {
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
    float ea = distribute ? (group->alpha * alpha) : group->alpha;
    style += ";opacity:" + CssFloatToString(ea);
  }
  out.openTag("div");
  out.addAttr("class", "pagx-group");
  out.addAttr("style", style);
  out.closeTagStart();
  const Padding* groupPadding = group->padding.isZero() ? nullptr : &group->padding;
  writeElements(out, group->elements, 1.0f, false, LayerPlacement::Background, groupPadding);
  bool hasForegroundPainter = false;
  for (auto* e : group->elements) {
    if (e->nodeType() == NodeType::Fill) {
      if (static_cast<const Fill*>(e)->placement == LayerPlacement::Foreground) {
        hasForegroundPainter = true;
        break;
      }
    } else if (e->nodeType() == NodeType::Stroke) {
      if (static_cast<const Stroke*>(e)->placement == LayerPlacement::Foreground) {
        hasForegroundPainter = true;
        break;
      }
    }
  }
  if (hasForegroundPainter) {
    writeElements(out, group->elements, 1.0f, false, LayerPlacement::Foreground, groupPadding);
  }
  out.closeTag();
}

void HTMLWriter::writeRepeater(HTMLBuilder& out, const Repeater* rep,
                               const std::vector<GeoInfo>& geos, const Fill* fill,
                               const Stroke* stroke, float alpha, const TrimPath* trim,
                               bool applyCopyAlphaDecay) {
  if (rep->copies <= 0 || !std::isfinite(rep->copies)) {
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
    if (!m.isIdentity() || ea < 1.0f) {
      out.openTag("div");
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
      out.closeTag();
    } else {
      renderGeo(out, geos, fill, stroke, 1.0f, false, trim, false);
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
