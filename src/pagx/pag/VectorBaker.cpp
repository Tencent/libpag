// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 5c VectorBaker — translates a PAGX layer's `contents` (vector of
// pagx::Element*) into PAGDocument's VectorPayload. Phase 5c covers nine
// geometry / modifier element types (Rectangle, Ellipse, Polystar, Path,
// TrimPath, RoundCorner, MergePath, Repeater, Group). Fill / Stroke land in
// Phase 6 (PaintBaker), and Text / TextPath / TextModifier in Phase 8.
//
// PAGX nodes expose plain fields (no Property<T> wrapper) plus a NaN-sentinel
// convention for "unset" positions. The baker substitutes `Point{0, 0}` for
// NaN coordinates rather than calling renderPosition() — calling that would
// require applyLayout() to have produced layoutBounds, and the Phase 3
// applyLayout perf cliff (memory: feedback_pagx_layout_perf.md) makes that
// brittle for synthetic test inputs.
#include "pagx/pag/VectorBaker.h"
#include <cmath>
#include <memory>
#include <utility>
#include <vector>
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Node.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/pag/BakeContext.h"
#include "pagx/pag/PAGDocument.h"
#include "renderer/ToTGFX.h"

namespace pagx::pag {

namespace {

// PAGX uses NAN as "position not explicitly set". The baker replaces it with
// {0,0} so Property<Point> stays well-formed without calling renderPosition()
// (which requires layout to have run).
tgfx::Point ResolvePoint(const pagx::Point& p) {
  float x = std::isnan(p.x) ? 0.0f : p.x;
  float y = std::isnan(p.y) ? 0.0f : p.y;
  return tgfx::Point{x, y};
}

tgfx::Point SizeToPoint(const pagx::Size& s) {
  return tgfx::Point{s.width, s.height};
}

// pagx::PolystarType is { Polygon=0, Star=1 } but tgfx::PolystarType is
// { Star=0, Polygon=1 } — direct cast would flip the meaning. Map by name.
tgfx::PolystarType MapPolystarType(pagx::PolystarType t) {
  return t == pagx::PolystarType::Star ? tgfx::PolystarType::Star : tgfx::PolystarType::Polygon;
}

// pagx::TrimType { Separate=0, Continuous=1 } matches tgfx::TrimPathType
// { Separate=0, Continuous=1 } — but ToTGFX has no overload for it, so map
// explicitly to keep the dependency tight.
tgfx::TrimPathType MapTrimType(pagx::TrimType t) {
  return t == pagx::TrimType::Continuous ? tgfx::TrimPathType::Continuous
                                         : tgfx::TrimPathType::Separate;
}

std::unique_ptr<VectorElement> BakeElement(BakeContext& ctx, const pagx::Element* src);

std::unique_ptr<VectorElement> BakeRectangle(const pagx::Rectangle& src) {
  auto data = std::make_unique<ElementRectangleData>();
  data->position = MakeProp(ResolvePoint(src.position));
  data->size = MakeProp(SizeToPoint(src.size));
  data->roundness = MakeProp(src.roundness);
  data->reversed = src.reversed;

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::Rectangle;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeEllipse(const pagx::Ellipse& src) {
  auto data = std::make_unique<ElementEllipseData>();
  data->position = MakeProp(ResolvePoint(src.position));
  data->size = MakeProp(SizeToPoint(src.size));
  data->reversed = src.reversed;

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::Ellipse;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakePolystar(const pagx::Polystar& src) {
  auto data = std::make_unique<ElementPolystarData>();
  data->position = MakeProp(ResolvePoint(src.position));
  data->pointCount = MakeProp(src.pointCount);
  data->outerRadius = MakeProp(src.outerRadius);
  data->innerRadius = MakeProp(src.innerRadius);
  data->outerRoundness = MakeProp(src.outerRoundness);
  data->innerRoundness = MakeProp(src.innerRoundness);
  data->rotation = MakeProp(src.rotation);
  data->reversed = src.reversed;
  data->polystarType = MapPolystarType(src.type);

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::Polystar;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakePath(const pagx::Path& src) {
  auto data = std::make_unique<ElementShapePathData>();
  data->position = MakeProp(ResolvePoint(src.position));
  if (src.data != nullptr) {
    data->path = MakeProp(pagx::ToTGFX(*src.data));
  }
  data->reversed = src.reversed;

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::ShapePath;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeTrimPath(const pagx::TrimPath& src) {
  auto data = std::make_unique<ElementTrimPathData>();
  data->start = MakeProp(src.start);
  data->end = MakeProp(src.end);
  data->offset = MakeProp(src.offset);
  data->trimType = MapTrimType(src.type);

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::TrimPath;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeRoundCorner(const pagx::RoundCorner& src) {
  auto data = std::make_unique<ElementRoundCornerData>();
  data->radius = MakeProp(src.radius);

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::RoundCorner;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeMergePath(const pagx::MergePath& src) {
  auto data = std::make_unique<ElementMergePathData>();
  data->mode = pagx::ToTGFX(src.mode);

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::MergePath;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeRepeater(const pagx::Repeater& src) {
  auto data = std::make_unique<ElementRepeaterData>();
  data->copies = MakeProp(src.copies);
  data->offset = MakeProp(src.offset);
  data->order = pagx::ToTGFX(src.order);
  data->anchor = MakeProp(tgfx::Point{src.anchor.x, src.anchor.y});
  data->position = MakeProp(tgfx::Point{src.position.x, src.position.y});
  data->rotation = MakeProp(src.rotation);
  data->scale = MakeProp(tgfx::Point{src.scale.x, src.scale.y});
  data->startAlpha = MakeProp(src.startAlpha);
  data->endAlpha = MakeProp(src.endAlpha);

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::Repeater;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeGroup(BakeContext& ctx, const pagx::Group& src) {
  if (!ctx.enterVectorGroup()) {
    return nullptr;
  }
  auto data = std::make_unique<ElementVectorGroupData>();
  data->anchor = MakeProp(tgfx::Point{src.anchor.x, src.anchor.y});
  data->position = MakeProp(tgfx::Point{src.position.x, src.position.y});
  data->scale = MakeProp(tgfx::Point{src.scale.x, src.scale.y});
  data->rotation = MakeProp(src.rotation);
  data->alpha = MakeProp(src.alpha);
  data->skew = MakeProp(src.skew);
  data->skewAxis = MakeProp(src.skewAxis);

  for (auto* child : src.elements) {
    if (ctx.hasFatal()) {
      break;
    }
    auto baked = BakeElement(ctx, child);
    if (baked != nullptr) {
      data->elements.push_back(std::move(baked));
    }
  }

  ctx.exitVectorGroup();

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::VectorGroup;
  el->payload = std::move(data);
  return el;
}

// Single-element dispatch by NodeType. Phase 5c silently skips Fill / Stroke
// / Text-family elements with a debug-only warn — Phase 6 (PaintBaker) and
// Phase 8 (TextBaker) will plug them in by widening the switch.
std::unique_ptr<VectorElement> BakeElement(BakeContext& ctx, const pagx::Element* src) {
  if (src == nullptr) {
    return nullptr;
  }
  switch (src->nodeType()) {
    case pagx::NodeType::Rectangle:
      return BakeRectangle(*static_cast<const pagx::Rectangle*>(src));
    case pagx::NodeType::Ellipse:
      return BakeEllipse(*static_cast<const pagx::Ellipse*>(src));
    case pagx::NodeType::Polystar:
      return BakePolystar(*static_cast<const pagx::Polystar*>(src));
    case pagx::NodeType::Path:
      return BakePath(*static_cast<const pagx::Path*>(src));
    case pagx::NodeType::TrimPath:
      return BakeTrimPath(*static_cast<const pagx::TrimPath*>(src));
    case pagx::NodeType::RoundCorner:
      return BakeRoundCorner(*static_cast<const pagx::RoundCorner*>(src));
    case pagx::NodeType::MergePath:
      return BakeMergePath(*static_cast<const pagx::MergePath*>(src));
    case pagx::NodeType::Repeater:
      return BakeRepeater(*static_cast<const pagx::Repeater*>(src));
    case pagx::NodeType::Group:
      return BakeGroup(ctx, *static_cast<const pagx::Group*>(src));
    case pagx::NodeType::Fill:
    case pagx::NodeType::Stroke:
    case pagx::NodeType::Text:
    case pagx::NodeType::TextModifier:
    case pagx::NodeType::TextPath:
    case pagx::NodeType::TextBox:
      // Phase 6 / Phase 8 will land these. Silent skip keeps the layer
      // shape intact — downstream readers see a smaller VectorPayload but
      // do not error.
      return nullptr;
    default:
      // Layer / resource / filter / style nodes are not valid Element
      // subtypes for `Layer::contents`; the PAGX importer should have
      // filtered them out. Defensive skip.
      return nullptr;
  }
}

}  // namespace

std::unique_ptr<VectorPayload> BakeVectorPayload(BakeContext& ctx,
                                                 const std::vector<pagx::Element*>& contents) {
  auto out = std::make_unique<VectorPayload>();
  for (auto* el : contents) {
    if (ctx.hasFatal()) {
      break;
    }
    auto baked = BakeElement(ctx, el);
    if (baked != nullptr) {
      out->contents.push_back(std::move(baked));
    }
  }
  return out;
}

}  // namespace pagx::pag
