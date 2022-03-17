/*
 * Copyright 2022 Tencent Inc. All Rights Reserved.
 *
 * Mp4BoxHelper.cpp
 *
 *  Created on: 2022/2/22
 *      Author: katacai
 */
#include "Mp4BoxHelper.h"
#include <chrono>
#include "H264Parser.h"
#include "H264Remuxer.h"
#include "codec/utils/ByteArray.h"

namespace pag {

std::unique_ptr<ByteData> Mp4BoxHelper::covertToMp4(VideoSequence* videoSequence) {
  int64_t nowTime = 0;
  if (!videoSequence->mp4Header) {
    nowTime = getCurrentTimeMicrosecond();
    std::unique_ptr<ByteData> mp4Data = createMp4(*videoSequence);
    LOGI("covertToMp4 without exist mp4 header, costTime:%lld", (getCurrentTimeMicrosecond() - nowTime));
    return mp4Data;
  }

  nowTime = getCurrentTimeMicrosecond();
  std::unique_ptr<ByteData> mp4Data = concatMp4(*videoSequence);
  LOGI("covertToMp4 with exist mp4 header, costTime:%lld", (getCurrentTimeMicrosecond() - nowTime));
  return mp4Data;
}

void Mp4BoxHelper::writeMp4Header(VideoSequence* videoSequence) {
  std::vector<ByteData*> nalus = H264Parser::getNaluFromSequence(*videoSequence);
  std::vector<H264Frame*> h264Frames = H264Parser::getH264Frames(nalus);
  H264Remuxer* remuxer = H264Remuxer::remux(h264Frames, *videoSequence);
  delete remuxer->getPayload();
  remuxer->writeMp4BoxesInSequence(*videoSequence);
}

std::unique_ptr<ByteData> Mp4BoxHelper::createMp4(VideoSequence& videoSequence) {
  std::vector<ByteData*> nalus = H264Parser::getNaluFromSequence(videoSequence);
  std::vector<H264Frame*> h264Frames = H264Parser::getH264Frames(nalus);
  H264Remuxer* remuxer = H264Remuxer::remux(h264Frames, videoSequence);
  return remuxer->convertMp4();
}

std::unique_ptr<ByteData> Mp4BoxHelper::concatMp4(VideoSequence& videoSequence) {
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

  ByteArray payload(nullptr, dataSize);
  payload.setOrder(ByteOrder::BigEndian);
  payload.writeBytes(videoSequence.mp4Header->data(), videoSequence.mp4Header->length());
  writeMdatBox(videoSequence, payload, mdatSize);

  return payload.release();
}

/**
 * 生成 mp4 中 mdat box 数据
 */
void Mp4BoxHelper::writeMdatBox(VideoSequence& sequence, ByteArray& payload, int mdatSize) {
  int splitSize = 4;

  payload.writeInt32(mdatSize);
  payload.writeUint8('m');
  payload.writeUint8('d');
  payload.writeUint8('a');
  payload.writeUint8('t');

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

int64_t Mp4BoxHelper::getCurrentTimeMillis() {
  std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch());
  return ms.count();
}

int64_t Mp4BoxHelper::getCurrentTimeMicrosecond() {
  static auto START_TIME = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - START_TIME);
  return static_cast<int64_t>(ns.count() * 1e-3);
}
}  // namespace pag
