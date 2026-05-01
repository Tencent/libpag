// Copyright (C) 2026 Tencent. All rights reserved.
//
// GlyphRunBlob byte codec.
//
// §D.11 layout recap:
//   u8  kind
//   u32 fontIndex         (UINT32_MAX = missing)
//   f32 fontSize
//   Matrix inverseMatrix  (variable-length, via WriteMatrix/ReadMatrix)
//   u8  glyphPrecisionLog2
//   varU32 glyphCount
//   u16[glyphCount] glyphId
//   switch (kind):
//     LayoutRun:
//       int32List  positionXY (2 × glyphCount)
//       bitstream  hasXformBits (glyphCount bits, byte-aligned after)
//       (for each glyph with hasXform=1) f32 scos, ssin, tx, ty
//     ClassicGlyphRun:
//       f32 baseX, baseY
//       floatList  xOffsets (glyphCount, precision = 2^-log2)
//       int32List  positionXY (2 × glyphCount)
//       int32List  anchorXY  (2 × glyphCount)
//       floatList  scaleXY   (2 × glyphCount, precision = 0.005)
//       floatList  rotations (glyphCount, precision = 0.1)
//       floatList  skews     (glyphCount, precision = 0.1)
//
// Quantised list helpers expect their bit-streams to start byte-aligned.
// That already holds after WriteMatrix (byte-aligned output) + writeUint8 +
// writeEncodedUint32 + writeUint16 × N, but each list leaves a 5-bit
// numBits header plus potentially ragged bits behind — we call
// alignWithBytes() between arrays so the next byte-level field (e.g. f32
// baseX for ClassicGlyphRun) still lines up.
#include "pagx/pag/GlyphRunCodec.h"
#include <cmath>
#include <cstring>
#include <vector>
#include "pagx/pag/ValueCodec.h"
#include "pagx/pag/limits.h"

namespace pagx::pag {

namespace {

constexpr float kScalePrecision = 0.005f;     // §D.11 ClassicGlyphRun scales
constexpr float kRotationPrecision = 0.1f;    // §D.11 ClassicGlyphRun rotations (deg)
constexpr float kSkewPrecision = 0.1f;        // §D.11 ClassicGlyphRun skews (deg)
constexpr uint8_t kDefaultPrecisionLog2 = 4;  // 2^-4 = 0.0625 px; §D.11 suggested default

// Converts a float array back from quantised int32 using the matching
// precision. Kept as a named helper (instead of inlined at each call site)
// so the two branches read consistently.
void DequantiseInts(const int32_t* src, uint32_t count, float precision, std::vector<float>* out) {
  out->resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    (*out)[i] = static_cast<float>(src[i]) * precision;
  }
}

}  // namespace

// ---- Write ----

void WriteGlyphRunBlobInline(::pag::EncodeStream* body, const GlyphRunBlob& blob) {
  body->writeUint8(static_cast<uint8_t>(blob.kind));
  body->writeUint32(blob.fontIndex);
  body->writeFloat(blob.fontSize);
  WriteMatrix(body, blob.inverseMatrix);
  body->writeUint8(kDefaultPrecisionLog2);
  const float positionPrecision = 1.0f / static_cast<float>(1u << kDefaultPrecisionLog2);
  const int32_t positionScale = 1 << kDefaultPrecisionLog2;

  if (blob.kind == GlyphRunKind::LayoutRun) {
    const auto& glyphs = blob.layoutGlyphs;
    const auto glyphCount = static_cast<uint32_t>(glyphs.size());
    body->writeEncodedUint32(glyphCount);

    for (const auto& g : glyphs) {
      body->writeUint16(g.glyphId);
    }

    // positionXY — 2 × count quantised int32s
    std::vector<int32_t> positions;
    positions.reserve(glyphCount * 2);
    for (const auto& g : glyphs) {
      positions.push_back(static_cast<int32_t>(std::roundf(g.position.x * positionScale)));
      positions.push_back(static_cast<int32_t>(std::roundf(g.position.y * positionScale)));
    }
    body->writeInt32List(positions.data(), glyphCount * 2);
    body->alignWithBytes();

    // hasXformBits — one bit per glyph. writeUBits writes MSB-first inside
    // each u8 byte; the Reader mirrors it with readUBits(1).
    for (const auto& g : glyphs) {
      body->writeUBits(g.hasXform ? 1u : 0u, 1);
    }
    body->alignWithBytes();

    // xform payload — dense array of (scos, ssin, tx, ty) quads for glyphs
    // that set hasXform. Kept raw f32 (no quantisation) because xforms are
    // usually zero-count or very small for pre-shaped runs and the write
    // cost is trivial; Phase 12 can add a quantised path if profiles call
    // for it.
    for (const auto& g : glyphs) {
      if (!g.hasXform) continue;
      body->writeFloat(g.scos);
      body->writeFloat(g.ssin);
      body->writeFloat(g.tx);
      body->writeFloat(g.ty);
    }
    return;
  }

  // ClassicGlyphRun
  const auto& glyphs = blob.classicGlyphs;
  const auto glyphCount = static_cast<uint32_t>(glyphs.size());
  body->writeEncodedUint32(glyphCount);

  for (const auto& g : glyphs) {
    body->writeUint16(g.glyphId);
  }

  body->writeFloat(blob.baseX);
  body->writeFloat(blob.baseY);

  // xOffsets — quantised floats at positionPrecision (§D.11 "precision = 2^-log2")
  std::vector<float> xOffsets;
  xOffsets.reserve(glyphCount);
  for (const auto& g : glyphs) {
    xOffsets.push_back(g.xOffset);
  }
  body->writeFloatList(xOffsets.data(), glyphCount, positionPrecision);
  body->alignWithBytes();

  // positionXY
  std::vector<int32_t> positions;
  positions.reserve(glyphCount * 2);
  for (const auto& g : glyphs) {
    positions.push_back(static_cast<int32_t>(std::roundf(g.position.x * positionScale)));
    positions.push_back(static_cast<int32_t>(std::roundf(g.position.y * positionScale)));
  }
  body->writeInt32List(positions.data(), glyphCount * 2);
  body->alignWithBytes();

  // anchorXY
  std::vector<int32_t> anchors;
  anchors.reserve(glyphCount * 2);
  for (const auto& g : glyphs) {
    anchors.push_back(static_cast<int32_t>(std::roundf(g.anchor.x * positionScale)));
    anchors.push_back(static_cast<int32_t>(std::roundf(g.anchor.y * positionScale)));
  }
  body->writeInt32List(anchors.data(), glyphCount * 2);
  body->alignWithBytes();

  // scaleXY — precision 0.005 per §D.11
  std::vector<float> scales;
  scales.reserve(glyphCount * 2);
  for (const auto& g : glyphs) {
    scales.push_back(g.scale.x);
    scales.push_back(g.scale.y);
  }
  body->writeFloatList(scales.data(), glyphCount * 2, kScalePrecision);
  body->alignWithBytes();

  std::vector<float> rotations;
  rotations.reserve(glyphCount);
  for (const auto& g : glyphs) {
    rotations.push_back(g.rotation);
  }
  body->writeFloatList(rotations.data(), glyphCount, kRotationPrecision);
  body->alignWithBytes();

  std::vector<float> skews;
  skews.reserve(glyphCount);
  for (const auto& g : glyphs) {
    skews.push_back(g.skew);
  }
  body->writeFloatList(skews.data(), glyphCount, kSkewPrecision);
  body->alignWithBytes();
}

// ---- Read ----

bool ReadGlyphRunBlobInline(::pag::DecodeStream* stream, DecodeContext* ctx, uint32_t tagEnd,
                            GlyphRunBlob* out) {
  // Byte-level header fields come first; defensively guard each read against
  // the enclosing Tag end so a truncated stream doesn't walk off the buffer.
  if (stream->position() + 1 > tagEnd) {
    ctx->error(ErrorCode::TruncatedData, "GlyphRunBlob header truncated (kind)");
    return false;
  }
  uint8_t kindByte = stream->readUint8();
  if (kindByte > static_cast<uint8_t>(GlyphRunKind::ClassicGlyphRun)) {
    ctx->warn(ErrorCode::InvalidEnumValue, "GlyphRunBlob.kind out of range; using LayoutRun");
    out->kind = GlyphRunKind::LayoutRun;
  } else {
    out->kind = static_cast<GlyphRunKind>(kindByte);
  }

  if (stream->position() + 4 > tagEnd) {
    ctx->error(ErrorCode::TruncatedData, "GlyphRunBlob truncated (fontIndex)");
    return false;
  }
  out->fontIndex = stream->readUint32();

  if (stream->position() + 4 > tagEnd) {
    ctx->error(ErrorCode::TruncatedData, "GlyphRunBlob truncated (fontSize)");
    return false;
  }
  out->fontSize = stream->readFloat();

  // Matrix read is variable-length (1 byte identity / translate / full).
  // ReadMatrix does its own internal bounds check via the stream, but we
  // still need enough room for at least the header byte.
  if (stream->position() + 1 > tagEnd) {
    ctx->error(ErrorCode::TruncatedData, "GlyphRunBlob truncated (inverseMatrix header)");
    return false;
  }
  out->inverseMatrix = ReadMatrix(stream);

  if (stream->position() + 1 > tagEnd) {
    ctx->error(ErrorCode::TruncatedData, "GlyphRunBlob truncated (precisionLog2)");
    return false;
  }
  uint8_t precisionLog2 = stream->readUint8();
  if (precisionLog2 > 30) {
    // A precisionLog2 > 30 makes `1 << precisionLog2` overflow int32.
    // Clamp + warn so future attackers can't trigger UB via a handcrafted
    // byte.
    ctx->warn(ErrorCode::InvalidEnumValue, "GlyphRunBlob.precisionLog2 out of range; clamped to 4");
    precisionLog2 = 4;
  }
  const float positionPrecision = 1.0f / static_cast<float>(1u << precisionLog2);

  DiagnosticCollectorGuard guard;
  guard.collector = static_cast<DiagnosticCollector*>(ctx);
  uint32_t glyphCount = ReadEncodedUint32Safe(stream, &guard);
  if (glyphCount > limits::MAX_GLYPHS_PER_RUN) {
    ctx->error(ErrorCode::StructureLimitExceeded,
               "GlyphRunBlob glyphCount exceeds MAX_GLYPHS_PER_RUN");
    return false;
  }
  // Roughly gate the per-glyph byte cost: minimum 2 bytes per glyph for the
  // u16 glyphId array, so glyphCount * 2 must not exceed the remaining Tag
  // body. This prevents a malicious stream from asking us to allocate huge
  // vectors before we've read enough to confirm they'll fill.
  if (stream->position() + static_cast<uint64_t>(glyphCount) * 2 > tagEnd) {
    ctx->error(ErrorCode::TruncatedData, "GlyphRunBlob glyphCount claims more bytes than Tag body");
    return false;
  }

  if (out->kind == GlyphRunKind::LayoutRun) {
    out->layoutGlyphs.clear();
    out->layoutGlyphs.resize(glyphCount);
    for (uint32_t i = 0; i < glyphCount; ++i) {
      out->layoutGlyphs[i].glyphId = stream->readUint16();
    }
    // positionXY
    std::vector<int32_t> positions(glyphCount * 2);
    stream->readInt32List(positions.data(), glyphCount * 2);
    stream->alignWithBytes();
    std::vector<float> positionsFloat;
    DequantiseInts(positions.data(), glyphCount * 2, positionPrecision, &positionsFloat);
    for (uint32_t i = 0; i < glyphCount; ++i) {
      out->layoutGlyphs[i].position.x = positionsFloat[i * 2 + 0];
      out->layoutGlyphs[i].position.y = positionsFloat[i * 2 + 1];
    }

    // hasXformBits
    std::vector<uint8_t> hasXform(glyphCount, 0u);
    uint32_t xformCount = 0;
    for (uint32_t i = 0; i < glyphCount; ++i) {
      hasXform[i] = static_cast<uint8_t>(stream->readUBits(1));
      if (hasXform[i]) {
        ++xformCount;
      }
    }
    stream->alignWithBytes();

    // Dense xform payload — 16 bytes per glyph carrying a set bit.
    if (stream->position() + static_cast<uint64_t>(xformCount) * 16 > tagEnd) {
      ctx->error(ErrorCode::TruncatedData, "GlyphRunBlob layoutRun xform payload truncated");
      return false;
    }
    for (uint32_t i = 0; i < glyphCount; ++i) {
      auto& g = out->layoutGlyphs[i];
      g.hasXform = hasXform[i] != 0;
      if (g.hasXform) {
        g.scos = stream->readFloat();
        g.ssin = stream->readFloat();
        g.tx = stream->readFloat();
        g.ty = stream->readFloat();
      }
    }
    return true;
  }

  // ClassicGlyphRun
  out->classicGlyphs.clear();
  out->classicGlyphs.resize(glyphCount);
  for (uint32_t i = 0; i < glyphCount; ++i) {
    out->classicGlyphs[i].glyphId = stream->readUint16();
  }

  if (stream->position() + 8 > tagEnd) {
    ctx->error(ErrorCode::TruncatedData, "GlyphRunBlob classicRun base X/Y truncated");
    return false;
  }
  out->baseX = stream->readFloat();
  out->baseY = stream->readFloat();

  // xOffsets
  std::vector<float> xOffsets(glyphCount);
  stream->readFloatList(xOffsets.data(), glyphCount, positionPrecision);
  stream->alignWithBytes();
  for (uint32_t i = 0; i < glyphCount; ++i) {
    out->classicGlyphs[i].xOffset = xOffsets[i];
  }

  // positionXY
  std::vector<int32_t> positions(glyphCount * 2);
  stream->readInt32List(positions.data(), glyphCount * 2);
  stream->alignWithBytes();
  for (uint32_t i = 0; i < glyphCount; ++i) {
    out->classicGlyphs[i].position.x = static_cast<float>(positions[i * 2 + 0]) * positionPrecision;
    out->classicGlyphs[i].position.y = static_cast<float>(positions[i * 2 + 1]) * positionPrecision;
  }

  // anchorXY
  std::vector<int32_t> anchors(glyphCount * 2);
  stream->readInt32List(anchors.data(), glyphCount * 2);
  stream->alignWithBytes();
  for (uint32_t i = 0; i < glyphCount; ++i) {
    out->classicGlyphs[i].anchor.x = static_cast<float>(anchors[i * 2 + 0]) * positionPrecision;
    out->classicGlyphs[i].anchor.y = static_cast<float>(anchors[i * 2 + 1]) * positionPrecision;
  }

  // scaleXY (precision 0.005)
  std::vector<float> scales(glyphCount * 2);
  stream->readFloatList(scales.data(), glyphCount * 2, kScalePrecision);
  stream->alignWithBytes();
  for (uint32_t i = 0; i < glyphCount; ++i) {
    out->classicGlyphs[i].scale.x = scales[i * 2 + 0];
    out->classicGlyphs[i].scale.y = scales[i * 2 + 1];
  }

  std::vector<float> rotations(glyphCount);
  stream->readFloatList(rotations.data(), glyphCount, kRotationPrecision);
  stream->alignWithBytes();
  for (uint32_t i = 0; i < glyphCount; ++i) {
    out->classicGlyphs[i].rotation = rotations[i];
  }

  std::vector<float> skews(glyphCount);
  stream->readFloatList(skews.data(), glyphCount, kSkewPrecision);
  stream->alignWithBytes();
  for (uint32_t i = 0; i < glyphCount; ++i) {
    out->classicGlyphs[i].skew = skews[i];
  }

  return true;
}

}  // namespace pagx::pag
