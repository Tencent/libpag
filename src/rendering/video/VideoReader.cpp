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
#include "base/utils/GetTimer.h"

namespace pag {
#define DECODER_TYPE_HARDWARE 1
#define DECODER_TYPE_SOFTWARE 2
#define DECODER_TYPE_FAIL 3
#define MAX_TRY_DECODE_COUNT 100
#define FORCE_SOFTWARE_SIZE 160000  // 400x400

class GPUDecoderTask : public Executor {
 public:
  static std::shared_ptr<Task> MakeAndRun(const VideoConfig& config) {
    auto task = Task::Make(std::unique_ptr<GPUDecoderTask>(new GPUDecoderTask(config)));
    task->run();
    return task;
  }

  std::unique_ptr<VideoDecoder> getDecoder() {
    return std::move(videoDecoder);
  }

 private:
  VideoConfig videoConfig = {};
  std::unique_ptr<VideoDecoder> videoDecoder = nullptr;

  explicit GPUDecoderTask(VideoConfig config) : videoConfig(std::move(config)) {
  }

  void execute() override {
    videoDecoder = VideoDecoder::Make(videoConfig, true);
  }
};

VideoReader::VideoReader(VideoConfig config, std::unique_ptr<MediaDemuxer> demuxer,
                         DecodingPolicy policy)
    : videoConfig(std::move(config)), demuxer(demuxer.release()) {
  if (videoConfig.width * videoConfig.height <= FORCE_SOFTWARE_SIZE) {
    // force using software decoder if the size of video is less than 400x400 for better
    // performance。
    policy = DecodingPolicy::Software;
  }
  decoderTypeIndex = DECODER_TYPE_HARDWARE;
  switch (policy) {
    case DecodingPolicy::Software:
      // Force using software decoders only when external decoders are available, because the
      // built-in libavc has poor performance.
      if (VideoDecoder::HasExternalSoftwareDecoder()) {
        decoderTypeIndex = DECODER_TYPE_SOFTWARE;
      }
      break;
    case DecodingPolicy::SoftwareToHardware:
      if (VideoDecoder::SoftwareToHardwareEnabled() && VideoDecoder::HasHardwareDecoder()) {
        decoderTypeIndex = DECODER_TYPE_SOFTWARE;
        gpuDecoderTask = GPUDecoderTask::MakeAndRun(videoConfig);
      }
      break;
    default:
      break;
  }
}

VideoReader::~VideoReader() {
  destroyVideoDecoder();
  delete demuxer;
}

bool VideoReader::decodeFrame(int64_t sampleTime) {
  tryMakeVideoDecoder();
  if (videoDecoder == nullptr) {
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
      tryMakeVideoDecoder();
      if (videoDecoder) {
        success = onDecodeFrame(sampleTime);
      }
    }
  }
  if (!success) {
    LOGE("VideoDecoder: Error on decoding frame.\n");
  }
  return success;
}

int64_t VideoReader::getSampleTimeAt(int64_t targetTime) {
  std::lock_guard<std::mutex> autoLock(locker);
  return demuxer->getSampleTimeAt(targetTime);
}

int64_t VideoReader::getNextSampleTimeAt(int64_t targetTime) {
  std::lock_guard<std::mutex> autoLock(locker);
  return demuxer->getNextSampleTimeAt(targetTime);
}

void VideoReader::reportPerformance(Performance* performance, int64_t decodingTime) {
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

std::shared_ptr<VideoBuffer> VideoReader::readSample(int64_t targetTime) {
  // Need a locker here in case there are other threads are decoding at the same time.
  std::lock_guard<std::mutex> autoLock(locker);
  auto sampleTime = demuxer->getSampleTimeAt(targetTime);
  if (sampleTime == currentRenderedTime) {
    return outputBuffer;
  }

  if (!renderFrame(sampleTime)) {
    destroyVideoDecoder();
    decoderTypeIndex++;
    if (!renderFrame(sampleTime)) {
      return nullptr;
    }
  }

  return outputBuffer;
}

bool VideoReader::sendData() {
  if (inputEndOfStream) {
    return true;
  }
  if (needsAdvance) {
    demuxer->advance();
    needsAdvance = false;
  }
  auto data = demuxer->readSampleData();
  if (data.length <= 0) {
    auto result = videoDecoder->onEndOfStream();
    if (result == DecodingResult::Error) {
      return false;
    } else if (result == DecodingResult::Success) {
      inputEndOfStream = true;
    }
  } else {
    auto sampleTime = demuxer->getSampleTime();
    auto result = videoDecoder->onSendBytes(data.data, data.length, sampleTime);
    if (result == DecodingResult::Error) {
      LOGE("VideoReader: Error on sending bytes for decoding.\n");
      return false;
    } else if (result == DecodingResult::Success) {
      needsAdvance = true;
      return true;
    }
  }
  return true;
}

bool VideoReader::onDecodeFrame(int64_t sampleTime) {
  if (demuxer->trySeek(sampleTime, currentDecodedTime)) {
    resetParams();
    needsAdvance = true;
    videoDecoder->onFlush();
  }
  int tryDecodeCount = 0;
  while (currentDecodedTime < sampleTime) {
    if (!sendData()) {
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

void VideoReader::tryMakeVideoDecoder() {
  if (gpuDecoderTask && !gpuDecoderTask->isRunning()) {
    if (switchToGPUDecoderOfTask()) {
      return;
    }
  }
  if (videoDecoder) {
    return;
  }
  videoDecoder = makeDecoder();
  if (videoDecoder) {
    return;
  }

  if (gpuDecoderTask && switchToGPUDecoderOfTask()) {
    return;
  }
  decoderTypeIndex = DECODER_TYPE_FAIL;
}

void VideoReader::destroyVideoDecoder() {
  if (videoDecoder == nullptr) {
    return;
  }
  delete videoDecoder;
  videoDecoder = nullptr;
  outputBuffer = nullptr;
  currentRenderedTime = INT64_MIN;
  resetParams();
}

void VideoReader::resetParams() {
  currentDecodedTime = INT64_MIN;
  outputEndOfStream = false;
  needsAdvance = false;
  inputEndOfStream = false;
  demuxer->resetParams();
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

bool VideoReader::renderFrame(int64_t sampleTime) {
  if (sampleTime == currentDecodedTime) {
    return true;
  }
  if (!decodeFrame(sampleTime)) {
    outputBuffer = nullptr;
    currentRenderedTime = INT64_MIN;
    return false;
  }
  if (outputEndOfStream) {
    return true;
  }
  outputBuffer = videoDecoder->onRenderFrame();
  if (outputBuffer) {
    currentRenderedTime = currentDecodedTime;
  } else {
    currentRenderedTime = INT64_MIN;
  }
  return outputBuffer != nullptr;
}

VideoDecoder* VideoReader::makeDecoder() {
  VideoDecoder* decoder = nullptr;
  if (decoderTypeIndex <= DECODER_TYPE_HARDWARE) {
    int64_t initialStartTime = GetTimer();
    // try hardware decoder.
    decoder = VideoDecoder::Make(videoConfig, true).release();
    hardDecodingInitialTime = GetTimer() - initialStartTime;
    if (decoder) {
      decoderTypeIndex = DECODER_TYPE_HARDWARE;
      return decoder;
    }
  }
  if (decoderTypeIndex <= DECODER_TYPE_SOFTWARE) {
    int64_t initialStartTime = GetTimer();
    // try software decoder.
    decoder = VideoDecoder::Make(videoConfig, false).release();
    softDecodingInitialTime = GetTimer() - initialStartTime;
    if (decoder) {
      decoderTypeIndex = DECODER_TYPE_SOFTWARE;
      return decoder;
    }
  }
  return decoder;
}
}  // namespace pag
