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

#pragma once
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pag/file.h"

namespace pag {

struct TagHeader {
  TagCode code;
  uint32_t length;
};

TagHeader ReadTagHeader(DecodeStream* stream);

template <typename T>
void ReadTags(DecodeStream* stream, T parameter, void (*reader)(DecodeStream*, TagCode, T)) {
  auto header = ReadTagHeader(stream);
  if (stream->context->hasException()) {
    return;
  }
  while (header.code != TagCode::End) {
    auto tagBytes = stream->readBytes(header.length);
    reader(&tagBytes, header.code, parameter);
    if (stream->context->hasException()) {
      return;
    }
    header = ReadTagHeader(stream);
    if (stream->context->hasException()) {
      return;
    }
  }
}

void WriteTagHeader(EncodeStream* stream, EncodeStream* tagBytes, TagCode code);

void WriteTagHeader(EncodeStream* stream, ByteData* tagBytes, TagCode code);

void WriteEndTag(EncodeStream* stream);

template <typename T>
void WriteTag(EncodeStream* stream, T parameter, TagCode (*writer)(EncodeStream*, T)) {
  EncodeStream bytes(stream->context);
  auto code = writer(&bytes, parameter);
  WriteTagHeader(stream, &bytes, code);
}
}  // namespace pag
