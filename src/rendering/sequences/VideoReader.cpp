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
static constexpr int64_t MAX_STALL_TIME_US = 500000;
static constexpr int64_t MAX_TOTAL_DECODE_TIME_US = 2000000;
static constexpr int FORCE_SOFTWARE_SIZE = 160000;  // 400x400

VideoReader::VideoReader(std::unique_ptr<VideoDemuxer> videoDemuxer)
    : VideoReader(std::move(videoDemuxer), Platform::Current()->getVideoDecoderFactories()) {
}

VideoReader::VideoReader(std::unique_ptr<VideoDemuxer> videoDemuxer,
                         std::vector<const VideoDecoderFactory*> decoderFactories)
    : demuxer(videoDemuxer.release()), decoderFactories(std::move(decoderFactories)) {
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
  auto deadline = tgfx::Clock::Now() + MAX_TOTAL_DECODE_TIME_US;
  // The deadline includes lock contention so each readBuffer() request has one shared wall-clock
  // budget. A single blocking platform codec call still cannot be interrupted by this soft budget.
  std::lock_guard<std::mutex> autoLock(locker);
  if (tgfx::Clock::Now() >= deadline) {
    return nullptr;
  }
  if (fallbackPending) {
    destroyVideoDecoder();
    factoryIndex++;
    fallbackPending = false;
  }
  auto targetTime = FrameToTime(targetFrame, frameRate);
  auto sampleTime = demuxer->getSampleTimeAt(targetTime);
  if (sampleTime == currentRenderedTime) {
    return lastBuffer;
  }
  lastBuffer = nullptr;
  currentRenderedTime = INT64_MIN;
  if (tgfx::Clock::Now() >= deadline || !checkVideoDecoder(deadline) ||
      tgfx::Clock::Now() >= deadline) {
    return nullptr;
  }
  auto status = decodeFrame(sampleTime, deadline);
  if (status == DecodeStatus::Error && tgfx::Clock::Now() < deadline) {
    resetParams();
    status = decodeFrame(sampleTime, deadline);
    if (status == DecodeStatus::Error && tgfx::Clock::Now() < deadline) {
      destroyVideoDecoder();
      factoryIndex++;
      if (tgfx::Clock::Now() < deadline && checkVideoDecoder(deadline) &&
          tgfx::Clock::Now() < deadline) {
        status = decodeFrame(sampleTime, deadline);
      }
    }
  }
  if (status == DecodeStatus::Stalled) {
    fallbackPending = true;
    return nullptr;
  }
  if (status != DecodeStatus::Success) {
    // A platform call may return an error only after crossing the deadline. Defer fallback to the
    // next request so this request still respects the shared decoding budget.
    if (tgfx::Clock::Now() >= deadline) {
      fallbackPending = true;
    }
    LOGE("VideoDecoder: Error on decoding frame.\n");
    return nullptr;
  }
  if (!outputEndOfStream) {
    if (tgfx::Clock::Now() >= deadline) {
      fallbackPending = true;
      return nullptr;
    }
    lastBuffer = videoDecoder->onRenderFrame();
    if (lastBuffer) {
      currentRenderedTime = currentDecodedTime;
    } else {
      fallbackPending = true;
    }
  }
  return lastBuffer;
}

void VideoReader::onReportPerformance(Performance* performance, int64_t decodingTime) {
  auto decoderType = lastDecoderType.load(std::memory_order_acquire);
  if (decoderType == DecoderType::Unknown) {
    return;
  }
  if (decoderType == DecoderType::Hardware) {
    performance->hardwareDecodingTime += decodingTime;
    performance->hardwareDecodingInitialTime += hardDecodingInitialTime.exchange(0);
  } else {
    performance->softwareDecodingTime += decodingTime;
    performance->softwareDecodingInitialTime += softDecodingInitialTime.exchange(0);
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

VideoReader::DecodeStatus VideoReader::decodeFrame(int64_t sampleTime, int64_t deadline) {
  if (demuxer->needSeeking(currentDecodedTime, sampleTime)) {
    resetParams();
    if (tgfx::Clock::Now() >= deadline) {
      return DecodeStatus::Stalled;
    }
    videoDecoder->onFlush();
    if (tgfx::Clock::Now() >= deadline) {
      return DecodeStatus::Stalled;
    }
    demuxer->seekTo(sampleTime);
  }
  auto lastProgressTime = tgfx::Clock::Now();
  int tryDecodeCount = 0;
  while (currentDecodedTime < sampleTime) {
    if (tgfx::Clock::Now() >= deadline) {
      return DecodeStatus::Stalled;
    }
    if (!sendSampleData()) {
      return DecodeStatus::Error;
    }
    if (tgfx::Clock::Now() >= deadline) {
      return DecodeStatus::Stalled;
    }
    auto result = videoDecoder->onDecodeFrame();
    if (result == DecodingResult::Error) {
      return DecodeStatus::Error;
    } else if (result == DecodingResult::Success) {
      tryDecodeCount = 0;
      lastProgressTime = tgfx::Clock::Now();
      currentDecodedTime = videoDecoder->presentationTime();
    } else if (result == DecodingResult::EndOfStream) {
      outputEndOfStream = true;
      return DecodeStatus::Success;
    } else if (result == DecodingResult::TryAgainLater) {
      auto currentTime = tgfx::Clock::Now();
      if (++tryDecodeCount >= MAX_TRY_DECODE_COUNT ||
          currentTime - lastProgressTime >= MAX_STALL_TIME_US || currentTime >= deadline) {
        LOGE("VideoDecoder: decoding frame stalled after %d attempts.\n", tryDecodeCount);
        return DecodeStatus::Stalled;
      }
    }
  }
  return DecodeStatus::Success;
}

bool VideoReader::checkVideoDecoder(int64_t deadline) {
  if (videoDecoder) {
    return true;
  }
  if (tgfx::Clock::Now() >= deadline) {
    return false;
  }
  videoDecoder = makeVideoDecoder(deadline).release();
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

std::unique_ptr<VideoDecoder> VideoReader::makeVideoDecoder(int64_t deadline) {
  while (factoryIndex < static_cast<int>(decoderFactories.size())) {
    if (tgfx::Clock::Now() >= deadline) {
      return nullptr;
    }
    auto factory = decoderFactories[factoryIndex];
    if (factory->isHardwareBacked() && preferSoftware) {
      factoryIndex++;
      continue;
    }
    tgfx::Clock clock = {};
    auto decoder = factory->createDecoder(demuxer->getFormat());
    if (decoder != nullptr) {
      if (decoder->isHardwareBacked()) {
        lastDecoderType.store(DecoderType::Hardware, std::memory_order_release);
        hardDecodingInitialTime = clock.elapsedTime();
      } else {
        lastDecoderType.store(DecoderType::Software, std::memory_order_release);
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
