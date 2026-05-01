// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAGX vector elements -> PAGDocument VectorPayload (Phase 5c + Phase 6
// PaintBaker). PaintBaker's ColorSource dispatch and Fill/Stroke bakers
// live in VectorBaker.cpp's anonymous namespace per design doc §19 Phase 6
// ("融入 VectorBaker 文件"); only the top-level payload entry is exported.
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
// check ctx.hasFatal() to detect mid-walk aborts. `doc` is threaded through
// so PaintBaker (Phase 6) can intern ImagePattern source images via
// ResourceBaker::RegisterImage.
std::unique_ptr<VectorPayload> BakeVectorPayload(BakeContext& ctx, PAGDocument& doc,
                                                 const std::vector<pagx::Element*>& contents);

}  // namespace pagx::pag
