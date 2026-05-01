// Copyright (C) 2026 Tencent. All rights reserved.
#include "pagx/pag/PathCodec.h"
#include <cmath>
#include <cstdint>
#include "pagx/pag/ErrorCode.h"
#include "pagx/pag/limits.h"

namespace pagx::pag {

namespace {

// §D.2 verb kind byte values — fixed in the wire format.
constexpr uint8_t kVerbMove = 0;
constexpr uint8_t kVerbLine = 1;
constexpr uint8_t kVerbQuad = 2;
constexpr uint8_t kVerbConic = 3;
constexpr uint8_t kVerbCubic = 4;
constexpr uint8_t kVerbClose = 5;

constexpr uint8_t kPathFormatRawFloat = 0;
// Phase 12 will introduce kPathFormatQuantized = 1.

// Coordinate finite-range guard. Any |v| >= QUANTIZATION_MAX_COORD is
// rejected even in format=0 because downstream tgfx PathMeasure has the
// same overflow concerns the quantised path would have.
bool PathCoordIsFinite(float v) {
  return std::isfinite(v) && std::abs(v) < static_cast<float>(path_format::QUANTIZATION_MAX_COORD);
}

uint8_t WireVerbForPathVerb(::tgfx::PathVerb v) {
  switch (v) {
    case ::tgfx::PathVerb::Move:
      return kVerbMove;
    case ::tgfx::PathVerb::Line:
      return kVerbLine;
    case ::tgfx::PathVerb::Quad:
      return kVerbQuad;
    case ::tgfx::PathVerb::Conic:
      return kVerbConic;
    case ::tgfx::PathVerb::Cubic:
      return kVerbCubic;
    case ::tgfx::PathVerb::Close:
      return kVerbClose;
    case ::tgfx::PathVerb::Done:
      // Done is a sentinel returned by Path iteration when there are no
      // further segments — never written on the wire. The encode loop
      // breaks out before reaching this case; the explicit branch keeps
      // the switch exhaustive for the compiler.
      return kVerbClose;
  }
  return kVerbClose;
}

}  // namespace

uint8_t ChoosePathFormat(const tgfx::Path& /*path*/) {
  // Phase 5b: format=0 always. Phase 12 will widen this with a verbCount /
  // coord-range check per §D.2 P1-8 ChoosePathFormat rules.
  return kPathFormatRawFloat;
}

void WritePath(::pag::EncodeStream* stream, const tgfx::Path& path) {
  uint8_t format = ChoosePathFormat(path);
  stream->writeUint8(format);

  // Count verbs by walking the iterator. Path::countVerbs() reports the
  // logical verb count (e.g. 4 for "moveTo + 2 lineTo + close") but the
  // Iterator may surface extra synthetic segments — for example tgfx
  // expands `close()` into "implicit lineTo back to contour start + Close",
  // so the iterator surfaces 5 segments for that same path. We must count
  // and write what the iterator actually emits, otherwise verbCount will
  // not match the per-verb bytes produced below.
  uint32_t verbCount = 0;
  for (auto& segment : path) {
    if (segment.verb == ::tgfx::PathVerb::Done) {
      break;
    }
    ++verbCount;
  }
  stream->writeEncodedUint32(verbCount);

  // Format=0 payload: u8 verbKind + per-verb point bytes.
  //
  // tgfx::Path::Iterator surfaces points as follows (Path.h:373-378):
  //   Move:  points[0]                       — the new pen position
  //   Line:  points[0]=start, points[1]=end  — start is the previous pen pos
  //   Quad:  points[0]=start, points[1..2]   — control + end
  //   Conic: points[0]=start, points[1..2]   — control + end
  //   Cubic: points[0]=start, points[1..3]   — control1 + control2 + end
  // We drop the implicit start point (the wire format derives it from the
  // running pen position; §D.2.1).
  for (auto& segment : path) {
    if (segment.verb == ::tgfx::PathVerb::Done) {
      break;
    }
    stream->writeUint8(WireVerbForPathVerb(segment.verb));
    switch (segment.verb) {
      case ::tgfx::PathVerb::Move:
        stream->writeFloat(segment.points[0].x);
        stream->writeFloat(segment.points[0].y);
        break;
      case ::tgfx::PathVerb::Line:
        stream->writeFloat(segment.points[1].x);
        stream->writeFloat(segment.points[1].y);
        break;
      case ::tgfx::PathVerb::Quad:
        stream->writeFloat(segment.points[1].x);
        stream->writeFloat(segment.points[1].y);
        stream->writeFloat(segment.points[2].x);
        stream->writeFloat(segment.points[2].y);
        break;
      case ::tgfx::PathVerb::Conic:
        stream->writeFloat(segment.points[1].x);
        stream->writeFloat(segment.points[1].y);
        stream->writeFloat(segment.points[2].x);
        stream->writeFloat(segment.points[2].y);
        stream->writeFloat(segment.conicWeight);
        break;
      case ::tgfx::PathVerb::Cubic:
        stream->writeFloat(segment.points[1].x);
        stream->writeFloat(segment.points[1].y);
        stream->writeFloat(segment.points[2].x);
        stream->writeFloat(segment.points[2].y);
        stream->writeFloat(segment.points[3].x);
        stream->writeFloat(segment.points[3].y);
        break;
      case ::tgfx::PathVerb::Close:
      case ::tgfx::PathVerb::Done:
        break;
    }
  }
}

tgfx::Path ReadPath(::pag::DecodeStream* stream, DecodeContext* ctx) {
  tgfx::Path empty;
  uint8_t format = stream->readUint8();
  if (format != kPathFormatRawFloat) {
    // Phase 5b only knows format=0. Phase 12 will widen to format=1.
    ctx->error(ErrorCode::MalformedTag, "Path pathFormat unsupported in this build");
    return empty;
  }

  uint32_t verbCount = stream->readEncodedUint32();
  if (verbCount > limits::MAX_PATH_VERBS) {
    ctx->error(ErrorCode::PathVerbLimitExceeded, "Path verb count exceeds MAX_PATH_VERBS");
    return empty;
  }

  tgfx::Path out;
  for (uint32_t i = 0; i < verbCount; ++i) {
    uint8_t v = stream->readUint8();
    switch (v) {
      case kVerbMove: {
        float x = stream->readFloat();
        float y = stream->readFloat();
        if (!PathCoordIsFinite(x) || !PathCoordIsFinite(y)) {
          ctx->error(ErrorCode::MalformedTag, "Path Move coord NaN/Inf/out-of-range");
          return empty;
        }
        out.moveTo(x, y);
        break;
      }
      case kVerbLine: {
        float x = stream->readFloat();
        float y = stream->readFloat();
        if (!PathCoordIsFinite(x) || !PathCoordIsFinite(y)) {
          ctx->error(ErrorCode::MalformedTag, "Path Line coord NaN/Inf/out-of-range");
          return empty;
        }
        out.lineTo(x, y);
        break;
      }
      case kVerbQuad: {
        float x0 = stream->readFloat();
        float y0 = stream->readFloat();
        float x1 = stream->readFloat();
        float y1 = stream->readFloat();
        if (!PathCoordIsFinite(x0) || !PathCoordIsFinite(y0) || !PathCoordIsFinite(x1) ||
            !PathCoordIsFinite(y1)) {
          ctx->error(ErrorCode::MalformedTag, "Path Quad coord NaN/Inf/out-of-range");
          return empty;
        }
        out.quadTo(x0, y0, x1, y1);
        break;
      }
      case kVerbConic: {
        float x0 = stream->readFloat();
        float y0 = stream->readFloat();
        float x1 = stream->readFloat();
        float y1 = stream->readFloat();
        float w = stream->readFloat();
        if (!PathCoordIsFinite(x0) || !PathCoordIsFinite(y0) || !PathCoordIsFinite(x1) ||
            !PathCoordIsFinite(y1)) {
          ctx->error(ErrorCode::MalformedTag, "Path Conic coord NaN/Inf/out-of-range");
          return empty;
        }
        // Conic weight valid range (0, +inf); cap at 1e6 to avoid rasterizer
        // pathological cases (§D.2.1).
        if (!std::isfinite(w) || w <= 0.0f || w > 1.0e6f) {
          ctx->error(ErrorCode::MalformedTag, "Path Conic weight NaN/Inf/out-of-range");
          return empty;
        }
        out.conicTo(x0, y0, x1, y1, w);
        break;
      }
      case kVerbCubic: {
        float x0 = stream->readFloat();
        float y0 = stream->readFloat();
        float x1 = stream->readFloat();
        float y1 = stream->readFloat();
        float x2 = stream->readFloat();
        float y2 = stream->readFloat();
        if (!PathCoordIsFinite(x0) || !PathCoordIsFinite(y0) || !PathCoordIsFinite(x1) ||
            !PathCoordIsFinite(y1) || !PathCoordIsFinite(x2) || !PathCoordIsFinite(y2)) {
          ctx->error(ErrorCode::MalformedTag, "Path Cubic coord NaN/Inf/out-of-range");
          return empty;
        }
        out.cubicTo(x0, y0, x1, y1, x2, y2);
        break;
      }
      case kVerbClose: {
        out.close();
        break;
      }
      default: {
        ctx->error(ErrorCode::MalformedTag, "Path verb byte invalid");
        return empty;
      }
    }
  }
  return out;
}

}  // namespace pagx::pag
