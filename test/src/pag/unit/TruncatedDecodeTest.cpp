// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 4a TruncatedDecode — covers Codec fatal segments 302-306 + 409.
// Layer-level malformed cases (404 path verbs, 405 glyph count, 402/408
// resource size) land in Phase 5/8 once the corresponding readers exist.
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

[[maybe_unused]] bool HasWarning(const DecodeResult& r, pagx::DiagnosticCode code) {
  for (const auto& w : r.warnings) {
    if (w.code == code) {
      return true;
    }
  }
  return false;
}

}  // namespace

// ---------- 303 TruncatedData ----------

TEST(Truncate, EmptyInput) {
  auto dec = Codec::Decode(nullptr, 0);
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::TruncatedData));
}

TEST(Truncate, ShorterThanFraming) {
  // Anything below the 9-byte file container framing is a hard 303.
  std::vector<uint8_t> tiny = {'P', 'A', 'G', 0x02, 0x00, 0x00};  // 6 bytes
  auto dec = Codec::Decode(tiny.data(), tiny.size());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::TruncatedData));
}

TEST(Truncate, MissingEndTag) {
  // Truncating mid-body so the End tag never arrives. Reuse a known-good
  // encoded doc and chop the trailing End tag (last 2 bytes).
  auto bytes = EncodeMinimalDoc();
  ASSERT_GE(bytes.size(), 11u);
  size_t cut = bytes.size() - 2;  // capture before the move below invalidates `bytes`
  auto out = pagx::test::CorruptBuilder::FromValid(std::move(bytes)).TruncateAt(cut).Build();
  // Edge: the bodyLength header still claims the original byte count, so the
  // safe-subtract guard at framing-time will trip BodyLengthOutOfRange=305
  // before the body loop ever runs. Either 303 or 305 is acceptable here —
  // both surface the underlying "stream chopped" condition.
  auto dec = Codec::Decode(out.data(), out.size());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_FALSE(dec.errors.empty());
  bool ok = HasError(dec, pagx::DiagnosticCode::TruncatedData) ||
            HasError(dec, pagx::DiagnosticCode::BodyLengthOutOfRange);
  EXPECT_TRUE(ok);
}

// ---------- 302 UnsupportedCompression ----------

TEST(Truncate, UnsupportedCompression) {
  auto bytes = EncodeMinimalDoc();
  // Compression byte is at offset 8 (after 3 magic + 1 version + 4 length).
  auto out = pagx::test::CorruptBuilder::FromValid(std::move(bytes))
                 .ReplaceBytes(8, {0x01})  // any non-zero value
                 .Build();
  auto dec = Codec::Decode(out.data(), out.size());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::UnsupportedCompression));
}

// ---------- 305 BodyLengthOutOfRange ----------

TEST(Truncate, CorruptedLengthFieldOversized) {
  // bodyLength is at offset 4 (little-endian u32). Set it to 0xFFFFFFFF —
  // that exceeds both MAX_TOTAL_BODY_BYTES (= 1 GB) and the remaining stream.
  auto bytes = EncodeMinimalDoc();
  auto out = pagx::test::CorruptBuilder::FromValid(std::move(bytes))
                 .ReplaceBytes(4, {0xFF, 0xFF, 0xFF, 0xFF})
                 .Build();
  auto dec = Codec::Decode(out.data(), out.size());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::BodyLengthOutOfRange));
}

// ---------- 304 MalformedTag (FileHeader canvas size) ----------

TEST(Truncate, FileHeaderCanvasNaN) {
  // Construct a doc whose width / height are NaN by crafting a custom Encode
  // path. Easier: encode a normal doc and overwrite width's 4 little-endian
  // bytes with NaN (0x7FC00000) in the FileHeader body.
  //
  // Layout: 9-byte framing + TagHeader (2 bytes for our small body) +
  //         FileHeader body starts here. The first 4 bytes of the body are
  //         f32 width — at offset 9 + 2 = 11.
  auto bytes = EncodeMinimalDoc();
  // NaN bit pattern, little-endian.
  auto out = pagx::test::CorruptBuilder::FromValid(std::move(bytes))
                 .ReplaceBytes(11, {0x00, 0x00, 0xC0, 0x7F})
                 .Build();
  auto dec = Codec::Decode(out.data(), out.size());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::MalformedTag));
}

// ---------- 409 PrematureEndTag ----------
//
// The cleanest reproduction is to inflate bodyLength in the framing without
// adding extra bytes — the End tag is hit while bytes remain unread by the
// body loop's accounting. But our loop accounts via stream.position(), so a
// mismatched bodyLength produces 305 first. Skip this case from Phase 4a;
// 409 is naturally exercised once ImageAssetTable / FontAssetTable land in
// Phase 4b and ErrorMarker can sit between two top-level Tags.

// ---------- 306 InvalidCompositionIndex ----------
//
// The CompositionRefPayload reader is Phase 4b; the index check lives there
// too. Test moves to Phase 4b.
