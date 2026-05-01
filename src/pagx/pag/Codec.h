// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAG v2 Codec entry — Encode(PAGDocument&) -> bytes, Decode(bytes) ->
// PAGDocument. Internal byte-stream I/O only; PAGExporter/PAGLoader sit
// above and translate between EncodeResult/DecodeResult and the public
// pagx::Result type.
//
// Authoritative design: §8.1 (interface contract) + §8.2 (Encode flow) +
// §8.3 (Decode flow) + §D.4 (file container).
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include "pag/types.h"  // pag::ByteData
#include "pagx/Diagnostic.h"
#include "pagx/pag/PAGDocument.h"

namespace pagx::pag {

struct EncodeResult {
  // Non-null on success. Encode never fails today (worst case warnings),
  // but kept as nullable for forward-compat with future hard-fail paths.
  std::unique_ptr<::pag::ByteData> bytes;
  std::vector<Diagnostic> warnings;
};

struct DecodeResult {
  // Non-null iff errors.empty(). Caller checks `doc != nullptr` to gate
  // success; warnings can accompany either path.
  std::unique_ptr<PAGDocument> doc;
  std::vector<Diagnostic> errors;
  std::vector<Diagnostic> warnings;
};

class Codec {
 public:
  // Encode contract:
  //   - Input doc must outlive the call (we hold a reference).
  //   - Returns bytes wrapped in pag::ByteData (owns the buffer).
  //   - No exceptions; never accepts nullptr (caller's responsibility).
  //   - Thread-safe: no static state; concurrent calls on different docs OK.
  static EncodeResult Encode(const PAGDocument& doc);

  // Decode contract:
  //   - data may be nullptr only if length == 0 (early reject).
  //   - On any fatal error returns {doc=nullptr, errors=[...]}.
  //   - Caller treats doc==nullptr as the unambiguous fail signal.
  static DecodeResult Decode(const uint8_t* data, size_t length);
};

}  // namespace pagx::pag
