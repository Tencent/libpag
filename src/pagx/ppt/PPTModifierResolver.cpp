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

#include "pagx/ppt/PPTModifierResolver.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/types/MergePathMode.h"
#include "renderer/ToTGFX.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/PathEffect.h"
#include "tgfx/core/PathTypes.h"

namespace pagx {

// std::pow(negative, non-integer) returns NaN, which would propagate through
// the downstream geometry when a Repeater uses a negative scale with a
// fractional offset. Raise the magnitude and reapply the sign so a negative
// base keeps producing a negative result.
static float SignedPow(float base, float exp) {
  float sign = base < 0.0f ? -1.0f : 1.0f;
  return sign * std::pow(std::abs(base), exp);
}

//==============================================================================
// Geometry helpers: build pagx::PathData from primitive shapes and from a
// resolved tgfx::Path round-trip.
//==============================================================================

static void AppendPathDataFromTGFX(PathData* out, const tgfx::Path& tp) {
  for (auto& seg : tp) {
    switch (seg.verb) {
      case tgfx::PathVerb::Move:
        out->moveTo(seg.points[0].x, seg.points[0].y);
        break;
      case tgfx::PathVerb::Line:
        out->lineTo(seg.points[1].x, seg.points[1].y);
        break;
      case tgfx::PathVerb::Quad:
        out->quadTo(seg.points[1].x, seg.points[1].y, seg.points[2].x, seg.points[2].y);
        break;
      case tgfx::PathVerb::Conic: {
        // PathData has no conic verb; flatten to a small fan of quads.
        auto quads = tgfx::Path::ConvertConicToQuads(seg.points[0], seg.points[1], seg.points[2],
                                                     seg.conicWeight, 1);
        for (size_t i = 1; i + 1 < quads.size(); i += 2) {
          out->quadTo(quads[i].x, quads[i].y, quads[i + 1].x, quads[i + 1].y);
        }
        break;
      }
      case tgfx::PathVerb::Cubic:
        out->cubicTo(seg.points[1].x, seg.points[1].y, seg.points[2].x, seg.points[2].y,
                     seg.points[3].x, seg.points[3].y);
        break;
      case tgfx::PathVerb::Close:
        out->close();
        break;
      case tgfx::PathVerb::Done:
        break;
    }
  }
}

static PathData* MakePathDataFromTGFX(PAGXDocument* doc, const tgfx::Path& tp) {
  auto* pd = doc->makeNode<PathData>();
  AppendPathDataFromTGFX(pd, tp);
  return pd;
}

// Build a pagx::Path element wrapping a fresh PathData.
//
// The path data already lives in the surrounding layer's coordinate space
// (resolver helpers bake in renderPosition / renderScale of the source
// primitive when emitting the geometry), so the wrapper Path keeps
// position=(0,0) and we prime its preferred layout from the data bounds.
// Without this, layoutBounds() reports an empty rect and any later modifier
// that re-enters PrimitiveToTGFXPath sees renderScale()==0 — collapsing the
// geometry to the origin and dropping the shape from the PPTX.
Element* PPTModifierResolver::makePathFromData(PathData* data) const {
  auto* p = _doc->makeNode<Path>();
  p->data = data;
  p->position = {0.0f, 0.0f};
  p->updateSize(nullptr);
  return p;
}

//==============================================================================
// Polystar -> tgfx::Path. Mirrors tgfx/src/layers/vectors/Polystar.cpp so the
// emitted geometry matches what the PAG renderer would produce for the same
// pagx::Polystar (fractional pointCount, roundness, and reversed all behave
// identically).
//==============================================================================

static void AddPolystarCurve(tgfx::Path* path, float centerX, float centerY, float angleDelta,
                             float dx1, float dy1, float roundness1, float dx2, float dy2,
                             float roundness2) {
  float control1X = dx1 - dy1 * roundness1 * angleDelta + centerX;
  float control1Y = dy1 + dx1 * roundness1 * angleDelta + centerY;
  float control2X = dx2 + dy2 * roundness2 * angleDelta + centerX;
  float control2Y = dy2 - dx2 * roundness2 * angleDelta + centerY;
  path->cubicTo(control1X, control1Y, control2X, control2Y, dx2 + centerX, dy2 + centerY);
}

static tgfx::Path StarToTGFXPath(const Polystar* star) {
  tgfx::Path path = {};
  float points = star->pointCount;
  if (points <= 0.0f) {
    return path;
  }
  auto renderPos = star->renderPosition();
  float centerX = renderPos.x;
  float centerY = renderPos.y;
  float innerRadius = star->renderInnerRadius();
  float outerRadius = star->renderOuterRadius();
  float innerRoundness = star->innerRoundness;
  float outerRoundness = star->outerRoundness;
  float direction = star->reversed ? -1.0f : 1.0f;
  float angleStep = static_cast<float>(M_PI) / points;
  float currentAngle = (star->rotation - 90.0f) * static_cast<float>(M_PI) / 180.0f;
  int numPoints = static_cast<int>(std::ceil(points)) * 2;
  float decimalPart = points - std::floor(points);
  int decimalIndex = -2;
  if (decimalPart != 0.0f) {
    decimalIndex = direction > 0.0f ? 1 : numPoints - 3;
    currentAngle -= angleStep * decimalPart * 2.0f;
  }

  float firstDx = outerRadius * std::cos(currentAngle);
  float firstDy = outerRadius * std::sin(currentAngle);
  float lastDx = firstDx;
  float lastDy = firstDy;
  path.moveTo(lastDx + centerX, lastDy + centerY);

  bool outerFlag = false;
  for (int i = 0; i < numPoints; ++i) {
    float angleDelta = angleStep * direction;
    float dx;
    float dy;
    if (i == numPoints - 1) {
      dx = firstDx;
      dy = firstDy;
    } else {
      float radius = outerFlag ? outerRadius : innerRadius;
      if (i == decimalIndex || i == decimalIndex + 1) {
        radius = innerRadius + decimalPart * (radius - innerRadius);
        angleDelta *= decimalPart;
      }
      currentAngle += angleDelta;
      dx = radius * std::cos(currentAngle);
      dy = radius * std::sin(currentAngle);
    }
    if (innerRoundness != 0.0f || outerRoundness != 0.0f) {
      float lastRoundness;
      float roundness;
      if (outerFlag) {
        lastRoundness = innerRoundness;
        roundness = outerRoundness;
      } else {
        lastRoundness = outerRoundness;
        roundness = innerRoundness;
      }
      AddPolystarCurve(&path, centerX, centerY, angleDelta * 0.5f, lastDx, lastDy, lastRoundness,
                       dx, dy, roundness);
      lastDx = dx;
      lastDy = dy;
    } else {
      path.lineTo(dx + centerX, dy + centerY);
    }
    outerFlag = !outerFlag;
  }
  path.close();
  return path;
}

static tgfx::Path PolygonToTGFXPath(const Polystar* poly) {
  tgfx::Path path = {};
  float points = poly->pointCount;
  if (points <= 0.0f) {
    return path;
  }
  int numPoints = static_cast<int>(std::floor(points));
  if (numPoints < 3) {
    return path;
  }
  auto renderPos = poly->renderPosition();
  float centerX = renderPos.x;
  float centerY = renderPos.y;
  float radius = poly->renderOuterRadius();
  float roundness = poly->outerRoundness;
  float direction = poly->reversed ? -1.0f : 1.0f;
  float angleStep = 2.0f * static_cast<float>(M_PI) / static_cast<float>(numPoints);
  float currentAngle = (poly->rotation - 90.0f) * static_cast<float>(M_PI) / 180.0f;

  float firstDx = radius * std::cos(currentAngle);
  float firstDy = radius * std::sin(currentAngle);
  float lastDx = firstDx;
  float lastDy = firstDy;
  path.moveTo(lastDx + centerX, lastDy + centerY);

  for (int i = 0; i < numPoints; ++i) {
    float angleDelta = angleStep * direction;
    float dx;
    float dy;
    if (i == numPoints - 1) {
      dx = firstDx;
      dy = firstDy;
    } else {
      currentAngle += angleDelta;
      dx = radius * std::cos(currentAngle);
      dy = radius * std::sin(currentAngle);
    }
    if (roundness != 0.0f) {
      AddPolystarCurve(&path, centerX, centerY, angleDelta * 0.25f, lastDx, lastDy, roundness, dx,
                       dy, roundness);
      lastDx = dx;
      lastDy = dy;
    } else {
      path.lineTo(dx + centerX, dy + centerY);
    }
  }
  path.close();
  return path;
}

//==============================================================================
// Primitive -> tgfx::Path. We honour position offsets so the path lives in the
// same coordinate space the painter assumes.
//==============================================================================

static tgfx::Path PrimitiveToTGFXPath(const Element* el) {
  auto type = el->nodeType();
  if (type == NodeType::Path) {
    auto* p = static_cast<const Path*>(el);
    if (p->data == nullptr) {
      return tgfx::Path();
    }
    auto tp = ToTGFX(*p->data);
    auto scale = p->renderScale();
    if (scale != 1.0f) {
      tgfx::Matrix scaleM = tgfx::Matrix::MakeScale(scale, scale);
      tp.transform(scaleM);
    }
    auto renderPos = p->renderPosition();
    if (renderPos.x != 0.0f || renderPos.y != 0.0f) {
      tgfx::Matrix m = tgfx::Matrix::MakeTrans(renderPos.x, renderPos.y);
      tp.transform(m);
    }
    if (p->reversed) {
      tp.reverse();
    }
    return tp;
  }
  if (type == NodeType::Rectangle) {
    auto* r = static_cast<const Rectangle*>(el);
    tgfx::Path tp = {};
    auto renderSize = r->renderSize();
    auto renderPos = r->renderPosition();
    float halfW = renderSize.width * 0.5f;
    float halfH = renderSize.height * 0.5f;
    tgfx::Rect rect = tgfx::Rect::MakeLTRB(renderPos.x - halfW, renderPos.y - halfH,
                                           renderPos.x + halfW, renderPos.y + halfH);
    float rad = std::min({r->roundness, halfW, halfH});
    if (rad < 0.0f) {
      rad = 0.0f;
    }
    // Mirror the tgfx renderer (Rectangle::apply): always go through
    // addRoundRect with startIndex=2 so a TrimPath / RoundCorner downstream
    // sees the same vertex order. Using addRect (default startIndex=0) starts
    // the contour at the top-left corner instead of mid-right, which made
    // trimmed rectangles render at a different position than the tgfx
    // baseline.
    tp.addRoundRect(rect, rad, rad, r->reversed, 2);
    return tp;
  }
  if (type == NodeType::Ellipse) {
    auto* e = static_cast<const Ellipse*>(el);
    auto renderSize = e->renderSize();
    auto renderPos = e->renderPosition();
    float halfW = renderSize.width * 0.5f;
    float halfH = renderSize.height * 0.5f;
    tgfx::Rect rect = tgfx::Rect::MakeLTRB(renderPos.x - halfW, renderPos.y - halfH,
                                           renderPos.x + halfW, renderPos.y + halfH);
    tgfx::Path tp = {};
    // Mirrors tgfx Ellipse::apply: addOval with default startIndex=0 (top
    // centre point), so the trim's start anchor matches the renderer.
    tp.addOval(rect, e->reversed);
    return tp;
  }
  if (type == NodeType::Polystar) {
    auto* ps = static_cast<const Polystar*>(el);
    return ps->type == PolystarType::Polygon ? PolygonToTGFXPath(ps) : StarToTGFXPath(ps);
  }
  return tgfx::Path();
}

//==============================================================================
// Modifier appliers. Each returns the new pagx::Path replacement(s).
//==============================================================================

Element* PPTModifierResolver::applyTrimToElement(Element* shape, const TrimPath* trim) const {
  auto tp = PrimitiveToTGFXPath(shape);
  if (tp.isEmpty()) {
    return shape;
  }
  float offset = trim->offset / 360.0f;
  auto effect = tgfx::PathEffect::MakeTrim(trim->start + offset, trim->end + offset);
  if (effect == nullptr || !effect->filterPath(&tp)) {
    return shape;
  }
  return makePathFromData(MakePathDataFromTGFX(_doc, tp));
}

Element* PPTModifierResolver::applyRoundCornerToElement(Element* shape,
                                                        const RoundCorner* corner) const {
  if (corner->radius <= 0.0f) {
    return shape;
  }
  auto tp = PrimitiveToTGFXPath(shape);
  if (tp.isEmpty()) {
    return shape;
  }
  auto effect = tgfx::PathEffect::MakeCorner(corner->radius);
  if (effect == nullptr || !effect->filterPath(&tp)) {
    return shape;
  }
  return makePathFromData(MakePathDataFromTGFX(_doc, tp));
}

static tgfx::PathOp MergeModeToPathOp(MergePathMode mode) {
  switch (mode) {
    case MergePathMode::Union:
      return tgfx::PathOp::Union;
    case MergePathMode::Difference:
      return tgfx::PathOp::Difference;
    case MergePathMode::Intersect:
      return tgfx::PathOp::Intersect;
    case MergePathMode::Xor:
      return tgfx::PathOp::XOR;
    case MergePathMode::Append:
    default:
      return tgfx::PathOp::Append;
  }
}

// Collapses every shape slot in `output` into a single path placed at the first
// slot's position, then rewrites `shapeSlots` to reference only that surviving
// slot. Used by TrimPath (Continuous) and MergePath which fuse all in-scope
// shapes into one resolved geometry.
static void CollapseShapeSlotsToSinglePath(std::vector<Element*>& output,
                                           std::vector<size_t>& shapeSlots, Element* replacement) {
  size_t firstSlot = shapeSlots.front();
  output[firstSlot] = replacement;
  // Erase right-to-left so earlier indices remain valid; shapeSlots is built
  // in ascending order during traversal.
  for (auto it = shapeSlots.rbegin(); it != shapeSlots.rend(); ++it) {
    if (*it != firstSlot) {
      output.erase(output.begin() + static_cast<long>(*it));
    }
  }
  shapeSlots.clear();
  shapeSlots.push_back(firstSlot);
}

//==============================================================================
// Repeater expansion. Each generated copy reuses the source element pointer; the
// transform is folded into a fresh wrapping Group.
//==============================================================================

static Group* MakeCopyGroup(PAGXDocument* doc, const std::vector<Element*>& body, float tx,
                            float ty, float rotation, float sx, float sy, float anchorX,
                            float anchorY, float alpha) {
  auto* g = doc->makeNode<Group>();
  g->elements = body;
  g->anchor = {anchorX, anchorY};
  g->position = {tx + anchorX, ty + anchorY};
  g->rotation = rotation;
  g->scale = {sx, sy};
  g->alpha = alpha;
  // Prime the wrapper's preferred layout from `position` so that
  // BuildGroupMatrix's read of renderPosition() returns the per-copy offset.
  // Without this, layoutBounds() falls back to preferredX/Y == 0 (these
  // synthetic groups never participate in the document layout pass) and every
  // copy collapses onto the origin, hiding all repeater offsets in the PPTX.
  // Skip Group::updateSize's child recursion — body children were already
  // measured by the document's layout pass — and call the base directly so
  // virtual dispatch invokes Group::onMeasure to seed preferredX/Y.
  g->LayoutNode::updateSize(nullptr);
  return g;
}

//==============================================================================
// resolve()
//==============================================================================

std::vector<Element*> PPTModifierResolver::resolve(const std::vector<Element*>& elements) {
  std::vector<Element*> output;
  output.reserve(elements.size());
  // Indices into `output` of every shape-like element produced so far in this
  // group. Modifiers operate on the contents of these slots.
  std::vector<size_t> shapeSlots;

  for (auto* element : elements) {
    auto type = element->nodeType();
    switch (type) {
      case NodeType::Polystar: {
        // Convert to an editable Path node so downstream writers see uniform
        // geometry. Round-tripping through tgfx keeps this consistent with how
        // the renderer would interpret the same Polystar.
        auto* ps = static_cast<const Polystar*>(element);
        auto tp = ps->type == PolystarType::Polygon ? PolygonToTGFXPath(ps) : StarToTGFXPath(ps);
        auto* pd = MakePathDataFromTGFX(_doc, tp);
        auto* path = makePathFromData(pd);
        shapeSlots.push_back(output.size());
        output.push_back(path);
        break;
      }
      case NodeType::Rectangle:
      case NodeType::Ellipse:
      case NodeType::Path:
        shapeSlots.push_back(output.size());
        output.push_back(element);
        break;
      case NodeType::Group:
        // Groups pass through unmodified; the writer recurses into them and will
        // re-invoke `resolve` on the inner element list with its own scope. This
        // avoids double-resolution and keeps the original Group identity.
        output.push_back(element);
        break;
      case NodeType::TrimPath: {
        auto* trim = static_cast<const TrimPath*>(element);
        if (shapeSlots.empty()) {
          break;
        }
        if (trim->type == TrimType::Separate) {
          for (auto idx : shapeSlots) {
            output[idx] = applyTrimToElement(output[idx], trim);
          }
        } else {
          // Continuous: concatenate into a single tgfx::Path so the trim spans
          // every contour in length space, then split back per-shape using
          // PathMeasure. We approximate by trimming the union and emitting it
          // as a single Path. Multi-contour shapes remain editable.
          tgfx::Path combined = {};
          for (auto idx : shapeSlots) {
            combined.addPath(PrimitiveToTGFXPath(output[idx]), tgfx::PathOp::Append);
          }
          if (combined.isEmpty()) {
            break;
          }
          float offset = trim->offset / 360.0f;
          auto effect = tgfx::PathEffect::MakeTrim(trim->start + offset, trim->end + offset);
          if (effect != nullptr && effect->filterPath(&combined)) {
            auto* path = makePathFromData(MakePathDataFromTGFX(_doc, combined));
            CollapseShapeSlotsToSinglePath(output, shapeSlots, path);
          }
        }
        break;
      }
      case NodeType::RoundCorner: {
        auto* rc = static_cast<const RoundCorner*>(element);
        for (auto idx : shapeSlots) {
          output[idx] = applyRoundCornerToElement(output[idx], rc);
        }
        break;
      }
      case NodeType::MergePath: {
        auto* mp = static_cast<const MergePath*>(element);
        if (shapeSlots.empty()) {
          break;
        }
        tgfx::Path combined = PrimitiveToTGFXPath(output[shapeSlots.front()]);
        auto op = MergeModeToPathOp(mp->mode);
        for (size_t i = 1; i < shapeSlots.size(); ++i) {
          combined.addPath(PrimitiveToTGFXPath(output[shapeSlots[i]]), op);
        }
        auto* path = makePathFromData(MakePathDataFromTGFX(_doc, combined));
        CollapseShapeSlotsToSinglePath(output, shapeSlots, path);
        break;
      }
      case NodeType::Repeater: {
        auto* rep = static_cast<const Repeater*>(element);
        float copiesF = rep->copies;
        if (copiesF < 0.0f) {
          break;
        }
        if (copiesF == 0.0f) {
          // Mirrors ShapeRenderer::ApplyRepeater: copies==0 clears the entire
          // group's content, so nothing in the current scope renders. Painters
          // that sit alone wouldn't paint anything anyway, so dropping them too
          // matches the renderer behaviour exactly.
          output.clear();
          shapeSlots.clear();
          break;
        }
        if (output.empty()) {
          break;
        }

        // Snapshot the entire current scope (shapes + painters + nested groups
        // + anything else accumulated so far) as the body of every copy. This
        // matches ShapeRenderer::ApplyRepeater which clones the parent group
        // for each copy. Without this, copies that didn't include the Fill /
        // Stroke siblings would render colourless because the writer's
        // CollectFillStroke walks only the top level of each Group.
        std::vector<Element*> body = output;

        int maxCount = static_cast<int>(std::ceil(copiesF));
        std::vector<Element*> generated;
        generated.reserve(static_cast<size_t>(maxCount));
        for (int i = 0; i < maxCount; ++i) {
          float progress = static_cast<float>(i) + rep->offset;
          float sx = SignedPow(rep->scale.x, progress);
          float sy = SignedPow(rep->scale.y, progress);
          float rotation = rep->rotation * progress;
          float tx = rep->position.x * progress;
          float ty = rep->position.y * progress;
          float frac = progress / static_cast<float>(maxCount);
          float copyAlpha = rep->startAlpha + (rep->endAlpha - rep->startAlpha) * frac;
          // Fractional copies: the last copy gets a partial alpha multiplier
          // so the cumulative opacity matches the fractional copy count.
          if (i == maxCount - 1 && static_cast<float>(maxCount) != copiesF) {
            copyAlpha *= (copiesF - static_cast<float>(i));
          }
          auto* g = MakeCopyGroup(_doc, body, tx, ty, rotation, sx, sy, rep->anchor.x,
                                  rep->anchor.y, copyAlpha);
          generated.push_back(g);
        }

        // Drawing order:
        //   BelowOriginal -> [c0, c1, ..., cN-1]: c0 drawn first (bottom),
        //     cN-1 drawn last (top). The original (smallest progress) sits at
        //     the bottom of the stack, copies layered above.
        //   AboveOriginal -> [cN-1, ..., c1, c0]: c0 drawn last (top). The
        //     original sits on top of the stack.
        if (rep->order == RepeaterOrder::AboveOriginal) {
          std::reverse(generated.begin(), generated.end());
        }

        // Replace the current scope with the copies. Subsequent siblings (after
        // the Repeater in source order) will be appended after these copies as
        // the surrounding loop continues. Generated groups are not added back
        // to shapeSlots: chained modifiers after a Repeater are rare and the
        // tgfx renderer flattens the repeater after modifiers anyway.
        output = std::move(generated);
        shapeSlots.clear();
        break;
      }
      default:
        // Fill, Stroke, Text, TextBox, TextPath, TextModifier, RangeSelector,
        // and any other element pass through unchanged. They are written by
        // dedicated writers downstream.
        output.push_back(element);
        break;
    }
  }

  return output;
}

}  // namespace pagx
