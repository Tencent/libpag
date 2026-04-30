// Copyright (C) 2026 Tencent. All rights reserved.
#include "pagx/pag/TagHeader.h"

namespace pagx::pag {

TagHeader ReadTagHeader(::pag::DecodeStream* stream) {
  TagHeader header;
  uint16_t codeAndLength = stream->readUint16();
  uint32_t length = codeAndLength & 0x3Fu;  // low 6 bits
  uint16_t code = codeAndLength >> 6;       // high 10 bits
  if (length == 63) {
    length = stream->readUint32();
  }
  header.code = static_cast<TagCode>(code);
  header.length = length;
  return header;
}

static void WriteTypeAndLength(::pag::EncodeStream* stream, TagCode code, uint32_t length) {
  uint16_t typeAndLength = static_cast<uint16_t>(static_cast<uint16_t>(code) << 6);
  if (length < 63) {
    typeAndLength = static_cast<uint16_t>(typeAndLength | static_cast<uint16_t>(length));
    stream->writeUint16(typeAndLength);
  } else {
    typeAndLength = static_cast<uint16_t>(typeAndLength | 63u);
    stream->writeUint16(typeAndLength);
    stream->writeUint32(length);
  }
}

void WriteTagHeader(::pag::EncodeStream* stream, TagCode code, uint32_t length) {
  WriteTypeAndLength(stream, code, length);
}

void WriteTag(::pag::EncodeStream* stream, TagCode code, ::pag::EncodeStream* tagBytes) {
  WriteTypeAndLength(stream, code, tagBytes->length());
  stream->writeBytes(tagBytes);
}

void WriteEndTag(::pag::EncodeStream* stream) {
  WriteTypeAndLength(stream, TagCode::End, 0);
}

}  // namespace pagx::pag
