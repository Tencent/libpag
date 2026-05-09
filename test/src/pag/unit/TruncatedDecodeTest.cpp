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

bool HasWarning(const DecodeResult& r, pagx::DiagnosticCode code) {
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

// =============================================================================
// Top-level Tag loop edge cases — exercise the §197 TagCode switch's default
// / duplicate-FileHeader / malformed-length branches in Codec.cpp. These are
// defensive paths that the Encoder never produces naturally, so we inject
// synthetic Tag bytes before the End marker of a known-good stream.
// =============================================================================

namespace {

// TagHeader wire format (TagHeader.cpp:19-28): little-endian u16 where the
// high 10 bits are the TagCode and the low 6 bits are the length (or 63 +
// a trailing u32 length for long bodies). Small helpers for readable byte
// vectors in tests:
std::vector<uint8_t> InlineTagHeader(uint16_t code, uint32_t length) {
  EXPECT_LT(length, 63u);  // we only ever use the inline form here.
  uint16_t typeAndLength = static_cast<uint16_t>((code << 6) | (length & 0x3Fu));
  return {static_cast<uint8_t>(typeAndLength & 0xFF),
          static_cast<uint8_t>((typeAndLength >> 8) & 0xFF)};
}

std::vector<uint8_t> LongTagHeader(uint16_t code, uint32_t length) {
  uint16_t typeAndLength = static_cast<uint16_t>((code << 6) | 63u);
  return {
      static_cast<uint8_t>(typeAndLength & 0xFF),
      static_cast<uint8_t>((typeAndLength >> 8) & 0xFF),
      static_cast<uint8_t>(length & 0xFF),
      static_cast<uint8_t>((length >> 8) & 0xFF),
      static_cast<uint8_t>((length >> 16) & 0xFF),
      static_cast<uint8_t>((length >> 24) & 0xFF),
  };
}

// Rewrite the little-endian bodyLength field at offset 4 of a PAG stream.
void PatchBodyLength(std::vector<uint8_t>* bytes, uint32_t newLength) {
  (*bytes)[4] = static_cast<uint8_t>(newLength & 0xFF);
  (*bytes)[5] = static_cast<uint8_t>((newLength >> 8) & 0xFF);
  (*bytes)[6] = static_cast<uint8_t>((newLength >> 16) & 0xFF);
  (*bytes)[7] = static_cast<uint8_t>((newLength >> 24) & 0xFF);
}

}  // namespace

// Phase 15 §228-231 — a top-level Tag with an unknown TagCode is skipped
// with an UnknownTagCode warning (forward-compat). The rest of the body
// decodes cleanly, so dec.doc must be non-null.
TEST(Truncate, UnknownTopLevelTagCodeWarns) {
  auto bytes = EncodeMinimalDoc();
  // End tag sits at the final 2 bytes (WriteEndTag = 0x0000). Splice in an
  // unknown Tag with code=500 and length=0 right before it.
  ASSERT_GE(bytes.size(), 11u);
  const uint32_t originalBodyLength =
      static_cast<uint32_t>(bytes[4]) | (static_cast<uint32_t>(bytes[5]) << 8) |
      (static_cast<uint32_t>(bytes[6]) << 16) | (static_cast<uint32_t>(bytes[7]) << 24);

  std::vector<uint8_t> out(bytes.begin(), bytes.end() - 2);  // drop End tag
  auto unknown = InlineTagHeader(/*code=*/500u, /*length=*/0u);
  out.insert(out.end(), unknown.begin(), unknown.end());
  out.push_back(0x00);  // End tag low byte
  out.push_back(0x00);  // End tag high byte
  PatchBodyLength(&out, originalBodyLength + static_cast<uint32_t>(unknown.size()));

  auto dec = Codec::Decode(out.data(), out.size());
  EXPECT_NE(dec.doc, nullptr);
  EXPECT_TRUE(HasWarning(dec, pagx::DiagnosticCode::UnknownTagCode));
}

// Phase 15 §199-202 — a second FileHeader Tag is ignored with an
// UnknownTagCode warning (forward-compat).
TEST(Truncate, DuplicateFileHeaderWarns) {
  auto bytes = EncodeMinimalDoc();
  // Splice in another FileHeader (TagCode=1) with a tiny length. We don't
  // actually need valid FileHeader body bytes — the tag is skipped by the
  // sawFileHeader guard before ReadFileHeader runs, but the body bytes
  // must still fit within the declared tag length so the tagEnd seek
  // lands on the next Tag. Use length=0 to avoid any body.
  ASSERT_GE(bytes.size(), 11u);
  const uint32_t originalBodyLength =
      static_cast<uint32_t>(bytes[4]) | (static_cast<uint32_t>(bytes[5]) << 8) |
      (static_cast<uint32_t>(bytes[6]) << 16) | (static_cast<uint32_t>(bytes[7]) << 24);

  std::vector<uint8_t> out(bytes.begin(), bytes.end() - 2);  // drop End
  auto dup = InlineTagHeader(/*code=*/1u, /*length=*/0u);    // duplicate FileHeader
  out.insert(out.end(), dup.begin(), dup.end());
  out.push_back(0x00);
  out.push_back(0x00);
  PatchBodyLength(&out, originalBodyLength + static_cast<uint32_t>(dup.size()));

  auto dec = Codec::Decode(out.data(), out.size());
  EXPECT_NE(dec.doc, nullptr);
  EXPECT_TRUE(HasWarning(dec, pagx::DiagnosticCode::UnknownTagCode));
}

// Phase 15 §190-194 — a Tag whose length field claims more bytes than
// bodyLength declares. Triggers MalformedTag and aborts the decode (doc
// goes back to nullptr).
TEST(Truncate, TagLengthExceedsBodyEndIsMalformed) {
  auto bytes = EncodeMinimalDoc();
  const uint32_t originalBodyLength =
      static_cast<uint32_t>(bytes[4]) | (static_cast<uint32_t>(bytes[5]) << 8) |
      (static_cast<uint32_t>(bytes[6]) << 16) | (static_cast<uint32_t>(bytes[7]) << 24);

  // Splice in a long-form Tag whose length claims 1 MB of body. The
  // tagEnd = tagBodyStart + 1 MB will trivially exceed bodyEnd; Codec
  // emits MalformedTag and bails.
  std::vector<uint8_t> out(bytes.begin(), bytes.end() - 2);
  auto poison = LongTagHeader(/*code=*/500u, /*length=*/1u << 20);
  out.insert(out.end(), poison.begin(), poison.end());
  out.push_back(0x00);
  out.push_back(0x00);
  PatchBodyLength(&out, originalBodyLength + static_cast<uint32_t>(poison.size()));

  auto dec = Codec::Decode(out.data(), out.size());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::MalformedTag));
}

// Phase 15 §163-167 — bodyLength claims more bytes than the stream holds
// at the TagHeader boundary. Specifically: body ends with a single byte
// followed by EOF, so bytesAvailable<2 fires before ReadTagHeader runs.
// We reach this path by making bodyLength exactly 1 greater than the
// actual trailing byte count (trimmed End tag to a single byte).
TEST(Truncate, TruncatedBeforeTagHeader) {
  auto bytes = EncodeMinimalDoc();
  const uint32_t originalBodyLength =
      static_cast<uint32_t>(bytes[4]) | (static_cast<uint32_t>(bytes[5]) << 8) |
      (static_cast<uint32_t>(bytes[6]) << 16) | (static_cast<uint32_t>(bytes[7]) << 24);

  // Drop the End tag's high byte so only 1 byte remains at the end; leave
  // bodyLength advertising the original (2-byte End) count. Codec reaches
  // the loop iteration for the final Tag, sees bytesAvailable == 1 < 2,
  // and trips TruncatedData §164.
  ASSERT_GE(bytes.size(), 1u);
  std::vector<uint8_t> out(bytes.begin(), bytes.end() - 1);  // chop 1 byte
  // Shrink bodyLength by exactly 1 so Layer-5 guard still accepts it.
  PatchBodyLength(&out, originalBodyLength - 1);

  auto dec = Codec::Decode(out.data(), out.size());
  EXPECT_EQ(dec.doc, nullptr);
  EXPECT_TRUE(HasError(dec, pagx::DiagnosticCode::TruncatedData));
}
