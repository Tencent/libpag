// Copyright (C) 2026 Tencent. All rights reserved.
//
// Builds malicious / truncated PAG v2 byte streams for negative-path tests.
// Phase 4 TruncatedDecodeTest, Phase 9 InflaterParity.CompositionSelfRef, and
// Phase 12 fuzz corpus all depend on this helper — Phase 1 lands the core
// API (valid-bytes ingestion + byte-level mutation primitives); later phases
// extend it with semantic builders (InflateVarU32At, SetNumBitsAt, ...) on
// demand without breaking Phase 1 callers.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace pagx::test {

class CorruptBuilder {
 public:
  // Construct from a known-valid PAG v2 byte stream produced by the real
  // Codec. Phase 4 will feed Codec::Encode(doc).bytes directly; Phase 1
  // unit tests can hand-craft a few-byte sample.
  static CorruptBuilder FromValid(std::vector<uint8_t> validBytes);

  // Keep only the first n bytes, discard the rest. Triggers TruncatedData=303
  // once the Decoder lands in Phase 4.
  CorruptBuilder& TruncateAt(size_t n);

  // XOR one byte at `offset` with `xorMask`. Triggers MalformedTag=304 /
  // generic bit-flip fuzz scenarios. Out-of-range offset is a no-op by
  // design so fuzz callers can be lazy about bounds.
  CorruptBuilder& FlipByteAt(size_t offset, uint8_t xorMask);

  // Replace bytes at `offset` with `patch`. Out-of-range writes are a no-op.
  CorruptBuilder& ReplaceBytes(size_t offset, const std::vector<uint8_t>& patch);

  // Convenience: rewrite the version byte (byte 3 of a PAG file header).
  CorruptBuilder& SetVersion(uint8_t v) {
    return ReplaceBytes(3, {v});
  }

  // Convenience: rewrite the 3-byte magic prefix.
  CorruptBuilder& SetMagic(const std::array<uint8_t, 3>& m) {
    return ReplaceBytes(0, {m[0], m[1], m[2]});
  }

  // Hand off the mutated buffer.
  std::vector<uint8_t> Build() const {
    return bytes_;
  }

 private:
  explicit CorruptBuilder(std::vector<uint8_t> bytes) : bytes_(std::move(bytes)) {
  }

  std::vector<uint8_t> bytes_;
};

}  // namespace pagx::test
