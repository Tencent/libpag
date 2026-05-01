// Copyright (C) 2026 Tencent. All rights reserved.
//
// Top-level PAGX → PAGDocument conversion entry point. Delegates to
// ResourceBaker (pre-pass) + LayerBaker / CompositionBaker (main pass) +
// VectorBaker / TextBaker / StyleFilterBaker (payload handlers, landed
// incrementally across Phase 5-8).
//
// Authoritative spec: design doc §7 (submodule map) + §8.3bis (RAII error
// path) + §21 §B (pre-check requirements: applyLayout / import resolution).
#pragma once

#include <memory>
#include <vector>
#include "pagx/Diagnostic.h"
#include "pagx/pag/PAGDocument.h"

namespace pagx {
class PAGXDocument;
}

namespace pagx::pag {

struct BakeResult {
  // Non-null on success. nullptr whenever `errors` is non-empty (§8.1 contract).
  std::unique_ptr<PAGDocument> doc;
  std::vector<Diagnostic> errors;    // Baker fatal 100-199
  std::vector<Diagnostic> warnings;  // Baker degradation 200-299
};

// Convert a fully-resolved, layout-applied PAGX document into a PAGDocument
// tree. `pagxDoc` MUST satisfy:
//   * pagxDoc.isLayoutApplied() == true (otherwise emits LayoutNotApplied=100)
//   * pagxDoc.hasUnresolvedImports() == false (otherwise emits UnresolvedImports=101)
// Any layer whose `composition` pointer references a PAGX Composition is
// routed through CompositionBaker; unsupported payload types are placeholders
// this phase and will be populated by Phase 5-8 submodules.
BakeResult Bake(const pagx::PAGXDocument& pagxDoc);

}  // namespace pagx::pag
