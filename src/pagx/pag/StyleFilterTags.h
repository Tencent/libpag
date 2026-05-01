// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 7 — byte-level codec for LayerFilters / LayerStyles sub-Tags plus the
// 5 Filter Tags (TagCode 120-124) and 3 Style Tags (TagCode 140-142) they
// wrap. Wire layout is pinned by design doc §D.9 (container) + §D.12 (per-
// filter / per-style body) and MUST NOT drift from those tables.
//
// Split into its own header rather than bolted onto CodecTags.h because the
// Filter/Style section is structurally separate (its own TagCode segment,
// its own baker in Phase 7 StyleFilterBaker), and the header already has a
// full dependency graph on PAGDocument + DecodeContext + EncodeSession.
#pragma once

#include <memory>
#include <vector>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pagx/pag/DecodeContext.h"
#include "pagx/pag/EncodeSession.h"
#include "pagx/pag/PAGDocument.h"

namespace pagx::pag {

// ---- LayerFilters container (TagCode = 13) ----
// body: varU32 filterCount, repeat filterCount × [Filter<Type> Tag]
// Only emitted when `filters` is non-empty; the LayerBlock writer guards
// that before calling in (§D.9 + §D.11 "仅当 !filters.empty()" clause).
void WriteLayerFilters(::pag::EncodeStream* stream,
                       const std::vector<std::unique_ptr<LayerFilter>>& filters,
                       EncodeSession* session);

void ReadLayerFilters(::pag::DecodeStream* stream, DecodeContext* ctx, uint64_t tagEnd,
                      std::vector<std::unique_ptr<LayerFilter>>* out);

// ---- LayerStyles container (TagCode = 14) ----
// body: varU32 styleCount, repeat styleCount × [Style<Type> Tag]
void WriteLayerStyles(::pag::EncodeStream* stream,
                      const std::vector<std::unique_ptr<LayerStyle>>& styles,
                      EncodeSession* session);

void ReadLayerStyles(::pag::DecodeStream* stream, DecodeContext* ctx, uint64_t tagEnd,
                     std::vector<std::unique_ptr<LayerStyle>>* out);

}  // namespace pagx::pag
