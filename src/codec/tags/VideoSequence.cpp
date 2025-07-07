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

#include "VideoSequence.h"
#include "codec/utils/NALUReader.h"

namespace pag {
VideoSequence* ReadVideoSequence(DecodeStream* stream, bool hasAlpha) {
  auto sequence = new VideoSequence();
  sequence->width = stream->readEncodedInt32();
  sequence->height = stream->readEncodedInt32();
  sequence->frameRate = stream->readFloat();

  if (hasAlpha) {
    sequence->alphaStartX = stream->readEncodedInt32();
    sequence->alphaStartY = stream->readEncodedInt32();
  }

  auto sps = ReadByteDataWithStartCode(stream);
  auto pps = ReadByteDataWithStartCode(stream);
  sequence->headers.push_back(sps.release());
  sequence->headers.push_back(pps.release());

  auto count = stream->readEncodedUint32();
  for (uint32_t i = 0; i < count; i++) {
    if (stream->context->hasException()) {
      return sequence;
    }
    auto videoFrame = new VideoFrame();
    sequence->frames.push_back(videoFrame);
    videoFrame->isKeyframe = stream->readBitBoolean();
  }
  for (uint32_t i = 0; i < count; i++) {
    if (stream->context->hasException()) {
      return sequence;
    }
    auto videoFrame = sequence->frames[i];
    videoFrame->frame = ReadTime(stream);
    videoFrame->fileBytes = ReadByteDataWithStartCode(stream).release();
  }

  if (stream->bytesAvailable() > 0) {
    count = stream->readEncodedUint32();
    for (uint32_t i = 0; i < count; i++) {
      if (stream->context->hasException()) {
        return sequence;
      }
      TimeRange staticTimeRange = {};
      staticTimeRange.start = ReadTime(stream);
      staticTimeRange.end = ReadTime(stream);
      sequence->staticTimeRanges.push_back(staticTimeRange);
    }
  }

  return sequence;
}

static void WriteByteDataWithoutStartCode(EncodeStream* stream, ByteData* byteData) {
  auto length = static_cast<uint32_t>(byteData->length());
  if (length < 4) {
    length = 0;
  } else {
    length -= 4;
  }
  stream->writeEncodedUint32(length);
  // Skip Annex B Prefix
  stream->writeBytes(byteData->data() + 4, length);
}

TagCode WriteVideoSequence(EncodeStream* stream, std::pair<VideoSequence*, bool>* parameter) {
  auto sequence = parameter->first;
  auto hasAlpha = parameter->second;
  stream->writeEncodedInt32(sequence->width);
  stream->writeEncodedInt32(sequence->height);
  stream->writeFloat(sequence->frameRate);

  if (hasAlpha) {
    stream->writeEncodedInt32(sequence->alphaStartX);
    stream->writeEncodedInt32(sequence->alphaStartY);
  }

  WriteByteDataWithoutStartCode(stream, sequence->headers[0]);  // sps
  WriteByteDataWithoutStartCode(stream, sequence->headers[1]);  // pps

  auto count = static_cast<uint32_t>(sequence->frames.size());
  stream->writeEncodedUint32(count);
  for (uint32_t i = 0; i < count; i++) {
    stream->writeBitBoolean(sequence->frames[i]->isKeyframe);
  }
  for (uint32_t i = 0; i < count; i++) {
    auto videoFrame = sequence->frames[i];
    WriteTime(stream, videoFrame->frame);
    WriteByteDataWithoutStartCode(stream, videoFrame->fileBytes);
  }

  stream->writeEncodedUint32(static_cast<uint32_t>(sequence->staticTimeRanges.size()));
  for (auto staticTimeRange : sequence->staticTimeRanges) {
    WriteTime(stream, staticTimeRange.start);
    WriteTime(stream, staticTimeRange.end);
  }

  return TagCode::VideoSequence;
}

ByteData* ReadMp4Header(DecodeStream* stream) {
  auto length = stream->readEncodedUint32();
  auto bytes = stream->readBytes(length);
  // must check whether the bytes is valid. otherwise memcpy will crash.
  if (length == 0 || length > bytes.length() || stream->context->hasException()) {
    return nullptr;
  }
  auto data = new (std::nothrow) uint8_t[length];
  if (data == nullptr) {
    return nullptr;
  }
  memcpy(data, bytes.data(), length);
  return ByteData::MakeAdopted(data, length).release();
}

TagCode WriteMp4Header(EncodeStream* stream, ByteData* byteData) {
  stream->writeEncodedUint32(static_cast<uint32_t>(byteData->length()));
  stream->writeBytes(byteData->data(), static_cast<uint32_t>(byteData->length()));
  return TagCode::Mp4Header;
}
}  // namespace pag
