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
#include "pagx/pag/limits.h"

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

// ---------- 409 PrematureEndTag (Phase 4b can naturally test) ----------
// Skipped here — depends on ErrorMarker injection (Phase 4b but landed via
// CorruptBuilder::AppendErrorMarkerTag which Phase 1 left as TODO).

// =============================================================================
// Phase 4b: 306 InvalidCompositionIndex / 105 table overflows / 406 LayerDepth
// / 403 InvalidUtf8
// =============================================================================

TEST(Truncate, InvalidCompositionIndex) {
  // Build a doc with one composition that contains a CompositionRef pointing
  // out of bounds (writer-side bug or attack). Encoding allows it; decode
  // must fatally reject with 306.
  pagx::pag::PAGDocument doc;
  doc.header.width = 100.0f;
  doc.header.height = 100.0f;
  auto comp = std::make_unique<pagx::pag::Composition>();
  comp->id = "x";
  comp->width = 10;
  comp->height = 10;
  auto refLayer = std::make_unique<pagx::pag::Layer>();
  refLayer->type = pagx::pag::LayerType::CompositionRef;
  refLayer->compositionIndex = 99;  // way past the 1-element compositions vec
  comp->layers.push_back(std::move(refLayer));
  doc.compositions.push_back(std::move(comp));

  auto enc = Codec::Encode(doc);
  ASSERT_NE(enc.bytes, nullptr);
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::InvalidCompositionIndex));
}

TEST(Truncate, InvalidUtf8InCompositionId) {
  // Build a doc with a composition whose id contains a malformed UTF-8
  // sequence (lone continuation byte 0x80). Encode -> Decode must surface
  // StringInvalidUtf8=403.
  pagx::pag::PAGDocument doc;
  doc.header.width = 100.0f;
  doc.header.height = 100.0f;
  auto comp = std::make_unique<pagx::pag::Composition>();
  comp->id = std::string("\x80\x80\x80", 3);  // invalid UTF-8 (orphan continuations)
  comp->width = 10;
  comp->height = 10;
  doc.compositions.push_back(std::move(comp));

  auto enc = Codec::Encode(doc);
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  // ReadUtf8String warns rather than fatally erroring (the empty string is
  // the degraded result), so the doc decodes but with a warning.
  ASSERT_NE(dec.doc, nullptr);
  bool foundUtf8Warn = false;
  for (const auto& w : dec.warnings) {
    if (w.code == pagx::DiagnosticCode::StringInvalidUtf8) {
      foundUtf8Warn = true;
      break;
    }
  }
  EXPECT_TRUE(foundUtf8Warn);
  // The id should have been replaced with empty per the safe-string contract.
  EXPECT_EQ(dec.doc->compositions[0]->id, "");
}

TEST(Truncate, LayerDepthLimitExceeded) {
  // Build a deeply-nested layer tree whose depth exceeds MAX_LAYER_DEPTH=64.
  // Encoder doesn't enforce depth (Baker is normally responsible) — the
  // Decoder must reject with 406 to defend against malicious / corrupt
  // streams that bypass the Baker.
  pagx::pag::PAGDocument doc;
  doc.header.width = 100.0f;
  doc.header.height = 100.0f;
  auto comp = std::make_unique<pagx::pag::Composition>();
  comp->id = "deep";
  comp->width = 10;
  comp->height = 10;

  auto root = std::make_unique<pagx::pag::Layer>();
  root->type = pagx::pag::LayerType::Layer;
  pagx::pag::Layer* cursor = root.get();
  for (uint32_t i = 0; i < pagx::pag::limits::MAX_LAYER_DEPTH + 5; ++i) {
    auto child = std::make_unique<pagx::pag::Layer>();
    child->type = pagx::pag::LayerType::Layer;
    pagx::pag::Layer* next = child.get();
    cursor->children.push_back(std::move(child));
    cursor = next;
  }
  comp->layers.push_back(std::move(root));
  doc.compositions.push_back(std::move(comp));

  auto enc = Codec::Encode(doc);
  ASSERT_NE(enc.bytes, nullptr);
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::LayerDepthLimitExceeded));
}
