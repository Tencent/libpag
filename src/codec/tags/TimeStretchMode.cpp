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

#include "TimeStretchMode.h"

namespace pag {
void ReadTimeStretchMode(DecodeStream* stream, CodecContext* context) {
  context->timeStretchMode = static_cast<PAGTimeStretchMode>(ReadUint8(stream));
  auto hasTimeRange = stream->readBoolean();
  if (hasTimeRange) {
    if (context->scaledTimeRange == nullptr) {
      context->scaledTimeRange = new TimeRange();
    }
    context->scaledTimeRange->start = ReadTime(stream);
    context->scaledTimeRange->end = ReadTime(stream);
  }
}

TagCode WriteTimeStretchMode(EncodeStream* stream, const File* file) {
  WriteUint8(stream, static_cast<uint8_t>(file->timeStretchMode));
  auto timeRange = file->scaledTimeRange;
  stream->writeBoolean(file->hasScaledTimeRange());
  if (file->hasScaledTimeRange()) {
    WriteTime(stream, timeRange.start);
    WriteTime(stream, timeRange.end);
  }
  return TagCode::TimeStretchMode;
}
}  // namespace pag
