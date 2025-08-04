/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "TagHeader.h"
#include "codec/CodecContext.h"

namespace pag {
TagHeader ReadTagHeader(DecodeStream* stream) {
  auto codeAndLength = stream->readUint16();
  uint32_t length = codeAndLength & static_cast<uint8_t>(63);
  uint16_t code = codeAndLength >> 6;
  if (length == 63) {
    length = stream->readUint32();
  }
  auto context = static_cast<CodecContext*>(stream->context);
  if (context->tagLevel < code) {
    context->tagLevel = code;
  }
  TagHeader header = {static_cast<TagCode>(code), length};
  return header;
}

void WriteTypeAndLength(EncodeStream* stream, TagCode code, uint32_t length) {
  uint16_t typeAndLength = static_cast<uint16_t>(code) << 6;
  if (length < 63) {
    typeAndLength = typeAndLength | static_cast<uint8_t>(length);
    stream->writeUint16(typeAndLength);
  } else {
    typeAndLength = typeAndLength | static_cast<uint8_t>(63);
    stream->writeUint16(typeAndLength);
    stream->writeUint32(length);
  }
}

void WriteTagHeader(EncodeStream* stream, EncodeStream* tagBytes, TagCode code) {
  WriteTypeAndLength(stream, code, tagBytes->length());
  stream->writeBytes(tagBytes);
}

void WriteTagHeader(EncodeStream* stream, ByteData* tagBytes, TagCode code) {
  WriteTypeAndLength(stream, code, static_cast<uint32_t>(tagBytes->length()));
  stream->writeBytes(tagBytes->data(), static_cast<uint32_t>(tagBytes->length()));
}

void WriteEndTag(EncodeStream* stream) {
  stream->writeUint16(0);
}
}  // namespace pag
