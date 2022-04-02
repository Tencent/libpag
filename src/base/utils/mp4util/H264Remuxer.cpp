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
#include <memory>
#include "tgfx/include/core/Clock.h"

namespace pag {

std::unique_ptr<H264Remuxer> H264Remuxer::Remux(VideoSequence* videoSequence) {
  std::unique_ptr<H264Remuxer> remuxer = std::make_unique<H264Remuxer>();
  remuxer->videoSequence = videoSequence;
  remuxer->mp4Track.id = remuxer->getTrackID();
  remuxer->mp4Track.timescale = BASE_MEDIA_TIME_SCALE;
  remuxer->mp4Track.duration =
      std::floor((videoSequence->frames.size() / videoSequence->frameRate) * BASE_MEDIA_TIME_SCALE);
  remuxer->mp4Track.fps = videoSequence->frameRate;
  remuxer->mp4Track.implicitOffset = GetImplicitOffset(videoSequence->frames);
  if (videoSequence->headers.size() < 2) {
    LOGE("header data error in video sequence");
    return remuxer;
  }

  auto spsBytes = videoSequence->headers.at(0);
  SpsData spsData = H264Parser::ParseSPS(spsBytes);
  remuxer->mp4Track.width = spsData.width;
  remuxer->mp4Track.height = spsData.height;
  remuxer->mp4Track.sps = {spsData.sps};
  remuxer->mp4Track.codec = spsData.codec;
  auto ppsBytes = videoSequence->headers.at(1);
  remuxer->mp4Track.pps = {ppsBytes};

  if (videoSequence->frames.empty()) {
    LOGE("no frame data in video sequence");
    return remuxer;
  }

  int headerLen = 0;
  for (auto header : videoSequence->headers) {
    headerLen += header->length();
  }

  auto sampleDelta = std::floor(remuxer->mp4Track.duration / videoSequence->frames.size());
  int count = 0;
  for (auto frame : videoSequence->frames) {
    int sampleSize = frame->fileBytes->length();
    if (count == 0) {
      sampleSize += headerLen;
    }
    remuxer->mp4Track.len += sampleSize;
    remuxer->mp4Track.pts.emplace_back(frame->frame);
    auto mp4Sample = std::make_shared<Mp4Sample>();
    mp4Sample->index = count;
    mp4Sample->size = sampleSize;
    mp4Sample->duration = sampleDelta;
    mp4Sample->cts = (frame->frame + remuxer->mp4Track.implicitOffset - count) * sampleDelta;
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

  SimpleArray stream(payLoadSize * 1.5);
  Mp4Generator::Clear();
  Mp4Generator::InitParam(boxParam);
  Mp4Generator::FTYP(&stream, true);
  Mp4Generator::MOOV(&stream, true);
  Mp4Generator::MOOF(&stream, true);
  Mp4Generator::MDAT(&stream, true);

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
  Mp4Generator::Clear();
  Mp4Generator::InitParam(boxParam);
  Mp4Generator::FTYP(&stream, true);
  Mp4Generator::MOOV(&stream, true);
  Mp4Generator::MOOF(&stream, true);

  sequence->mp4Header = stream.release().release();
}

int H264Remuxer::getPayLoadSize() const {
  return mp4Track.len;
}

int H264Remuxer::getTrackID() {
  trackId++;
  return trackId;
}

int64_t H264Remuxer::GetImplicitOffset(std::vector<VideoFrame*>& frames) {
  int64_t index = 0;
  int64_t maxOffset = 0;
  for (auto pts : frames) {
    int64_t offset = index - pts->frame;
    if (offset > maxOffset) {
      maxOffset = offset;
    }
    index++;
  }
  return maxOffset;
}
}  // namespace pag
