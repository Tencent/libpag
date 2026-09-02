/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "OHOSVideoDecoder.h"
#include <multimedia/player_framework/native_avbuffer.h>
#include <multimedia/player_framework/native_avcapability.h>
#include <multimedia/player_framework/native_avcodec_base.h>
#include <multimedia/player_framework/native_avformat.h>
#include <native_buffer/native_buffer.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include "base/utils/Log.h"
#include "rendering/video/SoftwareData.h"
#include "tgfx/core/ImageCodec.h"

namespace pag {
#define NV12_PLANE_COUNT 2

namespace {
// RAII wrapper for per-frame NV12 CPU buffers. The lifetime is owned by the SoftwareData
// attached to the ImageBuffer, so the memory stays alive until the asynchronous GPU upload
// task finishes, even if the decoder has been destroyed in the meantime.
struct NV12FrameBuffer {
  uint8_t* data[2] = {nullptr, nullptr};
  int lineSize[2] = {0, 0};

  NV12FrameBuffer() = default;

  ~NV12FrameBuffer() {
    delete[] data[0];
    delete[] data[1];
  }

  NV12FrameBuffer(const NV12FrameBuffer&) = delete;
  NV12FrameBuffer& operator=(const NV12FrameBuffer&) = delete;
};
}  // namespace

void OH_AVCodecOnError(OH_AVCodec*, int32_t errorCode, void* userData) {
  if (userData == nullptr) {
    return;
  }
  LOGE("video decoder error, errorCode:%d \n", errorCode);
  CodecUserData* codecUserData = static_cast<CodecUserData*>(userData);
  codecUserData->codecError = true;
  // Wake the input waiter as well. The input queue is untouched here, so without an explicit
  // error flag onSendBytes would keep waiting forever on a codec that will never deliver another
  // input buffer. Take the lock before notifying so the flag cannot be missed by a waiter that is
  // about to evaluate its predicate.
  { std::unique_lock<std::mutex> lock(codecUserData->inputMutex); }
  codecUserData->inputCondition.notify_all();
  std::unique_lock<std::mutex> lock(codecUserData->outputMutex);
  codecUserData->outputBufferInfoQueue.emplace(0, nullptr);
  codecUserData->outputCondition.notify_all();
}

void OH_AVCodecOnStreamChanged(OH_AVCodec*, OH_AVFormat*, void*) {
}

void OH_AVCodecOnNeedInputBuffer(OH_AVCodec*, uint32_t index, OH_AVBuffer* buffer, void* userData) {
  if (userData == nullptr) {
    return;
  }
  CodecUserData* codecUserData = static_cast<CodecUserData*>(userData);
  std::unique_lock<std::mutex> lock(codecUserData->inputMutex);
  codecUserData->inputBufferInfoQueue.emplace(index, buffer);
  codecUserData->inputCondition.notify_all();
}

void OH_AVCodecOnNewOutputBuffer(OH_AVCodec*, uint32_t index, OH_AVBuffer* buffer, void* userData) {
  if (userData == nullptr) {
    return;
  }
  CodecUserData* codecUserData = static_cast<CodecUserData*>(userData);
  std::unique_lock<std::mutex> lock(codecUserData->outputMutex);
  codecUserData->outputBufferInfoQueue.emplace(index, buffer);
  codecUserData->outputCondition.notify_all();
}

std::unique_ptr<OHOSVideoDecoder> OHOSVideoDecoder::MakeHardwareDecoder(const VideoFormat& format) {
  auto decoder = new OHOSVideoDecoder(format, true);
  if (!decoder->isValid) {
    delete decoder;
    return nullptr;
  }
  return std::unique_ptr<OHOSVideoDecoder>(decoder);
}

std::shared_ptr<OHOSVideoDecoder> OHOSVideoDecoder::MakeSoftwareDecoder(const VideoFormat& format) {
  auto decoder = std::shared_ptr<OHOSVideoDecoder>(new OHOSVideoDecoder(format, false));
  if (!decoder->isValid) {
    return nullptr;
  }
  return decoder;
}

OHOSVideoDecoder::OHOSVideoDecoder(const VideoFormat& format, bool hardware) {
  videoFormat = format;
  codecCategory = hardware ? HARDWARE : SOFTWARE;
  isValid = initDecoder(codecCategory);
}

OHOSVideoDecoder::~OHOSVideoDecoder() {
  if (videoCodec != nullptr) {
    releaseOutputBuffer();
    OH_VideoDecoder_Flush(videoCodec);
    OH_VideoDecoder_Stop(videoCodec);
    OH_VideoDecoder_Destroy(videoCodec);
    videoCodec = nullptr;
  }
  if (codecUserData) {
    codecUserData->clearQueue();
    delete codecUserData;
  }
}

bool OHOSVideoDecoder::initDecoder(const OH_AVCodecCategory avCodecCategory) {
  OH_AVCapability* capability =
      OH_AVCodec_GetCapabilityByCategory(OH_AVCODEC_MIMETYPE_VIDEO_AVC, false, avCodecCategory);
  const char* name = OH_AVCapability_GetName(capability);
  videoCodec = OH_VideoDecoder_CreateByName(name);
  if (videoCodec == nullptr) {
    LOGE("create video decoder failed!");
    return false;
  }

  codecUserData = new CodecUserData();
  OH_AVCodecCallback callback = {OH_AVCodecOnError, OH_AVCodecOnStreamChanged,
                                 OH_AVCodecOnNeedInputBuffer, OH_AVCodecOnNewOutputBuffer};
  int ret = OH_VideoDecoder_RegisterCallback(videoCodec, callback, codecUserData);
  if (ret != AV_ERR_OK) {
    LOGE("hardware decoder register callback failed!, ret:%d", ret);
    return false;
  }

  OH_AVFormat* ohFormat = OH_AVFormat_Create();
  OH_AVFormat_SetIntValue(ohFormat, OH_MD_KEY_WIDTH, videoFormat.width);
  OH_AVFormat_SetIntValue(ohFormat, OH_MD_KEY_HEIGHT, videoFormat.height);
  OH_AVFormat_SetIntValue(ohFormat, OH_MD_KEY_PIXEL_FORMAT, AV_PIXEL_FORMAT_NV12);
  ret = OH_VideoDecoder_Configure(videoCodec, ohFormat);
  OH_AVFormat_Destroy(ohFormat);
  if (ret != AV_ERR_OK) {
    LOGE("config video decoder failed!, ret:%d", ret);
    return false;
  }
  ret = OH_VideoDecoder_Prepare(videoCodec);
  if (ret != AV_ERR_OK) {
    LOGE("video decoder prepare failed!, ret:%d", ret);
    return false;
  }
  /**
   * According to HarmonyOS recommendations, temporarily modify the maximum number of hardware
   * decoding frames to 16 and the maximum number of software decoding frames to maxReorderSize + 1
   * to avoid stuttering on some HarmonyOS devices due to insufficient decoding data. However, this
   * modification will increase memory usage. Subsequent adjustments will be made once the
   * HarmonyOS system is improved.
   */
  maxPendingFramesCount =
      (codecCategory == HARDWARE) ? 16 : static_cast<size_t>(videoFormat.maxReorderSize) + 1;
  if (!start()) {
    LOGE("video decoder start failed!, ret:%d", ret);
    return false;
  }
  return true;
}

DecodingResult OHOSVideoDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  std::unique_lock<std::mutex> lock(codecUserData->inputMutex);
  codecUserData->inputCondition.wait(lock, [this]() {
    return codecUserData->inputBufferInfoQueue.size() > 0 || codecUserData->codecError;
  });
  if (codecUserData->codecError) {
    return DecodingResult::Error;
  }
  CodecBufferInfo codecBufferInfo = codecUserData->inputBufferInfoQueue.front();
  // Pop before use so a buffer that fails validation below is never left in the queue for the
  // next call to pick up again.
  codecUserData->inputBufferInfoQueue.pop();
  lock.unlock();

  if (codecBufferInfo.buffer == nullptr) {
    // The buffer was already popped above but cannot be returned to the codec, so mark the codec
    // as errored to stop further onSendBytes calls from draining and leaking the remaining input
    // buffers or blocking forever once the queue empties. onFlush resets this to recover.
    codecUserData->codecError = true;
    return DecodingResult::Error;
  }

  if (bytes != nullptr && length > 0) {
    uint8_t* bufferAddr = OH_AVBuffer_GetAddr(codecBufferInfo.buffer);
    int32_t capacity = OH_AVBuffer_GetCapacity(codecBufferInfo.buffer);
    // Guard the memcpy destination. On some HarmonyOS builds GetAddr can return null or report a
    // capacity smaller than the data when the codec is already in a bad state, which previously
    // crashed here with a SIGSEGV while writing the SPS/PPS headers during initialization.
    if (bufferAddr == nullptr || capacity < 0 || static_cast<size_t>(capacity) < length) {
      LOGE("onSendBytes: invalid input buffer, addr=%p, capacity=%d, length=%zu", bufferAddr,
           capacity, length);
      codecUserData->codecError = true;
      return DecodingResult::Error;
    }
    memcpy(bufferAddr, bytes, length);
  }

  OH_AVCodecBufferAttr bufferAttr;
  bufferAttr.size = length;
  bufferAttr.offset = 0;
  bufferAttr.pts = time >= 0 ? time : 0;
  bufferAttr.flags = length == 0 ? AVCODEC_BUFFER_FLAGS_EOS : AVCODEC_BUFFER_FLAGS_NONE;
  int ret = OH_AVBuffer_SetBufferAttr(codecBufferInfo.buffer, &bufferAttr);
  if (ret == AV_ERR_OK) {
    ret = OH_VideoDecoder_PushInputBuffer(videoCodec, codecBufferInfo.bufferIndex);
    if (ret != AV_ERR_OK) {
      LOGE("OH_VideoDecoder_PushInputBuffer failed, ret:%d", ret);
    }
  }

  if (ret != AV_ERR_OK) {
    // The buffer was popped but not accepted by the codec; treat the codec as errored so later
    // calls fail fast instead of leaking the remaining input buffers or blocking forever.
    codecUserData->codecError = true;
    return DecodingResult::Error;
  }
  if (length > 0 && time >= 0) {
    pendingFrames.push_back(time);
  }
  return DecodingResult::Success;
}

DecodingResult OHOSVideoDecoder::onEndOfStream() {
  return onSendBytes(nullptr, 0, 0);
}

DecodingResult OHOSVideoDecoder::onDecodeFrame() {
  releaseOutputBuffer();
  std::unique_lock<std::mutex> lock(codecUserData->outputMutex);
  codecUserData->outputCondition.wait(lock, [this]() {
    return codecUserData->outputBufferInfoQueue.size() > 0 ||
           pendingFrames.size() <= maxPendingFramesCount;
  });
  if (codecUserData->outputBufferInfoQueue.size() > 0) {
    codecBufferInfo = codecUserData->outputBufferInfoQueue.front();
    codecUserData->outputBufferInfoQueue.pop();
    lock.unlock();
    pendingFrames.remove(codecBufferInfo.attr.pts);
    if (codecBufferInfo.buffer == nullptr) {
      return DecodingResult::Error;
    }
  } else {
    lock.unlock();
    return DecodingResult::Success;
  }
  if (codecBufferInfo.attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
    return DecodingResult::EndOfStream;
  }
  return DecodingResult::Success;
}

void OHOSVideoDecoder::onFlush() {
  int ret = OH_VideoDecoder_Flush(videoCodec);
  if (ret != AV_ERR_OK) {
    return;
  }
  codecUserData->clearQueue();
  codecUserData->codecError = false;
  pendingFrames.clear();
  codecBufferInfo = {0, nullptr};
  if (!start()) {
    // start() failed on this recovery path. When OH_VideoDecoder_Start itself fails the codec is
    // not running, so OH_AVCodecOnNeedInputBuffer never fires and the next onSendBytes would block
    // forever on an empty input queue with codecError cleared above. Surface the failure as an
    // error instead so onSendBytes fast-fails and VideoReader can fall back to software decoding.
    codecUserData->codecError = true;
  }
}

bool OHOSVideoDecoder::start() {
  int ret = OH_VideoDecoder_Start(videoCodec);
  if (ret != AV_ERR_OK) {
    LOGE("video decoder start failed!, ret:%d", ret);
    return false;
  }
  for (auto& header : videoFormat.headers) {
    if (onSendBytes(const_cast<void*>(header->data()), header->size(), -1) !=
        DecodingResult::Success) {
      LOGE("video decoder send header failed!");
      return false;
    }
  }
  return true;
}

int64_t OHOSVideoDecoder::presentationTime() {
  if (codecBufferInfo.buffer) {
    return codecBufferInfo.attr.pts;
  }
  return -1;
}

std::shared_ptr<tgfx::ImageBuffer> OHOSVideoDecoder::onRenderFrame() {
  std::shared_ptr<tgfx::ImageBuffer> imageBuffer = nullptr;
  // TODO(kevingpqi): We temporarily disable texture generation through NativeBuffer, as enabling asynchronous hardware
  //  decoding will cause the jitter issue. We will re-enable it once HarmonyOS fixes this problem.
  if (false && codecCategory == HARDWARE) {
    OH_NativeBuffer* hardwareBuffer = OH_AVBuffer_GetNativeBuffer(codecBufferInfo.buffer);
    imageBuffer = tgfx::ImageBuffer::MakeFrom(hardwareBuffer, videoFormat.colorSpace);
    if (hardwareBuffer) {
      OH_NativeBuffer_Unreference(hardwareBuffer);
    }
  } else {
    if (videoStride == 0 || videoSliceHeight == 0) {
      OH_AVFormat* format = OH_VideoDecoder_GetOutputDescription(videoCodec);
      if (format) {
        OH_AVFormat_GetIntValue(format, OH_MD_KEY_VIDEO_STRIDE, &videoStride);
        OH_AVFormat_GetIntValue(format, OH_MD_KEY_VIDEO_SLICE_HEIGHT, &videoSliceHeight);
        OH_AVFormat_Destroy(format);
      }
      if (videoStride == 0 || videoSliceHeight == 0) {
        return nullptr;
      }
    }
    auto capacity = OH_AVBuffer_GetCapacity(codecBufferInfo.buffer);
    if (capacity <= 0) {
      return nullptr;
    }
    uint8_t* yuvAddress = OH_AVBuffer_GetAddr(codecBufferInfo.buffer);
    if (yuvAddress == nullptr) {
      return nullptr;
    }
    size_t yBufferSize = static_cast<size_t>(videoStride) * static_cast<size_t>(videoSliceHeight);
    size_t uvBufferSize = codecBufferInfo.attr.size > static_cast<int32_t>(yBufferSize)
                              ? static_cast<size_t>(codecBufferInfo.attr.size) - yBufferSize
                              : 0;
    if (uvBufferSize == 0) {
      return nullptr;
    }

    // Allocate a fresh frame buffer for every decoded frame. SoftwareData keeps a
    // shared_ptr to the NV12FrameBuffer, so the memory stays alive until the asynchronous
    // GPU upload task consumes it, even if the decoder is destroyed in the meantime.
    auto frameBuffer = std::make_shared<NV12FrameBuffer>();
    frameBuffer->data[0] = new (std::nothrow) uint8_t[yBufferSize];
    if (frameBuffer->data[0] == nullptr) {
      return nullptr;
    }
    frameBuffer->lineSize[0] = videoStride;
    frameBuffer->data[1] = new (std::nothrow) uint8_t[uvBufferSize];
    if (frameBuffer->data[1] == nullptr) {
      return nullptr;
    }
    frameBuffer->lineSize[1] = videoStride;

    memcpy(frameBuffer->data[0], yuvAddress, yBufferSize);
    memcpy(frameBuffer->data[1], yuvAddress + yBufferSize, uvBufferSize);

    uint8_t* planes[3] = {frameBuffer->data[0], frameBuffer->data[1], nullptr};
    int lineSizes[3] = {frameBuffer->lineSize[0], frameBuffer->lineSize[1], 0};
    auto yuvData =
        SoftwareData<NV12FrameBuffer>::Make(videoFormat.width, videoFormat.height, planes,
                                            lineSizes, NV12_PLANE_COUNT, std::move(frameBuffer));
    imageBuffer = tgfx::ImageBuffer::MakeNV12(yuvData, videoFormat.colorSpace);
  }
  return imageBuffer;
}

void OHOSVideoDecoder::releaseOutputBuffer() {
  if (codecBufferInfo.buffer) {
    int ret = OH_VideoDecoder_FreeOutputBuffer(videoCodec, codecBufferInfo.bufferIndex);
    if (ret != AV_ERR_OK) {
      LOGE("OH_VideoDecoder_FreeOutputBuffer failed, ret:%d", ret);
    }
    codecBufferInfo = {0, nullptr};
  }
}

}  // namespace pag
