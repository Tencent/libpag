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

#include "FontTables.h"

namespace pag {
void ReadFontTables(DecodeStream* stream) {
  auto count = stream->readEncodedUint32();
  auto context = static_cast<CodecContext*>(stream->context);
  auto fontIDMap = &(context->fontIDMap);
  for (uint32_t id = 0; id < count; id++) {
    if (stream->context->hasException()) {
      break;
    }
    auto fontData = new FontDescriptor();
    fontData->id = id;
    fontData->fontFamily = stream->readUTF8String();
    fontData->fontStyle = stream->readUTF8String();
    fontIDMap->insert(std::make_pair(id, fontData));
  }
}

TagCode WriteFontTables(EncodeStream* stream, std::vector<FontData>* fontList) {
  auto context = static_cast<CodecContext*>(stream->context);
  auto fontNameMap = &(context->fontNameMap);
  stream->writeEncodedUint32(static_cast<uint32_t>(fontList->size()));
  uint32_t id = 0;
  for (auto& font : *fontList) {
    stream->writeUTF8String(font.fontFamily);
    stream->writeUTF8String(font.fontStyle);
    auto fontData = new FontDescriptor();
    fontData->id = id++;
    fontData->fontFamily = font.fontFamily;
    fontData->fontStyle = font.fontStyle;
    fontNameMap->insert(std::make_pair(font.fontFamily + " - " + font.fontStyle, fontData));
  }
  return TagCode::FontTables;
}
}  // namespace pag
