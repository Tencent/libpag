/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "Mp4BoxHelper.h"
#include "H264Parser.h"
#include "H264Remuxer.h"
#include "SimpleArray.h"
#include "Utils.h"

namespace pag {

std::unique_ptr<ByteData> Mp4BoxHelper::CovertToMp4(VideoSequence* videoSequence) {
  tgfx::Clock clock;
  if (!videoSequence->mp4Header) {
    clock.mark("CreateMp4");
    std::unique_ptr<ByteData> mp4Data = CreateMp4(*videoSequence);
    LOGI("CovertToMp4 without exist mp4 header, costTime:%lld", clock.measure("", "CreateMp4"));
    return mp4Data;
  }

  clock.mark("ConcatMp4");
  std::unique_ptr<ByteData> mp4Data = ConcatMp4(*videoSequence);
  LOGI("CovertToMp4 with exist mp4 header, costTime:%lld", clock.measure("", "ConcatMp4"));
  return mp4Data;
}

void Mp4BoxHelper::WriteMp4Header(VideoSequence* videoSequence) {
  tgfx::Clock clock;
  clock.mark("WriteMp4Header");
  auto remuxer = H264Remuxer::Remux(*videoSequence);
  remuxer->writeMp4BoxesInSequence(*videoSequence);
  LOGI("write mp4 header, costTime:%lld", clock.measure("", "WriteMp4Header"));
}

std::unique_ptr<ByteData> Mp4BoxHelper::CreateMp4(VideoSequence& videoSequence) {
  tgfx::Clock clock;
  clock.mark("CreateMp4");
  auto remuxer = H264Remuxer::Remux(videoSequence);
  std::unique_ptr<ByteData> mp4Data = remuxer->convertMp4();
  LOGI("convertMp4, costTime:%lld", clock.measure("", "CreateMp4"));
  return mp4Data;
}

std::unique_ptr<ByteData> Mp4BoxHelper::ConcatMp4(VideoSequence& videoSequence) {
  int dataSize = videoSequence.mp4Header->length();
  int mdatSize = 0;
  for (auto header : videoSequence.headers) {
    int needSize = header->length();
    mdatSize += needSize;
  }
  for (auto frame : videoSequence.frames) {
    int needSize = frame->fileBytes->length();
    mdatSize += needSize;
  }
  mdatSize += 8;
  dataSize += mdatSize;

  SimpleArray payload(dataSize);
  payload.setOrder(ByteOrder::BigEndian);
  payload.writeBytes(videoSequence.mp4Header->data(), videoSequence.mp4Header->length());
  WriteMdatBox(videoSequence, payload, mdatSize);

  return payload.release();
}

void Mp4BoxHelper::WriteMdatBox(VideoSequence& sequence, SimpleArray& payload, int mdatSize) {
  payload.writeInt32(mdatSize);
  payload.writeUint8('m');
  payload.writeUint8('d');
  payload.writeUint8('a');
  payload.writeUint8('t');

  int splitSize = 4;
  for (auto header : sequence.headers) {
    int payLoadSize = header->length() - splitSize;
    payload.writeInt32(payLoadSize);
    payload.writeBytes(header->data(), payLoadSize, splitSize);
  }
  for (auto frame : sequence.frames) {
    int payLoadSize = frame->fileBytes->length() - splitSize;
    payload.writeInt32(payLoadSize);
    payload.writeBytes(frame->fileBytes->data(), payLoadSize, splitSize);
  }
}
}
