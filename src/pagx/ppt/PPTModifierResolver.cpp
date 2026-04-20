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
Element* PPTModifierResolver::makePathFromData(PathData* data) const {
  auto* p = _doc->makeNode<Path>();
  p->data = data;
  p->position = {0.0f, 0.0f};
  return p;
}

//==============================================================================
// Polystar -> tgfx::Path (mirrors tgfx/src/layers/vectors/Polystar.cpp).
// PAGX uses degrees for rotation; tgfx::Polystar internals use radians too.
//==============================================================================

static constexpr float kStarRoundnessKnot = 0.47829f;

static tgfx::Path StarToTGFXPath(const Polystar* star) {
  tgfx::Path path = {};
  int points = std::max(2, static_cast<int>(std::round(star->pointCount)));
  float currentAngle = (star->rotation - 90.0f) * static_cast<float>(M_PI) / 180.0f;
  float anglePerPoint = static_cast<float>(M_PI) / static_cast<float>(points);
  bool longSegment = false;
  float longRound = star->outerRoundness * 0.01f;
  float shortRound = star->innerRoundness * 0.01f;
  float longRadius = star->outerRadius;
  float shortRadius = star->innerRadius;

  if (star->reversed) {
    anglePerPoint = -anglePerPoint;
  }

  float x = longRadius * std::cos(currentAngle) + star->position.x;
  float y = longRadius * std::sin(currentAngle) + star->position.y;
  path.moveTo(x, y);
  currentAngle += anglePerPoint;

  int totalPoints = points * 2;
  float prevDx = 0.0f;
  float prevDy = 0.0f;
  for (int i = 0; i < totalPoints; ++i) {
    float radius = longSegment ? longRadius : shortRadius;
    float roundness = longSegment ? longRound : shortRound;
    float prevRadius = longSegment ? shortRadius : longRadius;
    float prevRoundness = longSegment ? shortRound : longRound;

    float nx = radius * std::cos(currentAngle) + star->position.x;
    float ny = radius * std::sin(currentAngle) + star->position.y;

    if (roundness == 0.0f && prevRoundness == 0.0f) {
      path.lineTo(nx, ny);
    } else {
      float cp1x = x - prevDx * prevRoundness * prevRadius * kStarRoundnessKnot;
      float cp1y = y - prevDy * prevRoundness * prevRadius * kStarRoundnessKnot;
      float ndx = -std::sin(currentAngle);
      float ndy = std::cos(currentAngle);
      if (star->reversed) {
        ndx = -ndx;
        ndy = -ndy;
      }
      float cp2x = nx + ndx * roundness * radius * kStarRoundnessKnot;
      float cp2y = ny + ndy * roundness * radius * kStarRoundnessKnot;
      path.cubicTo(cp1x, cp1y, cp2x, cp2y, nx, ny);
      prevDx = ndx;
      prevDy = ndy;
    }

    x = nx;
    y = ny;
    currentAngle += anglePerPoint;
    longSegment = !longSegment;
  }
  path.close();
  return path;
}

static tgfx::Path PolygonToTGFXPath(const Polystar* poly) {
  tgfx::Path path = {};
  int points = std::max(3, static_cast<int>(std::round(poly->pointCount)));
  float currentAngle = (poly->rotation - 90.0f) * static_cast<float>(M_PI) / 180.0f;
  float anglePerPoint = 2.0f * static_cast<float>(M_PI) / static_cast<float>(points);
  float radius = poly->outerRadius;
  float roundness = poly->outerRoundness * 0.01f;

  if (poly->reversed) {
    anglePerPoint = -anglePerPoint;
  }

  float x = radius * std::cos(currentAngle) + poly->position.x;
  float y = radius * std::sin(currentAngle) + poly->position.y;
  path.moveTo(x, y);
  currentAngle += anglePerPoint;

  for (int i = 0; i < points; ++i) {
    float nx = radius * std::cos(currentAngle) + poly->position.x;
    float ny = radius * std::sin(currentAngle) + poly->position.y;
    if (roundness == 0.0f) {
      path.lineTo(nx, ny);
    } else {
      float dx = -std::sin(currentAngle - anglePerPoint);
      float dy = std::cos(currentAngle - anglePerPoint);
      float ndx = -std::sin(currentAngle);
      float ndy = std::cos(currentAngle);
      if (poly->reversed) {
        dx = -dx;
        dy = -dy;
        ndx = -ndx;
        ndy = -ndy;
      }
      float cp1x = x - dx * roundness * radius * kStarRoundnessKnot;
      float cp1y = y - dy * roundness * radius * kStarRoundnessKnot;
      float cp2x = nx + ndx * roundness * radius * kStarRoundnessKnot;
      float cp2y = ny + ndy * roundness * radius * kStarRoundnessKnot;
      path.cubicTo(cp1x, cp1y, cp2x, cp2y, nx, ny);
    }
    x = nx;
    y = ny;
    currentAngle += anglePerPoint;
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
    if (p->position.x != 0.0f || p->position.y != 0.0f) {
      tgfx::Matrix m = tgfx::Matrix::MakeTrans(p->position.x, p->position.y);
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
    float halfW = r->size.width * 0.5f;
    float halfH = r->size.height * 0.5f;
    tgfx::Rect rect = tgfx::Rect::MakeLTRB(r->position.x - halfW, r->position.y - halfH,
                                           r->position.x + halfW, r->position.y + halfH);
    if (r->roundness > 0.0f) {
      float rad = std::min({r->roundness, halfW, halfH});
      tp.addRoundRect(rect, rad, rad, r->reversed);
    } else {
      tp.addRect(rect, r->reversed);
    }
    return tp;
  }
  if (type == NodeType::Ellipse) {
    auto* e = static_cast<const Ellipse*>(el);
    float halfW = e->size.width * 0.5f;
    float halfH = e->size.height * 0.5f;
    tgfx::Rect rect = tgfx::Rect::MakeLTRB(e->position.x - halfW, e->position.y - halfH,
                                           e->position.x + halfW, e->position.y + halfH);
    tgfx::Path tp = {};
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

static Element* ApplyTrimToElement(PAGXDocument* doc, Element* shape, const TrimPath* trim) {
  auto tp = PrimitiveToTGFXPath(shape);
  if (tp.isEmpty()) {
    return shape;
  }
  float offset = trim->offset / 360.0f;
  auto effect = tgfx::PathEffect::MakeTrim(trim->start + offset, trim->end + offset);
  if (effect == nullptr || !effect->filterPath(&tp)) {
    return shape;
  }
  auto* pd = MakePathDataFromTGFX(doc, tp);
  auto* p = doc->makeNode<Path>();
  p->data = pd;
  return p;
}

static Element* ApplyRoundCornerToElement(PAGXDocument* doc, Element* shape,
                                          const RoundCorner* corner) {
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
  auto* pd = MakePathDataFromTGFX(doc, tp);
  auto* p = doc->makeNode<Path>();
  p->data = pd;
  return p;
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
            output[idx] = ApplyTrimToElement(_doc, output[idx], trim);
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
            auto* pd = MakePathDataFromTGFX(_doc, combined);
            auto* path = makePathFromData(pd);
            // Replace the first slot with the merged trimmed path; remove the
            // remaining slots from output (right-to-left to keep indices valid).
            size_t firstSlot = shapeSlots.front();
            output[firstSlot] = path;
            for (auto it = shapeSlots.rbegin(); it != shapeSlots.rend(); ++it) {
              if (*it != firstSlot) {
                output.erase(output.begin() + static_cast<long>(*it));
              }
            }
            shapeSlots.clear();
            shapeSlots.push_back(firstSlot);
          }
        }
        break;
      }
      case NodeType::RoundCorner: {
        auto* rc = static_cast<const RoundCorner*>(element);
        for (auto idx : shapeSlots) {
          output[idx] = ApplyRoundCornerToElement(_doc, output[idx], rc);
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
        auto* pd = MakePathDataFromTGFX(_doc, combined);
        auto* path = makePathFromData(pd);
        size_t firstSlot = shapeSlots.front();
        output[firstSlot] = path;
        for (auto it = shapeSlots.rbegin(); it != shapeSlots.rend(); ++it) {
          if (*it != firstSlot) {
            output.erase(output.begin() + static_cast<long>(*it));
          }
        }
        shapeSlots.clear();
        shapeSlots.push_back(firstSlot);
        break;
      }
      case NodeType::Repeater: {
        auto* rep = static_cast<const Repeater*>(element);
        int copies = std::max(0, static_cast<int>(std::round(rep->copies)));
        if (copies <= 1 || shapeSlots.empty()) {
          break;
        }
        // Snapshot the shape elements that are subject to the repeater. We wrap
        // the snapshot in a Group per copy so the per-copy transform is a
        // single, atomic transform. The original copy (index 0) is kept in
        // place to avoid double-painting; copies 1..n-1 get their own groups.
        std::vector<Element*> body;
        body.reserve(shapeSlots.size());
        for (auto idx : shapeSlots) {
          body.push_back(output[idx]);
        }

        std::vector<Element*> generated;
        generated.reserve(static_cast<size_t>(copies - 1));
        for (int c = 1; c < copies; ++c) {
          float t = static_cast<float>(c) - rep->offset;
          float rotation = rep->rotation * t;
          float sx = std::pow(rep->scale.x, t);
          float sy = std::pow(rep->scale.y, t);
          float copyAlpha = 1.0f;
          if (copies > 1 && (rep->startAlpha != 1.0f || rep->endAlpha != 1.0f)) {
            float frac = static_cast<float>(c) / static_cast<float>(copies - 1);
            copyAlpha = rep->startAlpha + (rep->endAlpha - rep->startAlpha) * frac;
          }
          auto* g = MakeCopyGroup(_doc, body, rep->position.x * t, rep->position.y * t, rotation,
                                  sx, sy, rep->anchor.x, rep->anchor.y, copyAlpha);
          generated.push_back(g);
        }

        // Insert the generated copies before/after the existing shape slots so
        // RepeaterOrder is honoured. BelowOriginal -> generated copies come
        // first (drawn before) so the original is on top.
        size_t insertPos = (rep->order == RepeaterOrder::BelowOriginal) ? shapeSlots.front()
                                                                       : (shapeSlots.back() + 1);
        output.insert(output.begin() + static_cast<long>(insertPos), generated.begin(),
                      generated.end());

        // Bump every existing slot index that was at or after insertPos.
        for (auto& s : shapeSlots) {
          if (s >= insertPos) {
            s += generated.size();
          }
        }
        // Generated groups should themselves participate as shape slots in case
        // a later modifier (e.g. a downstream Trim) operates on them. Skip this
        // for simplicity — chained modifiers after a Repeater are rare and the
        // current tgfx semantics flatten the repeater after modifiers anyway.
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
