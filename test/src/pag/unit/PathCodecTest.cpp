// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 5b — direct WritePath / ReadPath round-trip + the §G.6 Path-related
// fatal cases (404 PathVerbLimitExceeded, 304 MalformedTag for invalid
// coords / weights / format).
//
// Round-trips end-to-end through Codec::Encode/Decode are also exercised via
// the ShapePath element in ElementTagsTest.PathRoundTripViaShapePath.
#include <vector>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "codec/utils/StreamContext.h"
#include "gtest/gtest.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/DecodeContext.h"
#include "pagx/pag/PAGDocument.h"
#include "pagx/pag/PathCodec.h"
#include "pagx/pag/limits.h"

using namespace pagx::pag;

namespace {

// Encodes a Path on a fresh EncodeStream, then decodes it via a fresh
// DecodeContext. Returns the decoded path; ctx errors / warnings are
// surfaced via the out parameters.
tgfx::Path PathRoundTrip(const tgfx::Path& in, std::vector<Diagnostic>* outErrors) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream enc(&sc);
  WritePath(&enc, in);
  auto bytes = enc.release();

  DecodeContext ctx;
  ::pag::DecodeStream dec(&ctx.streamContext, bytes->data(), bytes->length());
  ctx.currentStream = &dec;
  tgfx::Path out = ReadPath(&dec, &ctx);
  if (outErrors != nullptr) {
    *outErrors = std::move(ctx.errors);
  }
  return out;
}

// Counts verbs by walking the iterator (Path::countVerbs is the auth source
// but iterating is cheap and lets us also dump verb sequence in failure
// messages).
size_t CountVerbs(const tgfx::Path& p) {
  size_t n = 0;
  for (auto& seg : p) {
    if (seg.verb == ::tgfx::PathVerb::Done) {
      break;
    }
    ++n;
  }
  return n;
}

}  // namespace

TEST(PathCodec, EmptyPath) {
  tgfx::Path in;
  std::vector<Diagnostic> errs;
  auto out = PathRoundTrip(in, &errs);
  EXPECT_TRUE(errs.empty());
  EXPECT_EQ(CountVerbs(out), 0u);
}

TEST(PathCodec, SimpleMoveLineClose) {
  tgfx::Path in;
  in.moveTo(10.0f, 20.0f);
  in.lineTo(30.0f, 40.0f);
  in.lineTo(50.0f, 60.0f);
  in.close();

  std::vector<Diagnostic> errs;
  auto out = PathRoundTrip(in, &errs);
  EXPECT_TRUE(errs.empty());
  // tgfx's Path::Iterator surfaces synthetic segments — `close()` expands
  // into "implicit lineTo back to contour start + Close", so the iterated
  // verb count exceeds Path::countVerbs(). The wire format records what
  // the iterator emits, so the round-trip preserves iteration count.
  size_t iteratedIn = CountVerbs(in);
  EXPECT_EQ(CountVerbs(out), iteratedIn);
  std::vector<::tgfx::PathVerb> verbs;
  std::vector<float> coords;
  for (auto& seg : out) {
    if (seg.verb == ::tgfx::PathVerb::Done) break;
    verbs.push_back(seg.verb);
    if (seg.verb == ::tgfx::PathVerb::Move) {
      coords.push_back(seg.points[0].x);
      coords.push_back(seg.points[0].y);
    } else if (seg.verb == ::tgfx::PathVerb::Line) {
      // Iterator surfaces points[0]=start, points[1]=end (Path.h:373-378).
      coords.push_back(seg.points[1].x);
      coords.push_back(seg.points[1].y);
    }
  }
  EXPECT_EQ(verbs[0], ::tgfx::PathVerb::Move);
  EXPECT_EQ(verbs[1], ::tgfx::PathVerb::Line);
  EXPECT_EQ(verbs.back(), ::tgfx::PathVerb::Close);
  EXPECT_FLOAT_EQ(coords[0], 10.0f);
  EXPECT_FLOAT_EQ(coords[3], 40.0f);
}

TEST(PathCodec, QuadAndCubicAndConicRoundTrip) {
  tgfx::Path in;
  in.moveTo(0.0f, 0.0f);
  in.quadTo(10.0f, 20.0f, 30.0f, 40.0f);
  in.cubicTo(50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f);
  in.conicTo(110.0f, 120.0f, 130.0f, 140.0f, 0.7071f);

  std::vector<Diagnostic> errs;
  auto out = PathRoundTrip(in, &errs);
  EXPECT_TRUE(errs.empty());
  ASSERT_EQ(CountVerbs(out), 4u);

  for (auto& seg : out) {
    if (seg.verb == ::tgfx::PathVerb::Conic) {
      // Iterator: points[0]=start, points[1]=control, points[2]=end.
      EXPECT_FLOAT_EQ(seg.points[1].x, 110.0f);
      EXPECT_FLOAT_EQ(seg.points[2].y, 140.0f);
      EXPECT_NEAR(seg.conicWeight, 0.7071f, 1e-5f);
    }
  }
}

TEST(PathCodec, NaNCoordRejected) {
  // Encode a normal path then patch the first Move's x byte to NaN.
  tgfx::Path in;
  in.moveTo(10.0f, 10.0f);
  in.lineTo(20.0f, 20.0f);

  ::pag::StreamContext sc;
  ::pag::EncodeStream enc(&sc);
  WritePath(&enc, in);
  auto bytes = enc.release();

  // Layout: 1 byte format(0) + 1 byte verbCount varU32(2) +
  //         1 byte verb(Move) + 4 byte x + 4 byte y ...
  // Patch x at offset 3..6 with NaN bit pattern.
  uint8_t* p = const_cast<uint8_t*>(static_cast<const uint8_t*>(bytes->data()));
  p[3] = 0x00;
  p[4] = 0x00;
  p[5] = 0xC0;
  p[6] = 0x7F;  // f32 NaN little-endian

  DecodeContext ctx;
  ::pag::DecodeStream dec(&ctx.streamContext, bytes->data(), bytes->length());
  ctx.currentStream = &dec;
  auto out = ReadPath(&dec, &ctx);
  EXPECT_TRUE(ctx.hasError());
  EXPECT_EQ(ctx.errors[0].code, pagx::DiagnosticCode::MalformedTag);
  EXPECT_EQ(CountVerbs(out), 0u);
}

TEST(PathCodec, ConicWeightNegativeRejected) {
  tgfx::Path in;
  in.moveTo(0.0f, 0.0f);
  // Use weight != 1 so tgfx keeps the verb as Conic (weight==1 collapses
  // to Quad — see Path.h:180).
  in.conicTo(10.0f, 20.0f, 30.0f, 40.0f, 0.5f);

  ::pag::StreamContext sc;
  ::pag::EncodeStream enc(&sc);
  WritePath(&enc, in);
  auto bytes = enc.release();

  // Locate the conic weight: format(1) + verbCount varU32(2 = 1 byte)
  //   + Move verb(1) + 4+4 (Move xy) = 11 bytes consumed
  //   + Conic verb(1) + 4*4 (two control points xy) = 17 bytes after
  //   weight starts at offset 11 + 1 + 16 = 28 ... let's just patch the
  //   last 4 bytes of the buffer (weight is the trailing field).
  uint8_t* p = const_cast<uint8_t*>(static_cast<const uint8_t*>(bytes->data()));
  size_t weightOffset = bytes->length() - 4;
  // Store -1.0f little-endian.
  p[weightOffset + 0] = 0x00;
  p[weightOffset + 1] = 0x00;
  p[weightOffset + 2] = 0x80;
  p[weightOffset + 3] = 0xBF;

  DecodeContext ctx;
  ::pag::DecodeStream dec(&ctx.streamContext, bytes->data(), bytes->length());
  ctx.currentStream = &dec;
  auto out = ReadPath(&dec, &ctx);
  EXPECT_TRUE(ctx.hasError());
  EXPECT_EQ(ctx.errors[0].code, pagx::DiagnosticCode::MalformedTag);
}

TEST(PathCodec, UnsupportedFormatByte) {
  // Manually craft a path stream with format=0x05 — Phase 5b only supports
  // format=0 (raw float). Phase 12 will add format=1 (quantised); anything
  // else is fatal.
  std::vector<uint8_t> buf = {0x05, 0x00};  // format=5, verbCount=0
  ::pag::StreamContext sc;
  DecodeContext ctx;
  ::pag::DecodeStream dec(&ctx.streamContext, buf.data(), static_cast<uint32_t>(buf.size()));
  ctx.currentStream = &dec;
  auto out = ReadPath(&dec, &ctx);
  EXPECT_TRUE(ctx.hasError());
  EXPECT_EQ(ctx.errors[0].code, pagx::DiagnosticCode::MalformedTag);
}

TEST(PathCodec, VerbCountOverLimitFatal) {
  // varU32 verbCount = MAX_PATH_VERBS + 1. Decoder must reject before any
  // allocation.
  uint32_t over = limits::MAX_PATH_VERBS + 1;
  // Encode varU32 manually for the body — easier: use EncodeStream.
  ::pag::StreamContext sc;
  ::pag::EncodeStream enc(&sc);
  enc.writeUint8(0);  // format = 0
  enc.writeEncodedUint32(over);
  auto bytes = enc.release();

  DecodeContext ctx;
  ::pag::DecodeStream dec(&ctx.streamContext, bytes->data(), bytes->length());
  ctx.currentStream = &dec;
  auto out = ReadPath(&dec, &ctx);
  EXPECT_TRUE(ctx.hasError());
  EXPECT_EQ(ctx.errors[0].code, pagx::DiagnosticCode::PathVerbLimitExceeded);
  EXPECT_EQ(CountVerbs(out), 0u);
}

// =============================================================================
// End-to-end ShapePath roundtrip via Codec — confirms WritePathProperty +
// ReadPathProperty + ElementShapePath wiring all align.
// =============================================================================

TEST(PathCodec, ShapePathPathPropertyRoundTrip) {
  PAGDocument doc;
  doc.header.width = 100.0f;
  doc.header.height = 100.0f;

  auto comp = std::make_unique<Composition>();
  comp->id = "main";
  comp->width = 100;
  comp->height = 100;

  auto layer = std::make_unique<Layer>();
  layer->type = LayerType::Vector;
  layer->vector = std::make_unique<VectorPayload>();

  auto sp = std::make_unique<ElementShapePathData>();
  sp->position = MakeProp(::tgfx::Point{5.0f, 10.0f});
  tgfx::Path path;
  path.moveTo(0.0f, 0.0f);
  path.lineTo(50.0f, 0.0f);
  path.lineTo(50.0f, 50.0f);
  path.close();
  sp->path = MakeProp(std::move(path));
  sp->reversed = false;

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::ShapePath;
  el->payload = std::move(sp);
  layer->vector->contents.push_back(std::move(el));
  comp->layers.push_back(std::move(layer));
  doc.compositions.push_back(std::move(comp));

  auto enc = Codec::Encode(doc);
  auto decResult = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  ASSERT_NE(decResult.doc, nullptr);
  EXPECT_TRUE(decResult.errors.empty());

  const auto& outVec = decResult.doc->compositions[0]->layers[0]->vector;
  ASSERT_NE(outVec, nullptr);
  ASSERT_EQ(outVec->contents.size(), 1u);
  const auto& d = std::get<std::unique_ptr<ElementShapePathData>>(outVec->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->position.value.x, 5.0f);
  // Path round-trips at iteration-count parity (see SimpleMoveLineClose
  // comment about Close expansion).
  EXPECT_GT(CountVerbs(d->path.value), 0u);
}
