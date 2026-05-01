// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 8 GlyphRunBlob byte codec — exercises the two kinds (LayoutRun /
// ClassicGlyphRun) through the write→read cycle. Each test constructs a
// blob in-memory, encodes via WriteGlyphRunBlobInline, decodes via
// ReadGlyphRunBlobInline, and asserts the post-quantisation values stay
// within the precision promised by §D.11.
#include <cstring>
#include <vector>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "codec/utils/StreamContext.h"
#include "gtest/gtest.h"
#include "pagx/pag/DecodeContext.h"
#include "pagx/pag/GlyphRunCodec.h"
#include "pagx/pag/PAGDocument.h"

namespace pagx::pag {

namespace {

// Encodes a blob and decodes the bytes back. Returns the round-tripped
// blob plus the decoder's diagnostic lists so tests can assert on them.
struct RoundTripOutcome {
  GlyphRunBlob blob;
  std::vector<Diagnostic> errors;
  std::vector<Diagnostic> warnings;
  uint32_t encodedBytes = 0;
};

RoundTripOutcome RoundTrip(const GlyphRunBlob& in) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream enc(&sc);
  WriteGlyphRunBlobInline(&enc, in);
  auto bytes = enc.release();

  RoundTripOutcome r;
  r.encodedBytes = bytes != nullptr ? bytes->length() : 0;

  DecodeContext ctx;
  ::pag::DecodeStream dec(&ctx.streamContext, bytes->data(), bytes->length());
  ctx.currentStream = &dec;
  bool ok = ReadGlyphRunBlobInline(&dec, &ctx, bytes->length(), &r.blob);
  r.errors = std::move(ctx.errors);
  r.warnings = std::move(ctx.warnings);
  if (!ok && r.errors.empty()) {
    // Keep the invariant: nullptr-equivalent return → non-empty errors.
    r.errors.push_back(
        {ErrorCode::MalformedTag, "ReadGlyphRunBlobInline returned false", 0, UINT32_MAX});
  }
  return r;
}

// Quantisation precision from §D.11 (kDefaultPrecisionLog2 = 4 ↔ 0.0625 px).
constexpr float kPositionPrecision = 1.0f / 16.0f;
constexpr float kScalePrecision = 0.005f;
constexpr float kAnglePrecision = 0.1f;

}  // namespace

// -----------------------------------------------------------------------
// LayoutRun
// -----------------------------------------------------------------------

TEST(GlyphRunBlob, LayoutRunRoundTrip) {
  // 10 glyphs; half carry an RSXform with an identity-breaking rotation.
  GlyphRunBlob in;
  in.kind = GlyphRunKind::LayoutRun;
  in.fontIndex = 3;
  in.fontSize = 16.0f;
  in.inverseMatrix = tgfx::Matrix::I();
  in.layoutGlyphs.resize(10);
  for (size_t i = 0; i < 10; ++i) {
    auto& g = in.layoutGlyphs[i];
    g.glyphId = static_cast<uint16_t>(100 + i);
    g.position = tgfx::Point{static_cast<float>(i * 12), 4.0f};
    if (i % 2 == 0) {
      g.hasXform = true;
      g.scos = 0.707f;
      g.ssin = 0.707f;
      g.tx = static_cast<float>(i);
      g.ty = 0.0f;
    }
  }

  auto r = RoundTrip(in);
  EXPECT_TRUE(r.errors.empty());
  ASSERT_EQ(r.blob.kind, GlyphRunKind::LayoutRun);
  EXPECT_EQ(r.blob.fontIndex, 3u);
  EXPECT_FLOAT_EQ(r.blob.fontSize, 16.0f);
  ASSERT_EQ(r.blob.layoutGlyphs.size(), 10u);
  for (size_t i = 0; i < 10; ++i) {
    const auto& g = r.blob.layoutGlyphs[i];
    EXPECT_EQ(g.glyphId, static_cast<uint16_t>(100 + i));
    EXPECT_NEAR(g.position.x, static_cast<float>(i * 12), kPositionPrecision);
    EXPECT_NEAR(g.position.y, 4.0f, kPositionPrecision);
    EXPECT_EQ(g.hasXform, (i % 2 == 0));
    if (g.hasXform) {
      EXPECT_FLOAT_EQ(g.scos, 0.707f);
      EXPECT_FLOAT_EQ(g.ssin, 0.707f);
      EXPECT_FLOAT_EQ(g.tx, static_cast<float>(i));
      EXPECT_FLOAT_EQ(g.ty, 0.0f);
    }
  }
}

TEST(GlyphRunBlob, LayoutRunNoXformsCompressesBits) {
  // A run whose xforms vector is empty must serialise hasXformBits as all
  // zeros — no xform payload follows. We re-use the RoundTrip helper and
  // double-check no diagnostics surface.
  GlyphRunBlob in;
  in.kind = GlyphRunKind::LayoutRun;
  in.fontIndex = 0;
  in.fontSize = 12.0f;
  in.layoutGlyphs.resize(20);
  for (size_t i = 0; i < 20; ++i) {
    in.layoutGlyphs[i].glyphId = static_cast<uint16_t>(i);
    in.layoutGlyphs[i].position = tgfx::Point{static_cast<float>(i), 0.0f};
    // hasXform stays false by default
  }

  auto r = RoundTrip(in);
  EXPECT_TRUE(r.errors.empty());
  ASSERT_EQ(r.blob.layoutGlyphs.size(), 20u);
  for (const auto& g : r.blob.layoutGlyphs) {
    EXPECT_FALSE(g.hasXform);
  }
}

// -----------------------------------------------------------------------
// ClassicGlyphRun
// -----------------------------------------------------------------------

TEST(GlyphRunBlob, ClassicGlyphRunRoundTrip) {
  // 50 glyphs, all-default anchors / scale / rotation / skew. The
  // quantised lists should collapse aggressively (dynamic bit width chooses
  // ~1 bit per all-zero value); we don't inspect bytes directly here but
  // the roundtrip asserts parity.
  GlyphRunBlob in;
  in.kind = GlyphRunKind::ClassicGlyphRun;
  in.fontIndex = 7;
  in.fontSize = 20.0f;
  in.baseX = 100.0f;
  in.baseY = 50.0f;
  in.classicGlyphs.resize(50);
  for (size_t i = 0; i < 50; ++i) {
    auto& g = in.classicGlyphs[i];
    g.glyphId = static_cast<uint16_t>(200 + i);
    g.xOffset = static_cast<float>(i) * 8.0f;
    g.position = tgfx::Point{static_cast<float>(i) * 10.0f, 0.0f};
    // anchor, scale, rotation, skew stay at defaults
  }

  auto r = RoundTrip(in);
  EXPECT_TRUE(r.errors.empty());
  ASSERT_EQ(r.blob.kind, GlyphRunKind::ClassicGlyphRun);
  EXPECT_EQ(r.blob.fontIndex, 7u);
  EXPECT_FLOAT_EQ(r.blob.baseX, 100.0f);
  EXPECT_FLOAT_EQ(r.blob.baseY, 50.0f);
  ASSERT_EQ(r.blob.classicGlyphs.size(), 50u);
  for (size_t i = 0; i < 50; ++i) {
    const auto& g = r.blob.classicGlyphs[i];
    EXPECT_EQ(g.glyphId, static_cast<uint16_t>(200 + i));
    EXPECT_NEAR(g.xOffset, static_cast<float>(i) * 8.0f, kPositionPrecision);
    EXPECT_NEAR(g.position.x, static_cast<float>(i) * 10.0f, kPositionPrecision);
    EXPECT_NEAR(g.anchor.x, 0.0f, kPositionPrecision);
    EXPECT_NEAR(g.scale.x, 1.0f, kScalePrecision);
    EXPECT_NEAR(g.rotation, 0.0f, kAnglePrecision);
    EXPECT_NEAR(g.skew, 0.0f, kAnglePrecision);
  }
}

TEST(GlyphRunBlob, ClassicGlyphRunNonDefaultAttributes) {
  // Tiny run with non-default anchor / scale / rotation / skew so the
  // quantised lists exercise their full precision paths rather than the
  // all-zero fast path.
  GlyphRunBlob in;
  in.kind = GlyphRunKind::ClassicGlyphRun;
  in.fontIndex = 1;
  in.fontSize = 14.0f;
  in.baseX = 0.0f;
  in.baseY = 0.0f;
  in.classicGlyphs.resize(3);
  in.classicGlyphs[0] = {static_cast<uint16_t>(65),
                         1.5f,
                         tgfx::Point{10, 0},
                         tgfx::Point{2, 3},
                         tgfx::Point{1.25f, 0.75f},
                         30.0f,
                         5.0f};
  in.classicGlyphs[1] = {static_cast<uint16_t>(66),
                         -0.25f,
                         tgfx::Point{20, 0},
                         tgfx::Point{-1, 1},
                         tgfx::Point{0.5f, 0.5f},
                         -45.5f,
                         -2.5f};
  in.classicGlyphs[2] = {static_cast<uint16_t>(67),
                         0.0f,
                         tgfx::Point{30, 0},
                         tgfx::Point{0, 0},
                         tgfx::Point{1.0f, 1.0f},
                         0.0f,
                         0.0f};

  auto r = RoundTrip(in);
  EXPECT_TRUE(r.errors.empty());
  ASSERT_EQ(r.blob.classicGlyphs.size(), 3u);
  EXPECT_EQ(r.blob.classicGlyphs[0].glyphId, 65u);
  EXPECT_NEAR(r.blob.classicGlyphs[0].anchor.x, 2.0f, kPositionPrecision);
  EXPECT_NEAR(r.blob.classicGlyphs[0].scale.x, 1.25f, kScalePrecision);
  EXPECT_NEAR(r.blob.classicGlyphs[0].rotation, 30.0f, kAnglePrecision);
  EXPECT_NEAR(r.blob.classicGlyphs[0].skew, 5.0f, kAnglePrecision);
  EXPECT_NEAR(r.blob.classicGlyphs[1].anchor.x, -1.0f, kPositionPrecision);
  EXPECT_NEAR(r.blob.classicGlyphs[1].rotation, -45.5f, kAnglePrecision);
}

// -----------------------------------------------------------------------
// Edge cases
// -----------------------------------------------------------------------

TEST(GlyphRunBlob, EmptyGlyphCountLayoutRun) {
  // glyphCount=0 must not crash — each quantised list writes a numBits=0
  // header and nothing more. The decoder re-reads the header and loops
  // zero times.
  GlyphRunBlob in;
  in.kind = GlyphRunKind::LayoutRun;
  in.fontIndex = UINT32_MAX;
  in.fontSize = 12.0f;

  auto r = RoundTrip(in);
  EXPECT_TRUE(r.errors.empty());
  EXPECT_EQ(r.blob.kind, GlyphRunKind::LayoutRun);
  EXPECT_EQ(r.blob.fontIndex, UINT32_MAX);
  EXPECT_TRUE(r.blob.layoutGlyphs.empty());
}

TEST(GlyphRunBlob, EmptyGlyphCountClassicRun) {
  GlyphRunBlob in;
  in.kind = GlyphRunKind::ClassicGlyphRun;
  in.fontIndex = 0;
  in.fontSize = 10.0f;
  in.baseX = 5.0f;
  in.baseY = 10.0f;

  auto r = RoundTrip(in);
  EXPECT_TRUE(r.errors.empty());
  EXPECT_EQ(r.blob.kind, GlyphRunKind::ClassicGlyphRun);
  EXPECT_TRUE(r.blob.classicGlyphs.empty());
  EXPECT_FLOAT_EQ(r.blob.baseX, 5.0f);
  EXPECT_FLOAT_EQ(r.blob.baseY, 10.0f);
}

TEST(GlyphRunBlob, InverseMatrixPreservedWhenNonIdentity) {
  // Non-identity matrix exercises the full f32×6 branch of WriteMatrix
  // rather than the 1-byte identity fast path.
  GlyphRunBlob in;
  in.kind = GlyphRunKind::ClassicGlyphRun;
  in.fontIndex = 0;
  in.fontSize = 12.0f;
  in.inverseMatrix = tgfx::Matrix::MakeTrans(3, 4);
  in.inverseMatrix.postScale(2.0f, 2.0f);
  in.classicGlyphs.resize(1);
  in.classicGlyphs[0].glyphId = 42;

  auto r = RoundTrip(in);
  EXPECT_TRUE(r.errors.empty());
  EXPECT_FLOAT_EQ(r.blob.inverseMatrix.getTranslateX(), in.inverseMatrix.getTranslateX());
  EXPECT_FLOAT_EQ(r.blob.inverseMatrix.getScaleX(), in.inverseMatrix.getScaleX());
}

}  // namespace pagx::pag
