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

#include "VideoReader.h"
#include "base/utils/TimeUtil.h"
#include "tgfx/core/Clock.h"

namespace pag {
#define DECODER_TYPE_HARDWARE 1
#define DECODER_TYPE_SOFTWARE 2
#define DECODER_TYPE_FAIL 3
#define MAX_TRY_DECODE_COUNT 100
#define FORCE_SOFTWARE_SIZE 160000  // 400x400

class GPUDecoderTask : public Executor {
 public:
  static std::shared_ptr<Task> MakeAndRun(const VideoFormat& format) {
    auto task = Task::Make(std::unique_ptr<GPUDecoderTask>(new GPUDecoderTask(format)));
    task->run();
    return task;
  }

  std::unique_ptr<VideoDecoder> getDecoder() {
    return std::move(videoDecoder);
  }

 private:
  VideoFormat videoFormat = {};
  std::unique_ptr<VideoDecoder> videoDecoder = nullptr;

  explicit GPUDecoderTask(VideoFormat format) : videoFormat(std::move(format)) {
  }

  void execute() override {
    videoDecoder = VideoDecoder::Make(videoFormat, true);
  }
};

static Frame TotalFrames(VideoDemuxer* demuxer) {
  auto format = demuxer->getFormat();
  return TimeToFrame(format.duration, format.frameRate);
}

VideoReader::VideoReader(std::unique_ptr<VideoDemuxer> videoDemuxer)
    : SequenceReader(TotalFrames(videoDemuxer.get()), videoDemuxer->staticContent()),
      demuxer(videoDemuxer.release()) {
  auto videoFormat = demuxer->getFormat();
  frameRate = videoFormat.frameRate;
  decoderTypeIndex = DECODER_TYPE_HARDWARE;
  auto forceSoftware =
      (demuxer->staticContent() || videoFormat.width * videoFormat.height <= FORCE_SOFTWARE_SIZE);
  // Force using software decoders only when external decoders are available, because the built-in
  // libavc has poor performance.
  if (forceSoftware && VideoDecoder::HasExternalSoftwareDecoder()) {
    decoderTypeIndex = DECODER_TYPE_SOFTWARE;
  }
}

VideoReader::~VideoReader() {
  lastTask = nullptr;
  destroyVideoDecoder();
  delete demuxer;
}

bool VideoReader::decodeFrame(Frame targetFrame) {
  // Need a locker here in case there are other threads are decoding at the same time.
  std::lock_guard<std::mutex> autoLock(locker);
  auto targetTime = FrameToTime(targetFrame, frameRate);
  auto sampleTime = demuxer->getSampleTimeAt(targetTime);
  if (sampleTime == currentRenderedTime) {
    return true;
  }
  lastBuffer = nullptr;
  currentRenderedTime = INT64_MIN;
  if (!checkVideoDecoder()) {
    return false;
  }
  auto success = onDecodeFrame(sampleTime);
  if (!success) {
    // retry once.
    resetParams();
    success = onDecodeFrame(sampleTime);
    if (!success) {
      // fallback to software decoder.
      destroyVideoDecoder();
      decoderTypeIndex++;
      if (checkVideoDecoder()) {
        success = onDecodeFrame(sampleTime);
      }
    }
  }
  if (!success) {
    LOGE("VideoDecoder: Error on decoding frame.\n");
    return false;
  }
  if (!outputEndOfStream) {
    lastBuffer = videoDecoder->onRenderFrame();
    if (lastBuffer) {
      currentRenderedTime = currentDecodedTime;
    }
  }
  return lastBuffer != nullptr;
}

std::shared_ptr<tgfx::Texture> VideoReader::makeTexture(tgfx::Context* context) {
  if (lastBuffer == nullptr) {
    return nullptr;
  }
  return lastBuffer->makeTexture(context);
}

void VideoReader::recordPerformance(Performance* performance, int64_t decodingTime) {
  if (performance) {
    if (decoderTypeIndex == DECODER_TYPE_HARDWARE) {
      performance->hardwareDecodingTime += decodingTime;
      performance->hardwareDecodingInitialTime += hardDecodingInitialTime;
      hardDecodingInitialTime = 0;  // 只记录一次。
    } else {
      performance->softwareDecodingTime += decodingTime;
      performance->softwareDecodingInitialTime += softDecodingInitialTime;
      softDecodingInitialTime = 0;
    }
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

bool VideoReader::onDecodeFrame(int64_t sampleTime) {
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
  if (gpuDecoderTask && !gpuDecoderTask->isRunning()) {
    if (switchToGPUDecoderOfTask()) {
      return true;
    }
  }
  if (videoDecoder) {
    return true;
  }
  videoDecoder = makeVideoDecoder();
  if (videoDecoder) {
    return true;
  }

  if (gpuDecoderTask && switchToGPUDecoderOfTask()) {
    return true;
  }
  decoderTypeIndex = DECODER_TYPE_FAIL;
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

bool VideoReader::switchToGPUDecoderOfTask() {
  destroyVideoDecoder();
  auto executor = gpuDecoderTask->wait();
  videoDecoder = static_cast<GPUDecoderTask*>(executor)->getDecoder().release();
  gpuDecoderTask = nullptr;
  if (videoDecoder) {
    decoderTypeIndex = DECODER_TYPE_HARDWARE;
    return true;
  }
  return false;
}

VideoDecoder* VideoReader::makeVideoDecoder() {
  VideoDecoder* decoder = nullptr;
  if (decoderTypeIndex <= DECODER_TYPE_HARDWARE) {
    tgfx::Clock clock = {};
    // try hardware decoder.
    decoder = VideoDecoder::Make(demuxer->getFormat(), true).release();
    hardDecodingInitialTime = clock.measure();
    if (decoder) {
      decoderTypeIndex = DECODER_TYPE_HARDWARE;
      return decoder;
    }
  }
  if (decoderTypeIndex <= DECODER_TYPE_SOFTWARE) {
    tgfx::Clock clock = {};
    // try software decoder.
    decoder = VideoDecoder::Make(demuxer->getFormat(), false).release();
    softDecodingInitialTime = clock.measure();
    if (decoder) {
      decoderTypeIndex = DECODER_TYPE_SOFTWARE;
      return decoder;
    }
  }
  return decoder;
}
}  // namespace pag
