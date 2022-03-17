/*
 * Copyright 2022 Tencent Inc. All Rights Reserved.
 *
 * Mp4BoxHelper.cpp
 *
 *  Created on: 2022/2/22
 *      Author: katacai
 */
#include "Mp4BoxHelper.h"
#include "H264Parser.h"
#include "H264Remuxer.h"
#include "SimpleArray.h"
#include "Utils.h"

namespace pag {

std::unique_ptr<ByteData> Mp4BoxHelper::CovertToMp4(VideoSequence* videoSequence) {
  int64_t nowTime = 0;
  if (!videoSequence->mp4Header) {
    nowTime = Utils::Utils::getCurrentTimeMicrosecond();
    std::unique_ptr<ByteData> mp4Data = CreateMp4(*videoSequence);
    LOGI("CovertToMp4 without exist mp4 header, costTime:%lld", (Utils::Utils::getCurrentTimeMicrosecond() - nowTime));
    return mp4Data;
  }

  nowTime = Utils::getCurrentTimeMicrosecond();
  std::unique_ptr<ByteData> mp4Data = ConcatMp4(*videoSequence);
  LOGI("CovertToMp4 with exist mp4 header, costTime:%lld", (Utils::getCurrentTimeMicrosecond() - nowTime));
  return mp4Data;
}

void Mp4BoxHelper::WriteMp4Header(VideoSequence* videoSequence) {
  int64_t nowTime = Utils::getCurrentTimeMicrosecond();
  auto remuxer = H264Remuxer::Remux(*videoSequence);
  remuxer->writeMp4BoxesInSequence(*videoSequence);
  LOGI("write mp4 header, costTime:%lld", (Utils::getCurrentTimeMicrosecond() - nowTime));
}

std::unique_ptr<ByteData> Mp4BoxHelper::CreateMp4(VideoSequence& videoSequence) {
  int64_t nowTime = Utils::getCurrentTimeMicrosecond();
  auto remuxer = H264Remuxer::Remux(videoSequence);
  std::unique_ptr<ByteData> mp4Data = remuxer->convertMp4();
  LOGI("convertMp4, costTime:%lld", (Utils::getCurrentTimeMicrosecond() - nowTime));
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
