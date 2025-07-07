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

#include "MP4BoxHelper.h"
#include "MP4Generator.h"
#include "base/utils/Log.h"
#include "codec/utils/EncodeStream.h"
#include "tgfx/core/Clock.h"

namespace pag {
static const int SEQUENCE_NUMBER = 1;
static const int BASE_MEDIA_DECODE_TIME = 0;
static const int BASE_MEDIA_TIME_SCALE = 6000;

static Frame GetImplicitOffset(const std::vector<VideoFrame*>& frames) {
  Frame index = 0;
  Frame maxOffset = 0;
  for (const auto* pts : frames) {
    Frame offset = index - pts->frame;
    if (offset > maxOffset) {
      maxOffset = offset;
    }
    index++;
  }
  return maxOffset;
}

static std::shared_ptr<MP4Track> MakeMP4Track(const VideoSequence* videoSequence) {
  if (videoSequence->headers.size() < 2) {
    LOGE("Bad header data in video sequence");
    return nullptr;
  }
  if (videoSequence->frames.empty()) {
    LOGE("There is no frame data in the video sequence");
    return nullptr;
  }

  auto mp4Track = std::make_shared<MP4Track>();

  mp4Track->id = 1;  // track id
  mp4Track->timescale = BASE_MEDIA_TIME_SCALE;
  mp4Track->duration = static_cast<int32_t>(std::floor(
      (static_cast<float>(static_cast<int>(videoSequence->frames.size()) * BASE_MEDIA_TIME_SCALE) /
       videoSequence->frameRate)));
  mp4Track->implicitOffset = static_cast<int32_t>(GetImplicitOffset(videoSequence->frames));

  auto spsData = videoSequence->headers.at(0);
  mp4Track->width = videoSequence->getVideoWidth();
  mp4Track->height = videoSequence->getVideoHeight();
  mp4Track->sps = {spsData};
  mp4Track->pps = {videoSequence->headers.at(1)};

  int headerLen = 0;
  for (auto header : videoSequence->headers) {
    headerLen += static_cast<int>(header->length());
  }

  auto sampleDelta = mp4Track->duration / static_cast<int32_t>(videoSequence->frames.size());
  int count = 0;
  for (const auto* frame : videoSequence->frames) {
    int sampleSize = static_cast<int>(frame->fileBytes->length());
    if (count == 0) {
      sampleSize += headerLen;
    }
    mp4Track->len += sampleSize;
    mp4Track->pts.emplace_back(frame->frame);
    auto mp4Sample = std::make_shared<MP4Sample>();
    mp4Sample->index = count;
    mp4Sample->size = sampleSize;
    mp4Sample->duration = sampleDelta;
    mp4Sample->cts = (static_cast<int32_t>(frame->frame) + mp4Track->implicitOffset -
                      static_cast<int32_t>(count)) *
                     sampleDelta;
    mp4Sample->flags.isKeyFrame = frame->isKeyframe;
    mp4Sample->flags.isNonSync = frame->isKeyframe ? 0 : 1;
    mp4Sample->flags.dependsOn = frame->isKeyframe ? 2 : 1;

    mp4Track->samples.emplace_back(mp4Sample);
    count += 1;
  }
  return mp4Track;
}

static void WriteMdatBox(const VideoSequence* videoSequence, EncodeStream* payload,
                         int32_t mdatSize) {
  payload->writeInt32(mdatSize);
  payload->writeUint8('m');
  payload->writeUint8('d');
  payload->writeUint8('a');
  payload->writeUint8('t');

  int32_t splitSize = 4;
  for (const auto* header : videoSequence->headers) {
    int32_t payLoadSize = static_cast<int32_t>(header->length()) - splitSize;
    payload->writeInt32(payLoadSize);
    payload->writeBytes(header->data(), payLoadSize, splitSize);
  }
  for (const auto* frame : videoSequence->frames) {
    int32_t payLoadSize = static_cast<int32_t>(frame->fileBytes->length()) - splitSize;
    payload->writeInt32(payLoadSize);
    payload->writeBytes(frame->fileBytes->data(), payLoadSize, splitSize);
  }
}

static std::unique_ptr<ByteData> ConcatMP4(const VideoSequence* videoSequence) {
  auto dataSize = static_cast<int32_t>(videoSequence->MP4Header->length());
  int32_t mdatSize = 0;
  for (auto header : videoSequence->headers) {
    auto needSize = static_cast<int32_t>(header->length());
    mdatSize += needSize;
  }
  for (auto frame : videoSequence->frames) {
    auto needSize = static_cast<int32_t>(frame->fileBytes->length());
    mdatSize += needSize;
  }
  mdatSize += 8;
  dataSize += mdatSize;

  EncodeStream payload(nullptr, static_cast<uint32_t>(dataSize));
  payload.setByteOrder(tgfx::ByteOrder::BigEndian);
  payload.writeBytes(videoSequence->MP4Header->data(),
                     static_cast<uint32_t>(videoSequence->MP4Header->length()));
  WriteMdatBox(videoSequence, &payload, mdatSize);

  return payload.release();
}

static std::unique_ptr<ByteData> MakeMP4Data(const VideoSequence* videoSequence, bool includeMdat) {
  auto mp4Track = MakeMP4Track(videoSequence);
  if (!mp4Track || mp4Track->len == 0) {
    return nullptr;
  }

  BoxParam boxParam;
  boxParam.tracks = {mp4Track};
  boxParam.track = mp4Track;
  boxParam.duration = mp4Track->duration;
  boxParam.timescale = mp4Track->timescale;
  boxParam.sequenceNumber = SEQUENCE_NUMBER;
  boxParam.baseMediaDecodeTime = BASE_MEDIA_DECODE_TIME;
  boxParam.nalusBytesLen = mp4Track->len;
  boxParam.videoSequence = videoSequence;

  float sizeFactor = includeMdat ? 1.5f : 0.5f;
  EncodeStream stream(nullptr,
                      static_cast<uint32_t>(static_cast<float>(mp4Track->len) * sizeFactor));
  stream.setByteOrder(tgfx::ByteOrder::BigEndian);
  MP4Generator mp4Generator(boxParam);
  mp4Generator.ftyp(&stream, true);
  mp4Generator.moov(&stream, true);
  mp4Generator.moof(&stream, true);
  if (includeMdat) {
    mp4Generator.mdat(&stream, true);
  }
  return stream.release();
}

std::unique_ptr<ByteData> MP4BoxHelper::CovertToMP4(const VideoSequence* videoSequence) {
  if (!videoSequence->MP4Header) {
    return MakeMP4Data(videoSequence, true);
  }
  return ConcatMP4(videoSequence);
}

void MP4BoxHelper::WriteMP4Header(VideoSequence* videoSequence) {
  videoSequence->MP4Header = MakeMP4Data(videoSequence, false).release();
}
}  // namespace pag
