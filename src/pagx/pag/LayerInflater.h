// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 9 LayerInflater — PAGDocument → tgfx::Layer tree converter. Mirrors
// src/renderer/LayerBuilder.cpp (the authoritative "PAGX → tgfx::Layer"
// mapping), so every setter sequence here is intentionally parallel to
// LayerBuilder::applyLayerAttributes / convertLayer / convertVectorElement /
// convertColorSource — three-way synchrony (Baker / Codec / Inflater) with
// LayerBuilder as the single source of truth (§8.1).

#pragma once

#include <memory>
#include <vector>
#include "pagx/Diagnostic.h"
#include "pagx/pag/FontProvider.h"
#include "pagx/pag/PAGDocument.h"
#include "tgfx/layers/Layer.h"

namespace pagx::pag {

class LayerInflater {
 public:
  struct Options {
    // Font resolver used by text inflation. If null, Inflate() falls back to
    // MakeDefaultFontProvider() — see include/pagx/pag/FontProvider.h and
    // docs/pagx_to_pag_v2_phase16_text_redesign.md §5.2.
    std::shared_ptr<FontProvider> fontProvider = nullptr;

    // Explicit default constructor — makes the default-arg
    // `Inflate(..., Options opts = {})` resolve during the class body
    // (same clang quirk PAGLoader::Options / PAGExporter::Options work
    // around).
    Options() {
    }
  };

  struct Result {
    // Non-null on success. nullptr iff the document has no renderable content
    // (empty compositions / compositions[0] empty); in that case the
    // warnings vector holds exactly one InflaterEmptyDocument (604).
    std::shared_ptr<tgfx::Layer> layer = nullptr;
    std::vector<Diagnostic> warnings = {};
  };

  // Inflate a PAGDocument into a tgfx::Layer tree.
  //
  // Takes ownership of `doc` so per-ImageAsset::data release after
  // tgfx::Image::MakeFromEncoded can happen without racing the caller
  // (§11.1 "Inflater 生命周期纪律").
  //
  // Never produces fatal errors — per-layer / per-element failures degrade
  // into warnings (600-699, see §9.4) and the pipeline continues.
  static Result Inflate(std::unique_ptr<PAGDocument> doc, Options opts = {});
};

}  // namespace pagx::pag
