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

#include "NALUReader.h"
#include "platform/Platform.h"

namespace pag {
std::unique_ptr<ByteData> ReadByteDataWithStartCode(DecodeStream* stream) {
  auto length = stream->readEncodedUint32();
  auto bytes = stream->readBytes(length);
  // must check whether the bytes is valid. otherwise memcpy will crash.
  if (length == 0 || length > bytes.length() || stream->context->hasException()) {
    return nullptr;
  }
  auto data = new (std::nothrow) uint8_t[length + 4];
  if (data == nullptr) {
    return nullptr;
  }
  memcpy(data + 4, bytes.data(), length);
  if (Platform::Current()->naluType() == NALUType::AVCC) {
    // AVCC
    data[0] = static_cast<uint8_t>((length >> 24) & 0xFF);
    data[1] = static_cast<uint8_t>((length >> 16) & 0x00FF);
    data[2] = static_cast<uint8_t>((length >> 8) & 0x0000FF);
    data[3] = static_cast<uint8_t>(length & 0x000000FF);
  } else {
    // Annex B Prefix
    data[0] = 0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 1;
  }
  return ByteData::MakeAdopted(data, length + 4);
}
}  // namespace pag
