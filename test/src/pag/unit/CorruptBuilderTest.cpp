// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 1 unit tests for test/src/pag/support/CorruptBuilder.
#include <array>
#include <cstdint>
#include <vector>
#include "gtest/gtest.h"
#include "pag/support/CorruptBuilder.h"

namespace pagx::test {

TEST(CorruptBuilder, TruncateAt) {
  std::vector<uint8_t> valid{0x01, 0x02, 0x03, 0x04, 0x05};
  auto out = CorruptBuilder::FromValid(valid).TruncateAt(3).Build();
  ASSERT_EQ(out.size(), 3u);
  EXPECT_EQ(out[0], 0x01u);
  EXPECT_EQ(out[2], 0x03u);
}

TEST(CorruptBuilder, TruncateAtBeyondSizeIsNoop) {
  std::vector<uint8_t> valid{0x01, 0x02, 0x03};
  auto out = CorruptBuilder::FromValid(valid).TruncateAt(99).Build();
  EXPECT_EQ(out, valid);
}

TEST(CorruptBuilder, FlipByteAt) {
  std::vector<uint8_t> valid{0x00, 0xFF, 0xAA};
  auto out = CorruptBuilder::FromValid(valid).FlipByteAt(1, 0x01).Build();
  EXPECT_EQ(out[0], 0x00u);
  EXPECT_EQ(out[1], 0xFEu);  // 0xFF ^ 0x01
  EXPECT_EQ(out[2], 0xAAu);
}

TEST(CorruptBuilder, FlipByteAtOutOfRangeIsNoop) {
  std::vector<uint8_t> valid{0x00, 0x01, 0x02};
  auto out = CorruptBuilder::FromValid(valid).FlipByteAt(100, 0xFF).Build();
  EXPECT_EQ(out, valid);
}

TEST(CorruptBuilder, ReplaceBytes) {
  std::vector<uint8_t> valid{0xAA, 0xBB, 0xCC, 0xDD};
  auto out = CorruptBuilder::FromValid(valid).ReplaceBytes(1, {0x11, 0x22}).Build();
  EXPECT_EQ(out[0], 0xAAu);
  EXPECT_EQ(out[1], 0x11u);
  EXPECT_EQ(out[2], 0x22u);
  EXPECT_EQ(out[3], 0xDDu);
}

TEST(CorruptBuilder, ReplaceBytesClampsAtEnd) {
  // Patch would spill past the buffer — silently clamp.
  std::vector<uint8_t> valid{0xAA, 0xBB};
  auto out = CorruptBuilder::FromValid(valid).ReplaceBytes(1, {0x11, 0x22, 0x33}).Build();
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0], 0xAAu);
  EXPECT_EQ(out[1], 0x11u);
}

TEST(CorruptBuilder, SetVersionAndMagic) {
  // Simulated PAG v2 file header first 4 bytes: 'P','A','G', version
  std::vector<uint8_t> valid{'P', 'A', 'G', 0x02, 0x00, 0x00};
  auto out = CorruptBuilder::FromValid(valid).SetMagic({'X', 'Y', 'Z'}).SetVersion(0x99).Build();
  EXPECT_EQ(out[0], 'X');
  EXPECT_EQ(out[1], 'Y');
  EXPECT_EQ(out[2], 'Z');
  EXPECT_EQ(out[3], 0x99u);
}

TEST(CorruptBuilder, FluentChaining) {
  std::vector<uint8_t> valid(16, 0xAAu);
  auto out = CorruptBuilder::FromValid(valid)
                 .FlipByteAt(0, 0xFFu)
                 .ReplaceBytes(4, {0x01, 0x02})
                 .TruncateAt(8)
                 .Build();
  EXPECT_EQ(out.size(), 8u);
  EXPECT_EQ(out[0], 0x55u);  // 0xAA ^ 0xFF
  EXPECT_EQ(out[4], 0x01u);
  EXPECT_EQ(out[5], 0x02u);
}

}  // namespace pagx::test
