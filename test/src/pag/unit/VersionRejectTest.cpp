// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 4a: covers ErrorCode 300 (InvalidMagic) and 301 (UnsupportedVersion).
// We use CorruptBuilder over a known-good Encode output to surgically flip
// the magic / version byte rather than hand-crafting raw byte arrays — this
// keeps the tests resilient to file-container layout tweaks.
#include <vector>
#include "gtest/gtest.h"
#include "pag/support/CorruptBuilder.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/PAGDocument.h"

using namespace pagx::pag;

namespace {

std::vector<uint8_t> EncodeMinimalDoc() {
  PAGDocument doc;
  doc.header.width = 100.0f;
  doc.header.height = 100.0f;
  auto enc = Codec::Encode(doc);
  EXPECT_NE(enc.bytes, nullptr);
  std::vector<uint8_t> buf(enc.bytes->length());
  std::memcpy(buf.data(), enc.bytes->data(), enc.bytes->length());
  return buf;
}

bool HasError(const DecodeResult& r, pagx::DiagnosticCode code) {
  for (const auto& e : r.errors) {
    if (e.code == code) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST(VersionReject, BadMagic_PAX) {
  auto bytes = EncodeMinimalDoc();
  // Magic occupies bytes 0..2 ('P','A','G'). Replace byte 2 'G' -> 'X'.
  auto out = pagx::test::CorruptBuilder::FromValid(std::move(bytes)).ReplaceBytes(2, {'X'}).Build();
  auto dec = Codec::Decode(out.data(), out.size());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::InvalidMagic));
}

TEST(VersionReject, V1Rejected) {
  auto bytes = EncodeMinimalDoc();
  auto out = pagx::test::CorruptBuilder::FromValid(std::move(bytes)).SetVersion(0x01).Build();
  auto dec = Codec::Decode(out.data(), out.size());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::UnsupportedVersion));
}

TEST(VersionReject, V3Rejected_FutureVersion) {
  // Forward-compat test: ensures the upgrade path stays closed for v0.
  auto bytes = EncodeMinimalDoc();
  auto out = pagx::test::CorruptBuilder::FromValid(std::move(bytes)).SetVersion(0x03).Build();
  auto dec = Codec::Decode(out.data(), out.size());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::UnsupportedVersion));
}

TEST(VersionReject, V255Rejected) {
  auto bytes = EncodeMinimalDoc();
  auto out = pagx::test::CorruptBuilder::FromValid(std::move(bytes)).SetVersion(0xFF).Build();
  auto dec = Codec::Decode(out.data(), out.size());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::UnsupportedVersion));
}
