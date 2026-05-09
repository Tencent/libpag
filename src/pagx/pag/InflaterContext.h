// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 9 InflaterContext — state carrier for LayerInflater::Inflate.
//
// Unlike DecodeContext (which must police byte-level DoS from adversarial
// streams) the Inflater operates on a pre-validated PAGDocument, so this
// context is mainly a warnings collector + the mask two-pass state + the
// composition-cycle guard. Inherits DiagnosticCollector::warnings and
// intentionally does NOT override pushError — the base class's no-op
// pushError physically enforces the "Inflater has no fatal" contract
// (§9.4 / §8.5).

#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include "pagx/pag/DiagnosticCollector.h"
#include "pagx/pag/FontProvider.h"
#include "pagx/pag/PAGDocument.h"
#include "pagx/pag/limits.h"
#include "tgfx/layers/Layer.h"

namespace pagx::pag {

// Packed Layer path key for the Pass-1 `layerByPath` lookup. Holds the full
// chain of child indices from the enclosing Composition root down to the
// target Layer (≤ limits::MAX_MASK_PATH_DEPTH levels deep in practice; no
// hard cap here — callers enforce depth at encode/decode boundaries).
//
// Pre-v2.25 this was a uint64 bit-packed (5 × 10-bit index slots + 6-bit
// depth). That representation truncated beyond 5 levels, so legitimate
// `resources/pagx_to_html/clip_and_mask.pagx` (mask path depth 7) could
// not round-trip and also risked silent hash collisions with unrelated
// paths sharing a 5-level prefix. Current representation stores the path
// verbatim so equality is exact and the only cost is one heap allocation
// per entry.
struct PackedLayerPath {
  std::vector<uint32_t> path;
  bool operator==(const PackedLayerPath& o) const {
    return path == o.path;
  }
};

struct PackedLayerPathHash {
  size_t operator()(const PackedLayerPath& p) const {
    // FNV-1a-style fold over the index chain. Ample entropy for the
    // bounded (≤ MAX_MASK_PATH_DEPTH × uint32) inputs; rehashing hot
    // loops is not on the critical path.
    size_t h = 1469598103934665603ull;
    for (uint32_t idx : p.path) {
      h ^= static_cast<size_t>(idx);
      h *= 1099511628211ull;
    }
    return h;
  }
};

inline PackedLayerPath PackLayerPath(const uint32_t* data, size_t n) {
  PackedLayerPath out{};
  out.path.assign(data, data + n);
  return out;
}

inline PackedLayerPath PackLayerPath(const std::vector<uint32_t>& path) {
  PackedLayerPath out{};
  out.path = path;
  return out;
}

struct InflaterContext : DiagnosticCollector {
  // Pass 1 output — every instantiated tgfx::Layer keyed by its `layerPath`;
  // nullptr slots represent layers degraded by budget / cycle. Pass 2 reads
  // it to resolve `maskLayerPath`.
  std::unordered_map<PackedLayerPath, std::shared_ptr<tgfx::Layer>, PackedLayerPathHash>
      layerByPath = {};

  struct PendingMask {
    std::shared_ptr<tgfx::Layer> host;
    PackedLayerPath targetPath;
    LayerMaskType maskType = LayerMaskType::Alpha;
  };
  std::vector<PendingMask> pendingMasks = {};

  // Composition reference graph — one bool per compositions[] entry, flipped
  // true on enter / false on leave via CompositionVisitScope. Size is assigned
  // exactly once at Inflate entry.
  std::vector<bool> visitingComposition = {};
  uint32_t currentCompositionDepth = 0;

  // Global Layer budget. Decoder's per-Layer/per-Composition caps legalise
  // individual sub-structures but not cumulative totals — `10^6` layers
  // individually decodable can still OOM the GPU when all inflated.
  uint32_t totalInflatedLayers = 0;

  // Font resolver injected by the caller (Phase 16 runtime-shape mode).
  // Never null inside an active Inflate() call — LayerInflater::Inflate
  // substitutes MakeDefaultFontProvider() when the caller passes nullptr.
  std::shared_ptr<FontProvider> fontProvider;

  // Public warn wrapper (3-arg, same shape as DecodeContext / BakeContext).
  // Inflater has no byte stream, so byteOffset is always 0.
  void warn(ErrorCode code, std::string msg = {}, uint32_t contextIndex = UINT32_MAX) {
    pushWarning({code, std::move(msg), 0, contextIndex});
  }

  // Returns true if the budget allowed this instantiation; updates counter.
  // The caller uses the return value to decide whether to materialise a
  // tgfx::Layer or degrade to an empty placeholder / skipped slot.
  bool reserveLayerBudget(uint32_t layerIndex = UINT32_MAX) {
    if (totalInflatedLayers >= limits::MAX_INFLATED_LAYER_COUNT) {
      warn(ErrorCode::InflaterLayerBudgetExceeded, "MAX_INFLATED_LAYER_COUNT exceeded", layerIndex);
      return false;
    }
    ++totalInflatedLayers;
    return true;
  }

  // Separate pending-mask cap — a single Layer can queue N mask bindings, so
  // without this cap an attacker could bypass the Layer budget via N × host
  // layers with masks each.
  bool reservePendingMaskSlot() {
    if (pendingMasks.size() >= limits::MAX_PENDING_MASKS) {
      warn(ErrorCode::InflateMaskResolveFailed, "MAX_PENDING_MASKS exceeded; mask binding dropped");
      return false;
    }
    return true;
  }

  // Base class's virtual pushError stays a no-op — we deliberately do NOT
  // override, so any (incorrect) Inflater code path trying to push a fatal
  // is silently dropped. §9.4.
};

// RAII guard for the composition-reference graph. Increments the visitation
// counters on a valid enter; dtor rolls back only when `entered` was set, so
// an early-return (cycle / over-depth / out-of-range) is safe.
struct CompositionVisitScope {
  InflaterContext* ctx;
  uint32_t idx;
  bool entered = false;

  CompositionVisitScope(InflaterContext* c, uint32_t compositionIndex)
      : ctx(c), idx(compositionIndex) {
    if (idx >= ctx->visitingComposition.size()) {
      return;
    }
    if (ctx->visitingComposition[idx]) {
      ctx->warn(ErrorCode::InflateCompositionCycle, "composition self-reference or cycle", idx);
      return;
    }
    if (ctx->currentCompositionDepth >= limits::MAX_COMPOSITION_REF_DEPTH) {
      ctx->warn(ErrorCode::InflateCompositionCycle, "composition ref depth exceeds limit", idx);
      return;
    }
    ctx->visitingComposition[idx] = true;
    ++ctx->currentCompositionDepth;
    entered = true;
  }

  ~CompositionVisitScope() {
    if (!entered) {
      return;
    }
    ctx->visitingComposition[idx] = false;
    --ctx->currentCompositionDepth;
  }

  CompositionVisitScope(const CompositionVisitScope&) = delete;
  CompositionVisitScope& operator=(const CompositionVisitScope&) = delete;

  explicit operator bool() const {
    return entered;
  }
};

}  // namespace pagx::pag
