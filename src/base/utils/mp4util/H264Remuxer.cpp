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

#include "H264Remuxer.h"
#include "codec/utils/DecodeStream.h"

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

std::unique_ptr<H264Remuxer> H264Remuxer::Remux(const VideoSequence* videoSequence) {
  if (videoSequence->headers.size() < 2) {
    LOGE("header data error in video sequence");
    return nullptr;
  }
  if (videoSequence->frames.empty()) {
    LOGE("no frame data in video sequence");
    return nullptr;
  }

  std::unique_ptr<H264Remuxer> remuxer = std::make_unique<H264Remuxer>();
  remuxer->videoSequence = videoSequence;
  remuxer->mp4Track.id = 1;  // track id
  remuxer->mp4Track.timescale = BASE_MEDIA_TIME_SCALE;
  remuxer->mp4Track.duration = static_cast<int32_t>(std::floor(
      (static_cast<float>(static_cast<int>(videoSequence->frames.size()) * BASE_MEDIA_TIME_SCALE) /
       videoSequence->frameRate)));
  remuxer->mp4Track.implicitOffset = static_cast<int32_t>(GetImplicitOffset(videoSequence->frames));

  auto spsData = videoSequence->headers.at(0);
  remuxer->mp4Track.width = videoSequence->getVideoWidth();
  remuxer->mp4Track.height = videoSequence->getVideoHeight();
  remuxer->mp4Track.sps = {spsData};
  remuxer->mp4Track.pps = {videoSequence->headers.at(1)};

  int headerLen = 0;
  for (auto header : videoSequence->headers) {
    headerLen += static_cast<int>(header->length());
  }

  auto sampleDelta = std::floor(remuxer->mp4Track.duration / videoSequence->frames.size());
  int count = 0;
  for (const auto* frame : videoSequence->frames) {
    int sampleSize = static_cast<int>(frame->fileBytes->length());
    if (count == 0) {
      sampleSize += headerLen;
    }
    remuxer->mp4Track.len += sampleSize;
    remuxer->mp4Track.pts.emplace_back(frame->frame);
    auto mp4Sample = std::make_shared<Mp4Sample>();
    mp4Sample->index = count;
    mp4Sample->size = sampleSize;
    mp4Sample->duration = static_cast<int32_t>(sampleDelta);
    mp4Sample->cts =
        static_cast<int32_t>(frame->frame + remuxer->mp4Track.implicitOffset - count) * sampleDelta;
    mp4Sample->flags.isKeyFrame = frame->isKeyframe;
    mp4Sample->flags.isNonSync = frame->isKeyframe ? 0 : 1;
    mp4Sample->flags.dependsOn = frame->isKeyframe ? 2 : 1;

    remuxer->mp4Track.samples.emplace_back(mp4Sample);
    count += 1;
  }
  return remuxer;
}

std::unique_ptr<ByteData> H264Remuxer::convertMp4() {
  int payLoadSize = getPayLoadSize();
  if (payLoadSize == 0) {
    return nullptr;
  }

  BoxParam boxParam;
  boxParam.tracks = {&mp4Track};
  boxParam.track = &mp4Track;
  boxParam.duration = mp4Track.duration;
  boxParam.timescale = mp4Track.timescale;
  boxParam.sequenceNumber = SEQUENCE_NUMBER;
  boxParam.baseMediaDecodeTime = BASE_MEDIA_DECODE_TIME;
  boxParam.nalusBytesLen = payLoadSize;
  boxParam.videoSequence = videoSequence;

  SimpleArray stream(static_cast<uint32_t>(payLoadSize * 1.5));
  Mp4Generator mp4Generator(boxParam);
  mp4Generator.ftyp(&stream, true);
  mp4Generator.moov(&stream, true);
  mp4Generator.moof(&stream, true);
  mp4Generator.mdat(&stream, true);

  return stream.release();
}

void H264Remuxer::writeMp4BoxesInSequence(VideoSequence* sequence) {
  int payLoadSize = getPayLoadSize();
  if (payLoadSize == 0) {
    return;
  }

  BoxParam boxParam;
  boxParam.tracks = {&mp4Track};
  boxParam.track = &mp4Track;
  boxParam.duration = mp4Track.duration;
  boxParam.timescale = mp4Track.timescale;
  boxParam.sequenceNumber = SEQUENCE_NUMBER;
  boxParam.baseMediaDecodeTime = BASE_MEDIA_DECODE_TIME;

  SimpleArray stream(static_cast<uint32_t>(payLoadSize * 0.5));
  Mp4Generator mp4Generator(boxParam);
  mp4Generator.ftyp(&stream, true);
  mp4Generator.moov(&stream, true);
  mp4Generator.moof(&stream, true);

  sequence->mp4Header = stream.release().release();
}

int H264Remuxer::getPayLoadSize() const {
  return mp4Track.len;
}
}  // namespace pag
