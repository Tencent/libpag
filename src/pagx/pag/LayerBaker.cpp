// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 3 Baker submodule — handles Layer common fields plus type dispatch
// (Layer container vs CompositionRef). Per-payload bakers (Vector / Text /
// Style / Filter) populate `Layer::vector` / `Layer::text` / etc. in
// Phase 5-8 and are intentionally out of scope here.
//
// Authoritative mapping rules: design doc §7 + appendix A line numbers
// (LayerBuilder.cpp 166-202 / 205-218 / 731-796).
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"
#include "pagx/pag/BakeContext.h"
#include "pagx/pag/Baker.h"
#include "pagx/pag/ErrorCode.h"
#include "pagx/pag/PAGDocument.h"
#include "pagx/pag/PropertyEncoding.h"
#include "pagx/pag/VectorBaker.h"
#include "renderer/ToTGFX.h"

namespace pagx::pag {

namespace {

// Convert pagx::Matrix3D (column-major float[16]) to tgfx::Matrix3D using the
// public setRowColumn API. Mirrors the round-trip we exercised in Phase 1
// ValueCodec for the wire format — same column-major semantics.
tgfx::Matrix3D ToTgfxMatrix3D(const pagx::Matrix3D& m) {
  tgfx::Matrix3D out{};
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      out.setRowColumn(r, c, m.getRowColumn(r, c));
    }
  }
  return out;
}

// Compose pagx::Layer.x / y onto its 2D matrix to produce the PAGDocument
// Property<Matrix>. PAGX stores translate as separate (x, y) fields with the
// rest of the affine in `matrix`; PAG v2 stores everything in a single Matrix.
pag::Matrix BuildLayerMatrix(const pagx::Layer& src) {
  // Start from the PAGX matrix (typically identity for documents without
  // explicit transforms) then add (x, y) into the translate component.
  pag::Matrix m = pagx::ToTGFX(src.matrix);
  if (src.x != 0.0f || src.y != 0.0f) {
    m.setTranslateX(m.getTranslateX() + src.x);
    m.setTranslateY(m.getTranslateY() + src.y);
  }
  return m;
}

// Forward declarations for the recursive walkers.
struct LayerWalker {
  BakeContext& ctx;
  PAGDocument& doc;
  // Maps every PAGX Composition* we have already interned to its index inside
  // doc.compositions[]. This is what turns a CompositionRef chain into a
  // graph instead of a tree (cycles are ultimately caught by the Inflater
  // composition-cycle check in Phase 9).
  std::unordered_map<const pagx::Composition*, uint32_t> compositionIndex;
  // Pass-1 layer-path table: each PAGX Layer pointer maps to its chain of
  // child indices from the enclosing root, used by Pass-2 mask resolution.
  // Lives on the local walker rather than BakeContext because Phase 3 only
  // needs it inside this single Bake() call.
  //
  // Phase 5c populates this table for the entire layer subtree before
  // Pass 2 (bakeLayerList) starts so mask refs that point forward in the
  // sibling order still resolve correctly.
  std::unordered_map<const pagx::Layer*, std::vector<uint32_t>> layerPath;

  // Tracks which compositions are currently on the bake stack so we can
  // detect Composition-level cycles before we recurse forever.
  std::unordered_set<const pagx::Composition*> activeCompositions;

  // Mask Pass 1 — walk the layer tree once and record a child-index path
  // for every Layer*. The path is "chain of child indices from the root of
  // the enclosing composition", so a top-level layer at root index 2 has
  // path = [2]; its first child has [2, 0]; etc.
  void recordLayerPaths(const std::vector<pagx::Layer*>& src,
                        const std::vector<uint32_t>& parentPath) {
    for (size_t i = 0; i < src.size(); ++i) {
      if (src[i] == nullptr) {
        continue;
      }
      std::vector<uint32_t> here = parentPath;
      here.push_back(static_cast<uint32_t>(i));
      layerPath.emplace(src[i], here);
      recordLayerPaths(src[i]->children, here);
    }
  }

  // Returns the PAGDocument index of the given composition, baking it on
  // first sight. Returns UINT32_MAX when a fatal occurred (caller must
  // bubble up).
  uint32_t internComposition(const pagx::Composition* compPtr) {
    auto it = compositionIndex.find(compPtr);
    if (it != compositionIndex.end()) {
      return it->second;
    }
    if (activeCompositions.count(compPtr) != 0) {
      // Composition self/chain reference at Bake time — defensive guard so
      // we never recurse forever; the Inflater layer (Phase 9) is the
      // canonical owner of cycle reporting (InflateCompositionCycle=605).
      return UINT32_MAX;
    }
    if (!ctx.registerComposition()) {
      return UINT32_MAX;
    }
    uint32_t newIndex = static_cast<uint32_t>(doc.compositions.size());
    auto comp = std::make_unique<Composition>();
    comp->width = static_cast<uint32_t>(compPtr->width);
    comp->height = static_cast<uint32_t>(compPtr->height);
    doc.compositions.push_back(std::move(comp));
    compositionIndex.emplace(compPtr, newIndex);

    activeCompositions.insert(compPtr);
    // Mask Pass 1 — populate the path table for every layer in this
    // composition's subtree before Pass 2 starts. We clear the per-composition
    // entries on entry so paths from a sibling composition (different
    // child-index basis) do not leak into the lookup.
    layerPath.clear();
    recordLayerPaths(compPtr->layers, {});
    bool fatal = false;
    bakeLayerList(compPtr->layers, doc.compositions[newIndex]->layers, fatal);
    activeCompositions.erase(compPtr);
    if (fatal) {
      return UINT32_MAX;
    }
    return newIndex;
  }

  // Walk a PAGX layer list and append baked Layer objects to `out`. Sets
  // `fatal = true` if BakeContext recorded a fatal mid-walk so the caller
  // can stop early without finishing the loop.
  void bakeLayerList(const std::vector<pagx::Layer*>& src, std::vector<std::unique_ptr<Layer>>& out,
                     bool& fatal) {
    for (size_t i = 0; i < src.size(); ++i) {
      if (ctx.hasFatal()) {
        fatal = true;
        return;
      }
      auto baked = bakeLayer(src[i], static_cast<uint32_t>(i));
      if (baked) {
        out.push_back(std::move(baked));
      }
    }
  }

  // Bake a single PAGX Layer (returns nullptr when the budget cap fires —
  // ctx.errors will have been populated before this returns).
  std::unique_ptr<Layer> bakeLayer(const pagx::Layer* src, uint32_t /*indexInParent*/) {
    if (src == nullptr) {
      return nullptr;
    }
    if (!ctx.enterLayer()) {
      return nullptr;
    }
    auto out = std::make_unique<Layer>();

    // ---- Type dispatch ----
    if (src->composition != nullptr) {
      out->type = LayerType::CompositionRef;
      uint32_t childIndex = internComposition(src->composition);
      out->compositionIndex = childIndex;
    } else if (!src->contents.empty()) {
      // Phase 5c: a PAGX layer with vector contents becomes a Vector layer
      // in PAGDocument. VectorBaker handles per-element dispatch.
      out->type = LayerType::Vector;
      out->vector = BakeVectorPayload(ctx, src->contents);
    } else {
      // Generic Layer container (no payload). Phase 6-8 add Image / Solid /
      // Text / Mesh dispatch when their bakers come online.
      out->type = LayerType::Layer;
    }

    // ---- Common fields ----
    out->name = src->name;
    out->startTime = 0;
    out->duration = 1;
    out->stretch = {1, 1};

    out->visible = MakeProp(src->visible);
    out->alpha = MakeProp(src->alpha);
    out->blendMode = MakeProp(pagx::ToTGFX(src->blendMode));

    out->matrix = MakeProp(BuildLayerMatrix(*src));
    out->matrix3D = MakeProp(ToTgfxMatrix3D(src->matrix3D));

    out->scrollRect = MakeProp(pagx::ToTGFX(src->scrollRect));
    out->hasScrollRect = src->hasScrollRect;

    out->preserve3D = src->preserve3D;
    out->passThroughBackground = src->passThroughBackground;
    out->allowsEdgeAntialiasing = src->antiAlias;
    out->allowsGroupOpacity = src->groupOpacity;

    // Mask resolution (§12.1 Pass 2). Pass 1 (recordLayerPaths above) has
    // already walked the entire layer subtree of the current composition, so
    // forward sibling references resolve cleanly here.
    if (src->mask != nullptr) {
      if (src->mask == src) {
        // Self-reference is a Baker-side fatal-warn: leave maskLayerPath
        // empty, the Inflater layer (Phase 9) will not bind a mask.
        ctx.warn(ErrorCode::MaskSelfReference, "layer.mask points to itself");
      } else {
        auto it = layerPath.find(src->mask);
        if (it == layerPath.end()) {
          // Target lives in a different composition (or is dangling) —
          // §12.1 says emit MaskTargetMissing and leave path empty.
          ctx.warn(ErrorCode::MaskTargetMissing,
                   "layer.mask points outside the current composition");
        } else {
          out->maskLayerPath = it->second;
        }
      }
      out->maskType = static_cast<LayerMaskType>(pagx::ToTGFXMaskType(src->maskType));
    }

    // ---- Children recursion ----
    bool nestedFatal = false;
    bakeLayerList(src->children, out->children, nestedFatal);

    ctx.exitLayer();
    return out;
  }
};

}  // namespace

void BakeAllLayers(const pagx::PAGXDocument& pagxDoc, PAGDocument& doc, BakeContext& ctx) {
  LayerWalker walker{ctx, doc, {}, {}, {}};

  // Implicit root composition: every top-level PAGX layer becomes a child of
  // composition 0. PAGX has no first-class "root composition" concept, so
  // PAG v2 synthesizes one to satisfy the §5.1 invariant
  // ("compositions[0] is the root").
  if (!ctx.registerComposition()) {
    return;
  }
  auto rootComp = std::make_unique<Composition>();
  rootComp->width = static_cast<uint32_t>(pagxDoc.width);
  rootComp->height = static_cast<uint32_t>(pagxDoc.height);
  doc.compositions.push_back(std::move(rootComp));
  uint32_t rootIndex = 0;

  bool fatal = false;
  // Mask Pass 1 for the root composition's subtree.
  walker.layerPath.clear();
  walker.recordLayerPaths(pagxDoc.layers, {});
  walker.bakeLayerList(pagxDoc.layers, doc.compositions[rootIndex]->layers, fatal);
}

}  // namespace pagx::pag
