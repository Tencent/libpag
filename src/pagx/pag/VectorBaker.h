// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAGX vector elements -> PAGDocument VectorPayload (Phase 5c).
// LayerBaker calls into this when the source PAGX layer has a non-empty
// `contents` vector.
#pragma once

#include <memory>
#include <vector>
#include "pagx/pag/PAGDocument.h"

namespace pagx {
class Element;
}

namespace pagx::pag {

struct BakeContext;

// Bake a PAGX layer's `contents` vector into PAGDocument's VectorPayload.
// Always returns a non-null pointer (empty payload if nothing baked); callers
// check ctx.hasFatal() to detect mid-walk aborts.
std::unique_ptr<VectorPayload> BakeVectorPayload(BakeContext& ctx,
                                                 const std::vector<pagx::Element*>& contents);

}  // namespace pagx::pag
