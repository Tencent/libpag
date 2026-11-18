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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/pag/SequenceCodec.h"
#include <algorithm>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "codec/utils/StreamContext.h"

namespace pagx {

// The spec's per-container sanity cap: a container claiming more frames or headers than this is
// treated as malformed instead of being trusted to allocate.
static constexpr uint32_t MAX_SEQUENCE_ITEMS = 1u << 20;

static void WriteDataBlock(pag::EncodeStream* stream, const std::shared_ptr<Data>& data) {
  auto length = data != nullptr ? static_cast<uint32_t>(data->size()) : 0u;
  stream->writeEncodedUint32(length);
  if (length > 0) {
    stream->writeBytes(const_cast<uint8_t*>(data->bytes()), length);
  }
}

static std::shared_ptr<Data> ReadDataBlock(pag::DecodeStream* stream) {
  auto length = stream->readEncodedUint32();
  if (length == 0 || length > stream->bytesAvailable()) {
    return nullptr;
  }
  auto blockStream = stream->readBytes(length);
  return Data::MakeWithCopy(blockStream.data(), length);
}

std::shared_ptr<Data> SequenceCodec::EncodeVideoSequence(const VideoSequenceContainer& container) {
  pag::StreamContext context = {};
  pag::EncodeStream stream(&context);
  stream.writeFloat(container.frameRate);
  stream.writeUint32(container.width);
  stream.writeUint32(container.height);
  stream.writeEncodedUint32(static_cast<uint32_t>(container.frames.size()));
  stream.writeEncodedUint32(static_cast<uint32_t>(container.headers.size()));
  for (auto& header : container.headers) {
    WriteDataBlock(&stream, header);
  }
  stream.writeInt32(container.alphaStartX);
  stream.writeInt32(container.alphaStartY);
  for (auto& frame : container.frames) {
    uint8_t flags = frame.isKeyframe ? 1 : 0;
    stream.writeUint8(flags);
    stream.writeEncodedUint32(frame.frameIndex);
    WriteDataBlock(&stream, frame.data);
  }
  stream.writeEncodedUint32(static_cast<uint32_t>(container.staticRanges.size()));
  for (auto& range : container.staticRanges) {
    stream.writeEncodedUint32(range.start);
    stream.writeEncodedUint32(range.end);
  }
  auto bytes = stream.release();
  if (bytes == nullptr) {
    return nullptr;
  }
  return Data::MakeWithCopy(bytes->data(), bytes->length());
}

bool SequenceCodec::DecodeVideoSequence(const std::shared_ptr<Data>& data,
                                        VideoSequenceContainer* outContainer) {
  if (data == nullptr || outContainer == nullptr || data->empty()) {
    return false;
  }
  pag::StreamContext context = {};
  pag::DecodeStream stream(&context, data->bytes(), static_cast<uint32_t>(data->size()));
  outContainer->frameRate = stream.readFloat();
  outContainer->width = stream.readUint32();
  outContainer->height = stream.readUint32();
  auto frameCount = stream.readEncodedUint32();
  auto headerCount = stream.readEncodedUint32();
  if (frameCount > MAX_SEQUENCE_ITEMS || headerCount > MAX_SEQUENCE_ITEMS) {
    return false;
  }
  outContainer->headers.clear();
  for (uint32_t i = 0; i < headerCount; i++) {
    outContainer->headers.push_back(ReadDataBlock(&stream));
  }
  outContainer->alphaStartX = stream.readInt32();
  outContainer->alphaStartY = stream.readInt32();
  outContainer->frames.clear();
  outContainer->frames.reserve(frameCount);
  for (uint32_t i = 0; i < frameCount; i++) {
    VideoSequenceFrame frame = {};
    auto flags = stream.readUint8();
    frame.isKeyframe = (flags & 1) != 0;
    frame.frameIndex = stream.readEncodedUint32();
    frame.data = ReadDataBlock(&stream);
    if (frame.data == nullptr && frameCount > 0) {
      return false;
    }
    outContainer->frames.push_back(std::move(frame));
  }
  outContainer->staticRanges.clear();
  // The static range list trails the frame list; containers written before it existed end here.
  if (stream.bytesAvailable() > 0) {
    auto rangeCount = stream.readEncodedUint32();
    if (rangeCount > MAX_SEQUENCE_ITEMS) {
      return false;
    }
    for (uint32_t i = 0; i < rangeCount; i++) {
      SequenceStaticRange range = {};
      range.start = stream.readEncodedUint32();
      range.end = stream.readEncodedUint32();
      outContainer->staticRanges.push_back(range);
    }
  }
  return true;
}

std::shared_ptr<Data> SequenceCodec::EncodeBitmapSequence(
    const BitmapSequenceContainer& container) {
  pag::StreamContext context = {};
  pag::EncodeStream stream(&context);
  stream.writeFloat(container.frameRate);
  stream.writeUint32(container.width);
  stream.writeUint32(container.height);
  stream.writeEncodedUint32(static_cast<uint32_t>(container.frames.size()));
  for (auto& frame : container.frames) {
    uint8_t flags = frame.isKeyframe ? 1 : 0;
    stream.writeUint8(flags);
    stream.writeEncodedUint32(static_cast<uint32_t>(frame.bitmaps.size()));
    for (auto& block : frame.bitmaps) {
      stream.writeInt32(block.x);
      stream.writeInt32(block.y);
      stream.writeInt32(block.width);
      stream.writeInt32(block.height);
      WriteDataBlock(&stream, block.data);
    }
  }
  auto bytes = stream.release();
  if (bytes == nullptr) {
    return nullptr;
  }
  return Data::MakeWithCopy(bytes->data(), bytes->length());
}

bool SequenceCodec::DecodeBitmapSequence(const std::shared_ptr<Data>& data,
                                         BitmapSequenceContainer* outContainer) {
  if (data == nullptr || outContainer == nullptr || data->empty()) {
    return false;
  }
  pag::StreamContext context = {};
  pag::DecodeStream stream(&context, data->bytes(), static_cast<uint32_t>(data->size()));
  outContainer->frameRate = stream.readFloat();
  outContainer->width = stream.readUint32();
  outContainer->height = stream.readUint32();
  auto frameCount = stream.readEncodedUint32();
  if (frameCount > MAX_SEQUENCE_ITEMS) {
    return false;
  }
  outContainer->frames.clear();
  outContainer->frames.reserve(frameCount);
  for (uint32_t i = 0; i < frameCount; i++) {
    BitmapSequenceFrame frame = {};
    auto flags = stream.readUint8();
    frame.isKeyframe = (flags & 1) != 0;
    auto bitmapCount = stream.readEncodedUint32();
    if (bitmapCount > MAX_SEQUENCE_ITEMS) {
      return false;
    }
    frame.bitmaps.reserve(bitmapCount);
    for (uint32_t j = 0; j < bitmapCount; j++) {
      BitmapSequenceBlock block = {};
      block.x = stream.readInt32();
      block.y = stream.readInt32();
      block.width = stream.readInt32();
      block.height = stream.readInt32();
      block.data = ReadDataBlock(&stream);
      frame.bitmaps.push_back(std::move(block));
    }
    outContainer->frames.push_back(std::move(frame));
  }
  return true;
}

}  // namespace pagx
