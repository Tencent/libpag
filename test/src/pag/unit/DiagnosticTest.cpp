// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 0 deliverable: 10 assertions covering FormatDiagnostic / SeverityOf /
// StageOf / kAllDiagnosticCodes round-trip + ABI-appended forward-compat.
// Authoritative spec: docs/pagx_to_pag_v2_design.md §G.3bis.
#include <cstdint>
#include <string>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/StreamContext.h"
#include "gtest/gtest.h"
#include "pagx/Diagnostic.h"
#include "pagx/pag/DiagBuild.h"
#include "pagx/pag/ErrorCode.h"

namespace pagx {

// ---------- FormatDiagnostic ----------

TEST(FormatDiagnostic, OkCode) {
  Diagnostic d{DiagnosticCode::Ok, {}, 0, UINT32_MAX};
  EXPECT_EQ(FormatDiagnostic(d), "[Ok]");
}

TEST(FormatDiagnostic, NoMessageNoOffset) {
  Diagnostic d{DiagnosticCode::LayoutNotApplied, {}, 0, UINT32_MAX};
  EXPECT_EQ(FormatDiagnostic(d), "[LayoutNotApplied]");
}

TEST(FormatDiagnostic, WithMessage) {
  Diagnostic d{DiagnosticCode::LayoutNotApplied, "must call applyLayout", 0, UINT32_MAX};
  EXPECT_EQ(FormatDiagnostic(d), "[LayoutNotApplied] must call applyLayout");
}

TEST(FormatDiagnostic, WithOffset) {
  Diagnostic d{DiagnosticCode::TruncatedData, "unexpected EOF", 0x4a2c, UINT32_MAX};
  EXPECT_EQ(FormatDiagnostic(d), "[TruncatedData] unexpected EOF @0x4a2c");
}

TEST(FormatDiagnostic, UnknownCodeFallback) {
  // ABI-appended code: a future segment value that this binary does not yet
  // recognise. CodeToString must not crash; FormatDiagnostic must surface
  // the numeric value via the "Code(<num>)" forward-compat path.
  Diagnostic d{static_cast<DiagnosticCode>(9999), {}, 0, UINT32_MAX};
  EXPECT_EQ(FormatDiagnostic(d), "[Code(9999)]");
}

TEST(FormatDiagnostic, WithContextIndex) {
  // contextIndex set → emit " #ctx=N"
  Diagnostic with{DiagnosticCode::ImageSourceMissing, "missing", 0, 3};
  EXPECT_EQ(FormatDiagnostic(with), "[ImageSourceMissing] missing #ctx=3");

  // contextIndex == UINT32_MAX → omit the " #ctx=" segment
  Diagnostic without{DiagnosticCode::ImageSourceMissing, "missing", 0, UINT32_MAX};
  EXPECT_EQ(FormatDiagnostic(without), "[ImageSourceMissing] missing");
}

// ---------- SeverityOf / StageOf ----------

TEST(SeverityOf, FatalSegments) {
  EXPECT_EQ(SeverityOf(DiagnosticCode::LayoutNotApplied), DiagnosticSeverity::Fatal);
  EXPECT_EQ(SeverityOf(DiagnosticCode::TruncatedData), DiagnosticSeverity::Fatal);
}

TEST(SeverityOf, WarningSegments) {
  EXPECT_EQ(SeverityOf(DiagnosticCode::ImageSourceMissing), DiagnosticSeverity::Warning);
  EXPECT_EQ(SeverityOf(DiagnosticCode::InflateImageDecodeFailed), DiagnosticSeverity::Warning);
}

TEST(StageOf, AllThreeStages) {
  EXPECT_EQ(StageOf(DiagnosticCode::LayoutNotApplied), DiagnosticStage::Baker);
  EXPECT_EQ(StageOf(DiagnosticCode::InvalidMagic), DiagnosticStage::Codec);
  EXPECT_EQ(StageOf(DiagnosticCode::InflaterEmptyDocument), DiagnosticStage::Inflater);
}

// ---------- kAllDiagnosticCodes coverage + MakeDecodeDiag / MakeDiag round-trip ----------

TEST(CodeToString, AllEnumValues) {
  // Walk the canonical full-enum table. Every entry MUST format as
  // "[<EnumName>]" — never the "[Code(<num>)]" fallback. If a developer adds
  // a new DiagnosticCode but forgets to extend kAllDiagnosticCodes, the
  // size-bumping CR step (b) is missed and the entry is silently exempt; if
  // they forget to extend the CodeToString switch in Diagnostic.cpp, the
  // formatting falls back to "[Code(...)]" and EXPECT_NE below fires.
  for (DiagnosticCode c : pag::kAllDiagnosticCodes) {
    Diagnostic d{c, {}, 0, UINT32_MAX};
    std::string s = FormatDiagnostic(d);
    ASSERT_FALSE(s.empty()) << "code=" << static_cast<unsigned>(c);
    // Must NOT take the unknown-code fallback path.
    EXPECT_EQ(s.find("[Code("), std::string::npos)
        << "CodeToString missing case for " << static_cast<unsigned>(c) << ", got " << s;
    // Must look like "[Name]"
    ASSERT_GE(s.size(), 2u);
    EXPECT_EQ(s.front(), '[');
  }
}

TEST(MakeDiag, RoundTripBakerStyle) {
  // MakeDiag is the Baker / Inflater / Loader builder — byteOffset stays 0
  // and contextIndex defaults to UINT32_MAX.
  pag::Diagnostic d = pag::MakeDiag(pag::ErrorCode::ImageSourceMissing, "missing.png", 7);
  EXPECT_EQ(d.code, DiagnosticCode::ImageSourceMissing);
  EXPECT_EQ(d.message, "missing.png");
  EXPECT_EQ(d.byteOffset, 0u);
  EXPECT_EQ(d.contextIndex, 7u);

  pag::Diagnostic empty = pag::MakeDiag(pag::ErrorCode::LayoutNotApplied);
  EXPECT_EQ(empty.code, DiagnosticCode::LayoutNotApplied);
  EXPECT_TRUE(empty.message.empty());
  EXPECT_EQ(empty.byteOffset, 0u);
  EXPECT_EQ(empty.contextIndex, UINT32_MAX);
}

TEST(MakeDiag, RoundTripDecoderStyle) {
  // MakeDecodeDiag samples stream->position() into byteOffset. Build a v1
  // DecodeStream over a buffer and step its read pointer to verify the offset
  // is captured at call time. Buffer size > skip amount — DecodeStream::skip
  // refuses to advance past end-of-stream.
  ::pag::StreamContext sc;
  static constexpr uint8_t kBuf[256] = {0};
  ::pag::DecodeStream stream(&sc, kBuf, sizeof(kBuf));
  stream.skip(0x4a);  // advance the read pointer

  pag::Diagnostic d = pag::MakeDecodeDiag(pag::ErrorCode::TruncatedData, &stream, "EOF");
  EXPECT_EQ(d.code, DiagnosticCode::TruncatedData);
  EXPECT_EQ(d.message, "EOF");
  EXPECT_EQ(d.byteOffset, 0x4au);
  EXPECT_EQ(d.contextIndex, UINT32_MAX);
}

}  // namespace pagx
