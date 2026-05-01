// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 7 StyleFilterBaker — translates a PAGX layer's `filters` and `styles`
// vectors into PAGDocument's LayerFilter / LayerStyle lists. Lives behind
// its own header (rather than folded into LayerBaker.cpp) because the Tag
// byte writers in StyleFilterTags.cpp already form a separate unit — the
// baker and the codec change together when design doc §D.12 adds a new
// filter / style subtype.
#pragma once

#include <memory>
#include <vector>

namespace pagx {
class LayerFilter;
class LayerStyle;
}  // namespace pagx

namespace pagx::pag {

struct BakeContext;
struct LayerFilter;
struct LayerStyle;

// Bake a PAGX layer's `filters` vector into PAGDocument's LayerFilter list.
// Unknown PAGX node subtypes are skipped with an InvalidEnumValue warning so
// a partially-supported source tree still produces a usable PAG document.
void BakeLayerFilters(BakeContext& ctx, const std::vector<pagx::LayerFilter*>& src,
                      std::vector<std::unique_ptr<LayerFilter>>* out);

// Mirrors BakeLayerFilters for PAGX LayerStyle vectors.
void BakeLayerStyles(BakeContext& ctx, const std::vector<pagx::LayerStyle*>& src,
                     std::vector<std::unique_ptr<LayerStyle>>* out);

}  // namespace pagx::pag
