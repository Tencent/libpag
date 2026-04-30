// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAG v2 TagHeader — same wire format as v1 (`src/codec/TagHeader.cpp`) but
// bound to the pagx::pag::TagCode enum and free of the CodecContext::tagLevel
// tracking that the v1 implementation coupled in. Design doc: §4.2.
//
// Wire layout:
//   u16 codeAndLength      // high 10 bits = TagCode, low 6 bits = length
//   [u32 extendedLength]   // present only when the low 6 bits equal 63
//   ... body bytes ...
#pragma once

#include <cstdint>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pagx/pag/TagCode.h"

namespace pagx::pag {

struct TagHeader {
  TagCode code = TagCode::End;
  uint32_t length = 0;
};

// Read a TagHeader from the stream. Returns {End, 0} when the stream context
// already has an exception flagged — callers should still check the context
// to distinguish "legit End tag" from "read failure".
TagHeader ReadTagHeader(::pag::DecodeStream* stream);

// Write a standalone TagHeader. When `length < 63` encodes a 2-byte header;
// otherwise emits the 6-byte form (2 bytes with low-6 = 63 + u32 length).
void WriteTagHeader(::pag::EncodeStream* stream, TagCode code, uint32_t length);

// Write a full tag: header followed by body bytes copied from `tagBytes`.
void WriteTag(::pag::EncodeStream* stream, TagCode code, ::pag::EncodeStream* tagBytes);

// Emit the terminator End tag (code=0, length=0).
void WriteEndTag(::pag::EncodeStream* stream);

}  // namespace pagx::pag
