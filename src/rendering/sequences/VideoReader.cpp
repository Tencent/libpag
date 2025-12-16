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

#include "VideoReader.h"
#include "base/utils/TimeUtil.h"
#include "platform/Platform.h"
#include "tgfx/core/Clock.h"
#ifdef PAG_BUILD_FOR_WEB
#include "platform/web/WebVideoSequenceDemuxer.h"
#endif

namespace pag {

static constexpr int MAX_TRY_DECODE_COUNT = 100;
static constexpr int FORCE_SOFTWARE_SIZE = 160000;  // 400x400

VideoReader::VideoReader(std::unique_ptr<VideoDemuxer> videoDemuxer)
    : demuxer(videoDemuxer.release()) {
  auto videoFormat = demuxer->getFormat();
  frameRate = videoFormat.frameRate;
  // Force using software decoders only when external decoders are available, because the built-in
  // libavc has poor performance.
  if ((demuxer->staticContent() || videoFormat.width * videoFormat.height <= FORCE_SOFTWARE_SIZE) &&
      VideoDecoderFactory::HasExternalSoftwareDecoder()) {
    preferSoftware = true;
  }
}

VideoReader::~VideoReader() {
  destroyVideoDecoder();
  delete demuxer;
}

std::shared_ptr<tgfx::ImageBuffer> VideoReader::onMakeBuffer(Frame targetFrame) {
  // Need a locker here in case there are other threads are decoding at the same time.
  std::lock_guard<std::mutex> autoLock(locker);
  auto targetTime = FrameToTime(targetFrame, frameRate);
  auto sampleTime = demuxer->getSampleTimeAt(targetTime);
  if (sampleTime == currentRenderedTime) {
    return lastBuffer;
  }
  lastBuffer = nullptr;
  currentRenderedTime = INT64_MIN;
  if (!checkVideoDecoder()) {
    return nullptr;
  }
  auto success = decodeFrame(sampleTime);
  if (!success) {
    // retry once.
    resetParams();
    success = decodeFrame(sampleTime);
    if (!success) {
      // fallback to software decoder.
      destroyVideoDecoder();
      factoryIndex++;
      if (checkVideoDecoder()) {
        success = decodeFrame(sampleTime);
      }
    }
  }
  if (!success) {
    LOGE("VideoDecoder: Error on decoding frame.\n");
    return nullptr;
  }
  if (!outputEndOfStream) {
    lastBuffer = videoDecoder->onRenderFrame();
    if (lastBuffer) {
      currentRenderedTime = currentDecodedTime;
    }
  }
  return lastBuffer;
}

void VideoReader::onReportPerformance(Performance* performance, int64_t decodingTime) {
  if (videoDecoder == nullptr) {
    return;
  }
  if (videoDecoder->isHardwareBacked()) {
    performance->hardwareDecodingTime += decodingTime;
    performance->hardwareDecodingInitialTime += hardDecodingInitialTime;
    hardDecodingInitialTime = 0;  // 只记录一次。
  } else {
    performance->softwareDecodingTime += decodingTime;
    performance->softwareDecodingInitialTime += softDecodingInitialTime;
    softDecodingInitialTime = 0;
  }
}

bool VideoReader::sendSampleData() {
  if (inputEndOfStream) {
    return true;
  }
  if (videoSample.length <= 0) {
    videoSample = demuxer->nextSample();
  }
  if (videoSample.length <= 0) {
    auto result = videoDecoder->onEndOfStream();
    if (result == DecodingResult::Error) {
      return false;
    } else if (result == DecodingResult::Success) {
      inputEndOfStream = true;
    }
  } else {
    auto result = videoDecoder->onSendBytes(videoSample.data, videoSample.length, videoSample.time);
    if (result == DecodingResult::Error) {
      LOGE("VideoReader: Error on sending bytes for decoding.\n");
      return false;
    } else if (result == DecodingResult::Success) {
      videoSample = {};
      return true;
    }
  }
  return true;
}

bool VideoReader::decodeFrame(int64_t sampleTime) {
  if (demuxer->needSeeking(currentDecodedTime, sampleTime)) {
    resetParams();
    videoDecoder->onFlush();
    demuxer->seekTo(sampleTime);
  }
  int tryDecodeCount = 0;
  while (currentDecodedTime < sampleTime) {
    if (!sendSampleData()) {
      return false;
    }
    auto result = videoDecoder->onDecodeFrame();
    if (result == DecodingResult::Error) {
      return false;
    } else if (result == DecodingResult::Success) {
      tryDecodeCount = 0;
      currentDecodedTime = videoDecoder->presentationTime();
    } else if (result == DecodingResult::EndOfStream) {
      outputEndOfStream = true;
      return true;
    } else if (result == DecodingResult::TryAgainLater) {
      if ((tryDecodeCount++) >= MAX_TRY_DECODE_COUNT) {
        LOGE("VideoDecoder: try decoding frame count reach limit %d.\n", MAX_TRY_DECODE_COUNT);
        return false;
      }
    }
  }
  return true;
}

bool VideoReader::checkVideoDecoder() {
  if (videoDecoder) {
    return true;
  }
  videoDecoder = makeVideoDecoder().release();
  if (videoDecoder) {
#ifdef PAG_BUILD_FOR_WEB
    auto tmpDemuxer = static_cast<WebVideoSequenceDemuxer*>(demuxer);
    tmpDemuxer->setForHardwareDecoder(videoDecoder->isHardwareBacked());
#endif
    return true;
  }
  return false;
}

void VideoReader::destroyVideoDecoder() {
  if (videoDecoder == nullptr) {
    return;
  }
  delete videoDecoder;
  videoDecoder = nullptr;
  lastBuffer = nullptr;
  currentRenderedTime = INT64_MIN;
  resetParams();
}

void VideoReader::resetParams() {
  currentDecodedTime = INT64_MIN;
  outputEndOfStream = false;
  inputEndOfStream = false;
  videoSample = {};
  demuxer->reset();
}

std::unique_ptr<VideoDecoder> VideoReader::makeVideoDecoder() {
  static const auto factories = Platform::Current()->getVideoDecoderFactories();
  while (factoryIndex < static_cast<int>(factories.size())) {
    auto factory = factories[factoryIndex];
    if (factory->isHardwareBacked() && preferSoftware) {
      factoryIndex++;
      continue;
    }
    tgfx::Clock clock = {};
    auto decoder = factory->createDecoder(demuxer->getFormat());
    if (decoder != nullptr) {
      if (decoder->isHardwareBacked()) {
        hardDecodingInitialTime = clock.elapsedTime();
      } else {
        softDecodingInitialTime = clock.elapsedTime();
      }
      return decoder;
    }
    factoryIndex++;
  }
  LOGE("VideoReader::makeVideoDecoder failure, reset factoryIndex");
  factoryIndex = 0;
  return nullptr;
}
}  // namespace pag
