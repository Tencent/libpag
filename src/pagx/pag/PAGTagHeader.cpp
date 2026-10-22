/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/pag/PAGTagHeader.h"

namespace pagx {
static constexpr uint32_t PAGMaxShortTagLength = 62;
static constexpr uint32_t PAGExtendedTagLength = 63;

void WritePAGTagHeader(pag::EncodeStream* stream, TagCode code, uint32_t length) {
  auto header = static_cast<uint16_t>(static_cast<uint16_t>(code) << 6);
  if (length <= PAGMaxShortTagLength) {
    stream->writeUint16(static_cast<uint16_t>(header | length));
  } else {
    stream->writeUint16(static_cast<uint16_t>(header | PAGExtendedTagLength));
    stream->writeUint32(length);
  }
}

PAGTagHeader ReadPAGTagHeader(pag::DecodeStream* stream) {
  PAGTagHeader header = {};
  auto codeAndLength = stream->readUint16();
  header.length = codeAndLength & 0x3F;
  header.code = static_cast<TagCode>(codeAndLength >> 6);
  if (header.length == PAGExtendedTagLength) {
    header.length = stream->readUint32();
  }
  return header;
}

void WritePAGStringTable(pag::EncodeStream* stream, const std::vector<std::string>& strings) {
  stream->writeEncodedUint32(static_cast<uint32_t>(strings.size()));
  for (const auto& text : strings) {
    stream->writeEncodedUint32(static_cast<uint32_t>(text.size()));
    stream->writeBytes(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(text.data())),
                       static_cast<uint32_t>(text.size()));
  }
}

bool ReadPAGStringTable(pag::DecodeStream* stream, std::vector<std::string>* strings) {
  auto count = stream->readEncodedUint32();
  strings->clear();
  strings->reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    auto length = stream->readEncodedUint32();
    auto bytes = stream->readBytes(length);
    if (bytes.context->hasException()) {
      return false;
    }
    strings->emplace_back(reinterpret_cast<const char*>(bytes.data()), static_cast<size_t>(length));
    if (bytes.context->hasException()) {
      return false;
    }
  }
  return !stream->context->hasException();
}
}  // namespace pagx
