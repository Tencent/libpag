// Copyright (C) 2026 Tencent. All rights reserved.
#include "CorruptBuilder.h"

namespace pagx::test {

CorruptBuilder CorruptBuilder::FromValid(std::vector<uint8_t> validBytes) {
  return CorruptBuilder(std::move(validBytes));
}

CorruptBuilder& CorruptBuilder::TruncateAt(size_t n) {
  if (n < bytes_.size()) {
    bytes_.resize(n);
  }
  return *this;
}

CorruptBuilder& CorruptBuilder::FlipByteAt(size_t offset, uint8_t xorMask) {
  if (offset < bytes_.size()) {
    bytes_[offset] = static_cast<uint8_t>(bytes_[offset] ^ xorMask);
  }
  return *this;
}

CorruptBuilder& CorruptBuilder::ReplaceBytes(size_t offset, const std::vector<uint8_t>& patch) {
  if (offset >= bytes_.size()) {
    return *this;
  }
  size_t writable = std::min(patch.size(), bytes_.size() - offset);
  for (size_t i = 0; i < writable; ++i) {
    bytes_[offset + i] = patch[i];
  }
  return *this;
}

}  // namespace pagx::test
