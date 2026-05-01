// Copyright (C) 2026 Tencent. All rights reserved.
//
// Single entry point for every Read/Write operation used by the PAG v2 codec.
// Thin wrappers on top of v1 pag::EncodeStream / pag::DecodeStream — the v1
// toolbox is authoritative for bit-level / varint work; anything new lives
// here. Design doc authority: §4.3 (Property header), §D.1 (general
// conventions), §D.1 Matrix / Color variants.
//
// Phase 1 scope: primitive types (bool/int/float), Color (4×u8 RGBA), Point,
// Rect, Matrix variant, Matrix3D variant, and the three length-prefixed safe
// wrappers (ReadUtf8String / ReadLengthPrefixedBytes / ReadEncodedUint32Safe).
// Path / GlyphRun / Enum specializations land in Phase 5 / 7 / 8 together
// with their baker submodules — the specializations can be added here without
// breaking Phase 1 callers.
#pragma once

#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pagx/pag/DiagnosticCollector.h"
#include "pagx/pag/ErrorCode.h"
#include "pagx/pag/limits.h"
#include "tgfx/core/Color.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Matrix3D.h"
#include "tgfx/core/Point.h"
#include "tgfx/core/Rect.h"

namespace pagx::pag {

// ============================================================================
// Primitive scalar helpers — direct pass-through to v1 EncodeStream /
// DecodeStream. Kept here so Baker / Codec / Inflater only ever include
// ValueCodec.h, never the v1 stream headers directly.
// ============================================================================

inline void WriteBool(::pag::EncodeStream* s, bool v) {
  s->writeUint8(v ? uint8_t{1} : uint8_t{0});
}

inline bool ReadBool(::pag::DecodeStream* s) {
  return s->readUint8() != 0;
}

inline void WriteUint8(::pag::EncodeStream* s, uint8_t v) {
  s->writeUint8(v);
}
inline uint8_t ReadUint8(::pag::DecodeStream* s) {
  return s->readUint8();
}
inline void WriteUint16(::pag::EncodeStream* s, uint16_t v) {
  s->writeUint16(v);
}
inline uint16_t ReadUint16(::pag::DecodeStream* s) {
  return s->readUint16();
}
inline void WriteUint32(::pag::EncodeStream* s, uint32_t v) {
  s->writeUint32(v);
}
inline uint32_t ReadUint32(::pag::DecodeStream* s) {
  return s->readUint32();
}
inline void WriteInt32(::pag::EncodeStream* s, int32_t v) {
  s->writeInt32(v);
}
inline int32_t ReadInt32(::pag::DecodeStream* s) {
  return s->readInt32();
}
inline void WriteFloat(::pag::EncodeStream* s, float v) {
  s->writeFloat(v);
}
inline float ReadFloat(::pag::DecodeStream* s) {
  return s->readFloat();
}

// ============================================================================
// Color — 4×u8 RGBA quantization (design doc §D.1). The in-memory
// representation is tgfx::Color (4×float), but the wire format is 8-bit per
// channel to keep file size proportional to perceptual precision.
// ============================================================================

inline uint8_t FloatToU8(float v) {
  // clamp to [0, 1] then round to nearest u8; NaN → 0.
  if (!(v > 0.0f)) {
    return 0;
  }
  if (v >= 1.0f) {
    return 255;
  }
  return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

inline float U8ToFloat(uint8_t v) {
  return static_cast<float>(v) * (1.0f / 255.0f);
}

inline void WriteColor(::pag::EncodeStream* s, const tgfx::Color& c) {
  s->writeUint8(FloatToU8(c.red));
  s->writeUint8(FloatToU8(c.green));
  s->writeUint8(FloatToU8(c.blue));
  s->writeUint8(FloatToU8(c.alpha));
}

inline tgfx::Color ReadColor(::pag::DecodeStream* s) {
  tgfx::Color c{};
  c.red = U8ToFloat(s->readUint8());
  c.green = U8ToFloat(s->readUint8());
  c.blue = U8ToFloat(s->readUint8());
  c.alpha = U8ToFloat(s->readUint8());
  return c;
}

// ============================================================================
// Point / Rect — plain float dumps.
// ============================================================================

inline void WritePoint(::pag::EncodeStream* s, const tgfx::Point& p) {
  s->writeFloat(p.x);
  s->writeFloat(p.y);
}

inline tgfx::Point ReadPoint(::pag::DecodeStream* s) {
  tgfx::Point p{};
  p.x = s->readFloat();
  p.y = s->readFloat();
  return p;
}

inline void WriteRect(::pag::EncodeStream* s, const tgfx::Rect& r) {
  s->writeFloat(r.left);
  s->writeFloat(r.top);
  s->writeFloat(r.right);
  s->writeFloat(r.bottom);
}

inline tgfx::Rect ReadRect(::pag::DecodeStream* s) {
  tgfx::Rect r{};
  r.left = s->readFloat();
  r.top = s->readFloat();
  r.right = s->readFloat();
  r.bottom = s->readFloat();
  return r;
}

// ============================================================================
// Matrix / Matrix3D — variable-length encoding. Design doc §D.1.
//
//   u8 matrixHeader:
//     bit 0 = isIdentity       — matrix == I, no payload follows
//     bit 1 = hasTranslateOnly — only transX/transY differ from I
//     bit 2-7 = reserved (must be 0 on write; ignored on read)
//
// Full 2D matrix payload is 6 floats: scaleX / skewX / transX / skewY /
// scaleY / transY. Matrix3D fallback emits row-major 16 floats.
// ============================================================================

inline void WriteMatrix(::pag::EncodeStream* s, const tgfx::Matrix& m) {
  const bool isIdentity = (m == tgfx::Matrix::I());
  const bool hasTranslateOnly = !isIdentity && m.getScaleX() == 1.0f && m.getSkewX() == 0.0f &&
                                m.getSkewY() == 0.0f && m.getScaleY() == 1.0f;

  uint8_t header = 0;
  if (isIdentity) {
    header |= 0x01u;
  } else if (hasTranslateOnly) {
    header |= 0x02u;
  }
  s->writeUint8(header);

  if (isIdentity) {
    return;
  }
  if (hasTranslateOnly) {
    s->writeFloat(m.getTranslateX());
    s->writeFloat(m.getTranslateY());
    return;
  }
  s->writeFloat(m.getScaleX());
  s->writeFloat(m.getSkewX());
  s->writeFloat(m.getTranslateX());
  s->writeFloat(m.getSkewY());
  s->writeFloat(m.getScaleY());
  s->writeFloat(m.getTranslateY());
}

inline tgfx::Matrix ReadMatrix(::pag::DecodeStream* s) {
  uint8_t header = s->readUint8();
  if ((header & 0x01u) != 0) {
    return tgfx::Matrix::I();
  }
  if ((header & 0x02u) != 0) {
    tgfx::Matrix m = tgfx::Matrix::I();
    float tx = s->readFloat();
    float ty = s->readFloat();
    m.setTranslateX(tx);
    m.setTranslateY(ty);
    return m;
  }
  tgfx::Matrix m{};
  float scaleX = s->readFloat();
  float skewX = s->readFloat();
  float transX = s->readFloat();
  float skewY = s->readFloat();
  float scaleY = s->readFloat();
  float transY = s->readFloat();
  m.setAll(scaleX, skewX, transX, skewY, scaleY, transY);
  return m;
}

inline void WriteMatrix3D(::pag::EncodeStream* s, const tgfx::Matrix3D& m) {
  const bool isIdentity = (m == tgfx::Matrix3D::I());
  uint8_t header = 0;
  if (isIdentity) {
    header |= 0x01u;
  }
  s->writeUint8(header);
  if (isIdentity) {
    return;
  }
  // Serialize in the same order as tgfx::Matrix3D::setAll's parameter list —
  // the wire buffer therefore carries whatever convention setAll uses
  // internally, and roundtrip is trivially correct regardless of row / column
  // major interpretation. Indexing: buf[c * 4 + r] by getRowColumn semantics.
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      s->writeFloat(m.getRowColumn(r, c));
    }
  }
}

inline tgfx::Matrix3D ReadMatrix3D(::pag::DecodeStream* s) {
  uint8_t header = s->readUint8();
  if ((header & 0x01u) != 0) {
    return tgfx::Matrix3D::I();
  }
  // Read column-major (matching the writer above) and rebuild via the public
  // setRowColumn API — setAll / setColumn live in the private section of
  // tgfx::Matrix3D, so element-by-element is the only stable path.
  tgfx::Matrix3D m{};
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      m.setRowColumn(r, c, s->readFloat());
    }
  }
  return m;
}

// ============================================================================
// Length-prefixed safe wrappers — §D.1 P0 guard.
// Every "varU32 length + N bytes" field must flow through these so a
// maliciously large length prefix cannot trigger a multi-GB allocator call.
// ============================================================================

// DiagnosticCollectorGuard is a thin shim that lets ValueCodec emit warnings
// without pulling in the full DecodeContext. Phase 4 will plumb the real
// DecodeContext through the guard; Phase 1 test code passes a collector
// directly. Phase 1 callers may also pass `nullptr` to skip diagnostics in
// unit tests that only care about return values.
struct DiagnosticCollectorGuard {
  DiagnosticCollector* collector = nullptr;

  void warn(ErrorCode code, std::string msg = {}) {
    if (collector == nullptr) {
      return;
    }
    // Reach the protected pushWarning via a sibling bridge — ValueCodec is
    // the canonical entry point, so it is allowed to know about the base.
    struct Bridge : DiagnosticCollector {
      using DiagnosticCollector::pushWarning;
    };
    Diagnostic d{code, std::move(msg), 0, std::numeric_limits<uint32_t>::max()};
    static_cast<Bridge*>(collector)->pushWarning(std::move(d));
  }
};

// Reads an encoded uint32 with the ULEB128 5-byte hard stop enforced — if v1
// consumed more than 5 bytes, the payload had an illegal continuation chain,
// so we emit MalformedTag. Well-formed streams never hit the guard because
// v1 writeEncodedUint32 always emits ≤ 5 bytes.
inline uint32_t ReadEncodedUint32Safe(::pag::DecodeStream* s, DiagnosticCollectorGuard* guard) {
  uint32_t before = s->position();
  uint32_t value = s->readEncodedUint32();
  uint32_t after = s->position();
  if (after - before > 5 && guard != nullptr) {
    guard->warn(ErrorCode::MalformedTag, "varU32 exceeded 5-byte ULEB128 hard stop");
  }
  return value;
}

// Reads a varU32-prefixed UTF-8 string guarded by maxBytes. Any prefix bigger
// than either maxBytes or the stream's remaining bytes returns "" and warns.
// UTF-8 validity check is done via a minimal inline scanner — a full RFC 3629
// state machine can swap in later without changing the signature.
inline bool IsValidUtf8(const uint8_t* data, size_t len) {
  // Tight scanner — accepts 1-4 byte sequences per RFC 3629. Rejects
  // surrogate halves U+D800..U+DFFF and non-shortest forms.
  size_t i = 0;
  while (i < len) {
    uint8_t b0 = data[i];
    if (b0 < 0x80) {
      i += 1;
      continue;
    }
    uint8_t need = 0;
    uint32_t code = 0;
    uint32_t minValue = 0;
    if ((b0 & 0xE0u) == 0xC0u) {
      need = 1;
      code = b0 & 0x1Fu;
      minValue = 0x80;
    } else if ((b0 & 0xF0u) == 0xE0u) {
      need = 2;
      code = b0 & 0x0Fu;
      minValue = 0x800;
    } else if ((b0 & 0xF8u) == 0xF0u) {
      need = 3;
      code = b0 & 0x07u;
      minValue = 0x10000;
    } else {
      return false;
    }
    if (i + need >= len) {
      return false;
    }
    for (uint8_t k = 1; k <= need; ++k) {
      uint8_t bk = data[i + k];
      if ((bk & 0xC0u) != 0x80u) {
        return false;
      }
      code = (code << 6) | (bk & 0x3Fu);
    }
    if (code < minValue) {
      return false;  // non-shortest form
    }
    if (code >= 0xD800 && code <= 0xDFFF) {
      return false;  // UTF-16 surrogate half
    }
    if (code > 0x10FFFF) {
      return false;  // out of Unicode range
    }
    i += need + 1;
  }
  return true;
}

inline std::string ReadUtf8String(::pag::DecodeStream* s, DiagnosticCollectorGuard* guard,
                                  size_t maxBytes) {
  uint32_t n = ReadEncodedUint32Safe(s, guard);
  if (n > maxBytes || n > s->bytesAvailable()) {
    if (guard != nullptr) {
      guard->warn(ErrorCode::StringInvalidUtf8, "utf8 string length exceeds limit or stream");
    }
    // Skip as many bytes as we can still reach so the Tag's seek-to-end stays sane.
    uint32_t skippable = std::min<uint32_t>(n, s->bytesAvailable());
    s->skip(skippable);
    return {};
  }
  std::string out;
  out.resize(n);
  if (n > 0) {
    // v1 readBytes(length) returns a sub-stream carrying the read buffer; copy
    // from it via its data() pointer so we never stash raw vector<uint8_t>.
    ::pag::DecodeStream sub = s->readBytes(n);
    std::memcpy(out.data(), sub.data(), n);
  }
  if (!IsValidUtf8(reinterpret_cast<const uint8_t*>(out.data()), out.size())) {
    if (guard != nullptr) {
      guard->warn(ErrorCode::StringInvalidUtf8, "invalid UTF-8 byte sequence");
    }
    return {};
  }
  return out;
}

// Symmetric writer for ReadUtf8String. Encodes `s.size()` as varU32 followed
// by the raw bytes. v1 writeBytes() takes a non-const uint8_t* — the cast is
// safe because EncodeStream only reads the input buffer.
inline void WriteUtf8String(::pag::EncodeStream* stream, const std::string& s) {
  stream->writeEncodedUint32(static_cast<uint32_t>(s.size()));
  if (!s.empty()) {
    stream->writeBytes(reinterpret_cast<uint8_t*>(const_cast<char*>(s.data())),
                       static_cast<uint32_t>(s.size()));
  }
}

// Reads varU32 length + length bytes of opaque data. Owning buffer is handed
// back via shared_ptr<const tgfx::Data>. Zero-copy path is intentionally NOT
// wired here — Phase 10.5 will add a ZeroCopyScope hook that flips the
// allocation strategy. errorCode selects which warn fires on overrun
// (ImageResourceSizeExceeded vs FontResourceSizeExceeded, typically).
inline std::shared_ptr<const tgfx::Data> ReadLengthPrefixedBytes(::pag::DecodeStream* s,
                                                                 DiagnosticCollectorGuard* guard,
                                                                 size_t maxBytes,
                                                                 ErrorCode errorCode) {
  uint32_t n = ReadEncodedUint32Safe(s, guard);
  if (n > maxBytes || n > s->bytesAvailable()) {
    if (guard != nullptr) {
      guard->warn(errorCode, "bytes length exceeds limit or stream");
    }
    uint32_t skippable = std::min<uint32_t>(n, s->bytesAvailable());
    s->skip(skippable);
    return nullptr;
  }
  if (n == 0) {
    return tgfx::Data::MakeEmpty();
  }
  auto buf = std::make_unique<uint8_t[]>(n);
  ::pag::DecodeStream sub = s->readBytes(n);
  std::memcpy(buf.get(), sub.data(), n);
  // Hand the owning buffer to tgfx::Data::MakeAdopted with the default
  // DeleteProc — tgfx will call `delete[]` on the released pointer.
  return tgfx::Data::MakeAdopted(buf.release(), n, tgfx::Data::DeleteProc);
}

}  // namespace pagx::pag
