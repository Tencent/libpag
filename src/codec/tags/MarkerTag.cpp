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

#include "MarkerTag.h"

namespace pag {

void ReadMarkerList(DecodeStream* stream, std::vector<Marker*>* markers) {
  std::vector<bool> flagList;

  auto count = stream->readEncodedUint32();
  for (size_t i = 0; i < count; i++) {
    if (stream->context->hasException()) {
      break;
    }
    auto hasDuration = stream->readBitBoolean();
    flagList.push_back(hasDuration);
  }

  for (auto hasDuration : flagList) {
    auto marker = new Marker();
    marker->startTime = ReadTime(stream);
    if (hasDuration) {
      marker->duration = ReadTime(stream);
    }
    marker->comment = stream->readUTF8String();
    markers->push_back(marker);
  }
}

TagCode WriteMarkerList(EncodeStream* stream, std::vector<Marker*>* markers) {
  stream->writeEncodedUint32(static_cast<uint32_t>(markers->size()));
  for (auto marker : *markers) {
    bool hasDuration = (marker->duration != 0);
    stream->writeBitBoolean(hasDuration);
  }

  for (auto marker : *markers) {
    bool hasDuration = (marker->duration != 0);

    WriteTime(stream, marker->startTime);
    if (hasDuration) {
      WriteTime(stream, marker->duration);
    }
    stream->writeUTF8String(marker->comment);
  }
  return TagCode::MarkerList;
}
}  // namespace pag
