// Copyright (C) 2026 Tencent. All rights reserved.
//
// Top-level Baker entry point. Pre-flight check + dispatch to LayerBaker
// (Phase 3) and (later) ResourceBaker / VectorBaker / TextBaker. Authoritative
// spec: design doc §7 + §8.3bis.
#include "pagx/pag/Baker.h"
#include <utility>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"
#include "pagx/pag/BakeContext.h"
#include "pagx/pag/ErrorCode.h"

namespace pagx::pag {

// Implemented in LayerBaker.cpp.
void BakeAllLayers(const pagx::PAGXDocument& pagxDoc, PAGDocument& doc, BakeContext& ctx);

namespace {

bool PreflightChecks(const pagx::PAGXDocument& pagxDoc, BakeContext& ctx) {
  if (!pagxDoc.isLayoutApplied()) {
    ctx.error(ErrorCode::LayoutNotApplied,
              "PAGXDocument must have applyLayout() called before Bake");
    return false;
  }
  if (pagxDoc.hasUnresolvedImports()) {
    ctx.error(ErrorCode::UnresolvedImports,
              "PAGXDocument has unresolved imports; run 'pagx resolve' first");
    return false;
  }
  return true;
}

}  // namespace

BakeResult Bake(const pagx::PAGXDocument& pagxDoc) {
  BakeResult result;
  BakeContext ctx;
  auto doc = std::make_unique<PAGDocument>();

  // 1) Header — canvas size carries over from the PAGX document. Background
  // colour and frame rate stay at their defaults (background black,
  // 24 fps × 1 frame) until Phase 5+ wires the corresponding PAGX fields.
  doc->header.width = pagxDoc.width;
  doc->header.height = pagxDoc.height;

  // 2) Pre-flight gate (LayoutNotApplied=100 / UnresolvedImports=101).
  if (!PreflightChecks(pagxDoc, ctx)) {
    doc.reset();
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  // 3) Empty-document short-circuit. EmptyDocument=207 is informational
  // (warning, not fatal) — Inflater treats {layer == nullptr, warning 604}
  // as a legitimate "empty but successful" outcome (§15.3 ok contract).
  if (pagxDoc.layers.empty()) {
    ctx.warn(ErrorCode::EmptyDocument, "PAGX document has no top-level layers");
    result.doc = std::move(doc);
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  // 4) Main pass — Phase 3 walks PAGX Layer trees and produces compositions
  // + the layer hierarchy with generic-field semantics. Per-payload bakers
  // (VectorBaker / TextBaker / ...) populate payloads in Phase 5-8.
  BakeAllLayers(pagxDoc, *doc, ctx);

  if (ctx.hasFatal()) {
    doc.reset();
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  // 5) Sanity: a bake without fatal MUST produce at least one composition,
  // otherwise downstream Codec / Inflater would crash on doc.compositions[0].
  if (doc->compositions.empty()) {
    ctx.error(ErrorCode::EmptyCompositions,
              "Baker produced no compositions despite non-empty PAGX layers");
    doc.reset();
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  result.doc = std::move(doc);
  result.warnings = std::move(ctx.warnings);
  return result;
}

}  // namespace pagx::pag
