// Copyright (C) 2026 Tencent. All rights reserved.
//
// Bake-side state for PAGX → PAGDocument conversion. Holds three orthogonal
// concerns:
//   1. Diagnostic collection (errors[] for Baker fatal 100-199 + the
//      inherited warnings[] for 200-299).
//   2. Hierarchy budgets — `MAX_LAYER_DEPTH` / `MAX_VECTOR_ELEMENT_DEPTH`
//      single-node depth caps PLUS Baker-only cumulative caps
//      (`BAKE_MAX_TOTAL_*`) that defend against attacker-controlled XML.
//   3. Resource interning maps populated by the pre-pass ResourceBaker —
//      VectorBaker / TextBaker reach into them when building element bodies.
//
// Authoritative spec: design doc §8.5 (DiagnosticCollector hierarchy) +
// §H.2 (depth caps) + §7.2 (resource pre-pass).
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "pagx/Diagnostic.h"
#include "pagx/pag/DiagnosticCollector.h"
#include "pagx/pag/ErrorCode.h"
#include "pagx/pag/limits.h"
#include "tgfx/core/Data.h"

namespace pagx::pag {

struct BakeContext : DiagnosticCollector {
  // ---- Diagnostics ----
  std::vector<Diagnostic> errors;  // Baker fatal segment 100-199

  // ---- Per-traversal counters (single-node depth caps) ----
  uint32_t currentLayerDepth = 0;          // §H.1 MAX_LAYER_DEPTH
  uint32_t currentVectorElementDepth = 0;  // §H.1 MAX_VECTOR_ELEMENT_DEPTH

  // ---- Whole-document cumulative counters (Baker-only caps) ----
  // PAGX XML is attacker-controlled; per-node depth checks miss "wide and
  // shallow" abuse where a flat 2^31 sibling list never exceeds the depth
  // limit but explodes memory. These three counters are the global circuit
  // breaker (§H.2).
  uint32_t totalLayerCount = 0;
  uint32_t totalVectorElementCount = 0;
  uint32_t totalCompositionCount = 0;

  // ---- Resource interning (§7.2 pre-pass artefacts) ----
  // Three deduplication layers (P1-7 v2.15 / P2-7 v2.16): pointer-equal node →
  // pointer-equal embedded Data → semantic key string. Splitting them avoids
  // a variant<Data*, string> hash key and stays O(1) per lookup.
  std::unordered_map<const void*, uint32_t> imageIndexByNode;           // PAGX node ptr
  std::unordered_map<const tgfx::Data*, uint32_t> imageIndexByDataPtr;  // embedded Data ptr
  std::unordered_map<std::string, uint32_t> imageIndexByKey;            // URI / abs path

  std::unordered_map<const void*, uint32_t> fontIndexByNode;
  std::unordered_map<const tgfx::Data*, uint32_t> fontIndexByDataPtr;  // Embedded font Data
  std::unordered_map<std::string, uint32_t> fontIndexByKey;            // "system\0family\0style"

  // ---- Mask two-pass index (§12.1) ----
  // Pass 1 records every PAGX layer's path (chain of child indices from the
  // enclosing root); Pass 2 looks up mask targets by pointer.
  std::unordered_map<const void*, std::vector<uint32_t>> layerPathByPagxLayer;

  // ---- Public 3-arg API ----

  void error(ErrorCode code, std::string msg = {}, uint32_t contextIndex = UINT32_MAX) {
    if (errors.size() >= limits::MAX_DIAGNOSTICS) {
      return;
    }
    if (msg.size() > limits::MAX_DIAGNOSTIC_MESSAGE_BYTES) {
      msg.resize(limits::MAX_DIAGNOSTIC_MESSAGE_BYTES);
    }
    errors.push_back({code, std::move(msg), 0, contextIndex});
  }

  void warn(ErrorCode code, std::string msg = {}, uint32_t contextIndex = UINT32_MAX) {
    pushWarning({code, std::move(msg), 0, contextIndex});
  }

  bool hasFatal() const {
    return !errors.empty();
  }

 protected:
  void pushError(Diagnostic d) override {
    if (errors.size() >= limits::MAX_DIAGNOSTICS) {
      return;
    }
    if (d.message.size() > limits::MAX_DIAGNOSTIC_MESSAGE_BYTES) {
      d.message.resize(limits::MAX_DIAGNOSTIC_MESSAGE_BYTES);
    }
    errors.push_back(std::move(d));
  }

 public:
  // ---- Hierarchy enter / exit (§H.2 enter/exit discipline) ----
  // Returns false when the budget is exhausted; the caller MUST then bail out
  // of the subtree without calling exit*(). exit*() saturates at zero so a
  // missing-pair bug cannot underflow uint32_t into UINT32_MAX (which would
  // wedge every subsequent enter*() and flood diagnostics).

  bool enterLayer() {
    if (currentLayerDepth >= limits::MAX_LAYER_DEPTH) {
      error(ErrorCode::CompositionCycleDepth, "MAX_LAYER_DEPTH exceeded");
      return false;
    }
    if (totalLayerCount >= limits::BAKE_MAX_TOTAL_LAYERS) {
      error(ErrorCode::StructureLimitExceeded, "BAKE_MAX_TOTAL_LAYERS exceeded");
      return false;
    }
    ++currentLayerDepth;
    ++totalLayerCount;
    return true;
  }

  void exitLayer() {
    if (currentLayerDepth > 0) {
      --currentLayerDepth;
    }
  }

  bool enterVectorGroup() {
    if (currentVectorElementDepth >= limits::MAX_VECTOR_ELEMENT_DEPTH) {
      error(ErrorCode::StructureLimitExceeded, "MAX_VECTOR_ELEMENT_DEPTH exceeded");
      return false;
    }
    if (totalVectorElementCount >= limits::BAKE_MAX_TOTAL_VECTOR_ELEMENTS) {
      error(ErrorCode::StructureLimitExceeded, "BAKE_MAX_TOTAL_VECTOR_ELEMENTS exceeded");
      return false;
    }
    ++currentVectorElementDepth;
    ++totalVectorElementCount;
    return true;
  }

  void exitVectorGroup() {
    if (currentVectorElementDepth > 0) {
      --currentVectorElementDepth;
    }
  }

  bool registerComposition() {
    if (totalCompositionCount >= limits::BAKE_MAX_TOTAL_COMPOSITIONS) {
      error(ErrorCode::StructureLimitExceeded, "MAX_COMPOSITIONS exceeded (Baker pre-pass)");
      return false;
    }
    ++totalCompositionCount;
    return true;
  }
};

}  // namespace pagx::pag
