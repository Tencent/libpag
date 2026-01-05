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

#include "BitmapSequence.h"

namespace pag {
BitmapSequence* ReadBitmapSequence(DecodeStream* stream) {
  auto sequence = new BitmapSequence();
  sequence->width = stream->readEncodedInt32();
  sequence->height = stream->readEncodedInt32();
  sequence->frameRate = stream->readFloat();
  auto count = stream->readEncodedUint32();
  for (uint32_t i = 0; i < count; i++) {
    if (stream->context->hasException()) {
      return sequence;
    }
    auto bitmapFrame = new BitmapFrame();
    sequence->frames.push_back(bitmapFrame);
    bitmapFrame->isKeyframe = stream->readBitBoolean();
  }
  for (uint32_t i = 0; i < count; i++) {
    auto bitmapFrame = sequence->frames[i];
    uint32_t bitmapCount = stream->readEncodedUint32();
    for (uint32_t j = 0; j < bitmapCount; j++) {
      if (stream->context->hasException()) {
        break;
      }
      auto bitmap = new BitmapRect();
      bitmapFrame->bitmaps.push_back(bitmap);
      bitmap->x = stream->readEncodedInt32();
      bitmap->y = stream->readEncodedInt32();
      bitmap->fileBytes = stream->readByteData().release();
    }
  }
  return sequence;
}

TagCode WriteBitmapSequence(EncodeStream* stream, BitmapSequence* sequence) {
  stream->writeEncodedInt32(sequence->width);
  stream->writeEncodedInt32(sequence->height);
  stream->writeFloat(sequence->frameRate);
  auto count = static_cast<uint32_t>(sequence->frames.size());
  stream->writeEncodedUint32(count);
  for (uint32_t i = 0; i < count; i++) {
    stream->writeBitBoolean(sequence->frames[i]->isKeyframe);
  }
  for (uint32_t i = 0; i < count; i++) {
    auto bitmapFrame = sequence->frames[i];
    uint32_t bitmapCount = 0;
    for (auto bitmap : bitmapFrame->bitmaps) {
      if (bitmap->fileBytes->length() == 0) {
        continue;
      }
      bitmapCount++;
    }
    stream->writeEncodedUint32(bitmapCount);
    for (auto bitmap : bitmapFrame->bitmaps) {
      auto fileBytes = bitmap->fileBytes;
      if (fileBytes->length() == 0) {
        continue;
      }
      stream->writeEncodedInt32(bitmap->x);
      stream->writeEncodedInt32(bitmap->y);
      stream->writeByteData(fileBytes);
    }
  }
  return TagCode::BitmapSequence;
}
}  // namespace pag
