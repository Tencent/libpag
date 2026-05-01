// Copyright (C) 2026 Tencent. All rights reserved.
//
// Property<T> + propHeader bit layout (design doc §4.3). Phase 1 lands the
// Constant-encoding path only — the rest of the encoding table (Hold /
// Linear / Bezier / Spatial) is reserved for the animation era.
//
// Wire layout per Property<T>:
//   [1 byte]  propHeader
//             bit 0-3 = encoding (0 = Constant; 1..15 reserved)
//             bit 4   = isDefault (1 → skip value bytes; Reader returns MakeProp(defaultValue))
//             bit 5-7 = reserved (Writer MUST write 0; Reader MUST ignore)
//   [value]   only when isDefault == 0 AND encoding == Constant
//
// The write side takes a defaultValue argument: when the field value equals
// the default, we flip the isDefault bit and skip the payload — typical
// static documents save ~60% on Property bytes.
//
// Read side also takes the defaultValue so that isDefault=1 can be resolved
// without the caller having to replay the initializer.
#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pagx/pag/PathCodec.h"
#include "pagx/pag/ValueCodec.h"
#include "tgfx/core/BlendMode.h"

namespace pagx::pag {

// propHeader bit masks — must match design doc §4.3 table exactly.
namespace prop_header {
constexpr uint8_t kEncodingMask = 0x0Fu;  // bits 0-3
constexpr uint8_t kIsDefaultBit = 0x10u;  // bit 4
constexpr uint8_t kReservedMask = 0xE0u;  // bits 5-7 (Writer must keep these zero)
constexpr uint8_t kEncodingConstant = 0x00u;
}  // namespace prop_header

enum class PropertyEncoding : uint8_t {
  Constant = 0,
  // 1..15 reserved for keyframe encodings (Hold / Linear / Bezier / Spatial).
};

// In-memory Property<T> container. Kept lean on purpose — no keyframe storage
// here in the static era (see §C.2 rationale). Animation era will add a typed
// keyframe field next to `value` without breaking the wire layout, since the
// wire branches on propHeader.encoding not on memory layout.
template <typename T>
struct Property {
  PropertyEncoding encoding = PropertyEncoding::Constant;
  T value = {};
};

template <typename T>
inline Property<T> MakeProp(T value) {
  return Property<T>{PropertyEncoding::Constant, std::move(value)};
}

// ------------------------------------------------------------------
// Equality helpers — drives the isDefault short-circuit on write.
// tgfx types all expose operator==; primitives use raw ==.
// ------------------------------------------------------------------

template <typename T>
inline bool PropertyValueEquals(const T& a, const T& b) {
  return a == b;
}

// ------------------------------------------------------------------
// WriteValue<T> / ReadValue<T> dispatch — narrow extension points used
// by WriteProperty / ReadProperty below. Specialized per T so the
// header stays closed to Baker-side editing.
// ------------------------------------------------------------------

template <typename T>
inline void WriteValue(::pag::EncodeStream* s, const T& v);

template <typename T>
inline T ReadValue(::pag::DecodeStream* s);

template <>
inline void WriteValue<bool>(::pag::EncodeStream* s, const bool& v) {
  WriteBool(s, v);
}
template <>
inline bool ReadValue<bool>(::pag::DecodeStream* s) {
  return ReadBool(s);
}

template <>
inline void WriteValue<uint8_t>(::pag::EncodeStream* s, const uint8_t& v) {
  WriteUint8(s, v);
}
template <>
inline uint8_t ReadValue<uint8_t>(::pag::DecodeStream* s) {
  return ReadUint8(s);
}

template <>
inline void WriteValue<uint16_t>(::pag::EncodeStream* s, const uint16_t& v) {
  WriteUint16(s, v);
}
template <>
inline uint16_t ReadValue<uint16_t>(::pag::DecodeStream* s) {
  return ReadUint16(s);
}

template <>
inline void WriteValue<uint32_t>(::pag::EncodeStream* s, const uint32_t& v) {
  WriteUint32(s, v);
}
template <>
inline uint32_t ReadValue<uint32_t>(::pag::DecodeStream* s) {
  return ReadUint32(s);
}

template <>
inline void WriteValue<int32_t>(::pag::EncodeStream* s, const int32_t& v) {
  WriteInt32(s, v);
}
template <>
inline int32_t ReadValue<int32_t>(::pag::DecodeStream* s) {
  return ReadInt32(s);
}

template <>
inline void WriteValue<float>(::pag::EncodeStream* s, const float& v) {
  WriteFloat(s, v);
}
template <>
inline float ReadValue<float>(::pag::DecodeStream* s) {
  return ReadFloat(s);
}

template <>
inline void WriteValue<tgfx::Color>(::pag::EncodeStream* s, const tgfx::Color& v) {
  WriteColor(s, v);
}
template <>
inline tgfx::Color ReadValue<tgfx::Color>(::pag::DecodeStream* s) {
  return ReadColor(s);
}

template <>
inline void WriteValue<tgfx::Point>(::pag::EncodeStream* s, const tgfx::Point& v) {
  WritePoint(s, v);
}
template <>
inline tgfx::Point ReadValue<tgfx::Point>(::pag::DecodeStream* s) {
  return ReadPoint(s);
}

template <>
inline void WriteValue<tgfx::Rect>(::pag::EncodeStream* s, const tgfx::Rect& v) {
  WriteRect(s, v);
}
template <>
inline tgfx::Rect ReadValue<tgfx::Rect>(::pag::DecodeStream* s) {
  return ReadRect(s);
}

template <>
inline void WriteValue<tgfx::Matrix>(::pag::EncodeStream* s, const tgfx::Matrix& v) {
  WriteMatrix(s, v);
}
template <>
inline tgfx::Matrix ReadValue<tgfx::Matrix>(::pag::DecodeStream* s) {
  return ReadMatrix(s);
}

template <>
inline void WriteValue<tgfx::Matrix3D>(::pag::EncodeStream* s, const tgfx::Matrix3D& v) {
  WriteMatrix3D(s, v);
}
template <>
inline tgfx::Matrix3D ReadValue<tgfx::Matrix3D>(::pag::DecodeStream* s) {
  return ReadMatrix3D(s);
}

// BlendMode — wire byte is the underlying enum value. Decoder side does not
// validate the range here; ReadEnum<BlendMode> in §G.5 (Phase 5+) will do
// that. Out-of-range values surface to the caller as an unmapped enum to be
// downgraded by the next layer.
template <>
inline void WriteValue<tgfx::BlendMode>(::pag::EncodeStream* s, const tgfx::BlendMode& v) {
  s->writeUint8(static_cast<uint8_t>(v));
}
template <>
inline tgfx::BlendMode ReadValue<tgfx::BlendMode>(::pag::DecodeStream* s) {
  return static_cast<tgfx::BlendMode>(s->readUint8());
}

// tgfx::Path — Phase 5b. Path's decode path needs to push fatal diagnostics
// (NaN/Inf coords, verb count overrun) so it cannot ride the basic ValueCodec
// pipeline. WritePath / ReadPath live in PathCodec.h. Wired through dedicated
// WriteProperty<Path> / ReadProperty<Path> specializations below so the
// caller-facing API stays uniform with the other Property<T> entries.
//
// We still provide a dummy ReadValue<Path> specialization that pushes a
// fatal: it should NEVER be called from the regular Property pipeline (the
// Property<Path> specializations bypass ReadValue<T>), but its presence
// prevents accidental unspecialised template instantiation from returning
// a nonsense Path.
template <>
inline void WriteValue<tgfx::Path>(::pag::EncodeStream* s, const tgfx::Path& v) {
  WritePath(s, v);
}
// No ReadValue<Path> specialization — Property<Path> uses the dedicated
// ReadProperty<Path> overload below, which threads DecodeContext through.

// ------------------------------------------------------------------
// WriteProperty<T> / ReadProperty<T> — the only API Baker/Codec touch.
// ------------------------------------------------------------------

template <typename T>
inline void WriteProperty(::pag::EncodeStream* stream, const Property<T>& prop,
                          const T& defaultValue) {
  uint8_t header = static_cast<uint8_t>(PropertyEncoding::Constant);
  const bool isDefault =
      prop.encoding == PropertyEncoding::Constant && PropertyValueEquals(prop.value, defaultValue);
  if (isDefault) {
    header = static_cast<uint8_t>(header | prop_header::kIsDefaultBit);
  }
  stream->writeUint8(header);
  if (!isDefault) {
    WriteValue<T>(stream, prop.value);
  }
}

template <typename T>
inline Property<T> ReadProperty(::pag::DecodeStream* stream, const T& defaultValue,
                                uint32_t enclosingTagEnd) {
  uint8_t header = stream->readUint8();
  uint8_t encoding = static_cast<uint8_t>(header & prop_header::kEncodingMask);
  bool isDefault = (header & prop_header::kIsDefaultBit) != 0;

  if (encoding == prop_header::kEncodingConstant) {
    if (isDefault) {
      return MakeProp(defaultValue);
    }
    return MakeProp(ReadValue<T>(stream));
  }
  // Unknown encoding — design doc §4.4 rule 1: skip the rest of the enclosing
  // Tag so the main Codec loop keeps alignment. Use skip() rather than
  // setPosition() because v1 setPosition semantics check `_position + value`
  // as an over-read guard, which would mis-fire here.
  if (enclosingTagEnd > stream->position()) {
    stream->skip(enclosingTagEnd - stream->position());
  }
  return MakeProp(defaultValue);
}

// ------------------------------------------------------------------
// Property<Path> dedicated overload — Path's decode path needs to push
// fatal diagnostics, so we route through DecodeContext directly instead of
// piggy-backing on the basic ReadValue<T> pipeline. Callers go through this
// overload by passing the ctx as the second arg (compile error makes the
// distinction explicit).
// ------------------------------------------------------------------

inline Property<tgfx::Path> ReadPathProperty(::pag::DecodeStream* stream, DecodeContext* ctx,
                                             const tgfx::Path& defaultValue,
                                             uint32_t enclosingTagEnd) {
  uint8_t header = stream->readUint8();
  uint8_t encoding = static_cast<uint8_t>(header & prop_header::kEncodingMask);
  bool isDefault = (header & prop_header::kIsDefaultBit) != 0;

  if (encoding == prop_header::kEncodingConstant) {
    if (isDefault) {
      return MakeProp(defaultValue);
    }
    tgfx::Path p = ReadPath(stream, ctx);
    if (ctx->hasError()) {
      return MakeProp(defaultValue);
    }
    return MakeProp(std::move(p));
  }
  // Unknown encoding — same §4.4 rule 1 skip-to-tag-end semantics.
  if (enclosingTagEnd > stream->position()) {
    stream->skip(enclosingTagEnd - stream->position());
  }
  return MakeProp(defaultValue);
}

inline void WritePathProperty(::pag::EncodeStream* stream, const Property<tgfx::Path>& prop,
                              const tgfx::Path& defaultValue) {
  uint8_t header = static_cast<uint8_t>(PropertyEncoding::Constant);
  // Path equality (operator==) is defined as "PathFillType + verb array +
  // Point array equivalent" per Path.h:43 — safe to use for the isDefault
  // collapse.
  const bool isDefault = prop.encoding == PropertyEncoding::Constant && prop.value == defaultValue;
  if (isDefault) {
    header = static_cast<uint8_t>(header | prop_header::kIsDefaultBit);
  }
  stream->writeUint8(header);
  if (!isDefault) {
    WritePath(stream, prop.value);
  }
}

}  // namespace pagx::pag
