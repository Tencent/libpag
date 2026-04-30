// Copyright (C) 2026 Tencent. All rights reserved.
//
// Shared base for the three pipeline contexts (BakeContext / DecodeContext /
// InflaterContext). Holds the warnings vector + MAX_DIAGNOSTICS cap +
// MAX_DIAGNOSTIC_MESSAGE_BYTES truncation logic. Subclasses publish 3-arg
// public wrappers `error(code, msg, contextIndex)` / `warn(...)` that bind to
// the protected pushError / pushWarning helpers below — callers always see a
// single signature, no C++ name-hiding pitfalls.
#pragma once

#include <utility>
#include <vector>
#include "pagx/Diagnostic.h"
#include "pagx/pag/limits.h"

namespace pagx::pag {

struct DiagnosticCollector {
  std::vector<Diagnostic> warnings;

  virtual ~DiagnosticCollector() = default;

 protected:
  // Subclasses call into these. Base class deliberately does NOT expose public
  // error/warn — see §8.5 for the rationale (avoid name hiding).
  void pushWarning(Diagnostic d) {
    if (warnings.size() >= limits::MAX_DIAGNOSTICS) {
      return;
    }
    if (d.message.size() > limits::MAX_DIAGNOSTIC_MESSAGE_BYTES) {
      d.message.resize(limits::MAX_DIAGNOSTIC_MESSAGE_BYTES);
    }
    warnings.push_back(std::move(d));
  }

  // Default no-op. Only DecodeContext / BakeContext override (they own
  // `errors`); InflaterContext intentionally does NOT override — calls to
  // pushError there silently disappear, physically enforcing "Inflater has no
  // fatal" semantics.
  virtual void pushError(Diagnostic /* d */) { /* no-op */
  }
};

}  // namespace pagx::pag
