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

#include "HardwareDecoder.h"
#include <multimedia/player_framework/native_avbuffer.h>
#include <multimedia/player_framework/native_avcapability.h>
#include <multimedia/player_framework/native_avcodec_base.h>
#include <multimedia/player_framework/native_avformat.h>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include "base/utils/Log.h"
#include "rendering/video/SoftwareData.h"
#include "tgfx/core/ImageCodec.h"

namespace pag {
#define NV12_PLANE_COUNT 2

void OH_AVCodecOnError(OH_AVCodec*, int32_t, void*) {
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
  LOGI("----------   OH_AVCodecOnNeedInputBuffer --------, size:%d", codecUserData->inputBufferInfoQueue.size());    
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

HardwareDecoder::HardwareDecoder(const VideoFormat& format) {
  videoFormat = format;
  isValid = initDecoder(codecCategory);
  if (!isValid) {
    codecCategory = SOFTWARE;
    LOGI("Fall back to the software decoder!");
    isValid = initDecoder(codecCategory);
  }
}

HardwareDecoder::~HardwareDecoder() {
  if (codecCategory == SOFTWARE) {
    if (yuvBuffer.data[0] != nullptr) {
      delete[] yuvBuffer.data[0];
      delete[] yuvBuffer.data[1];
    }
  }
  if (videoCodec != nullptr) {
    OH_VideoDecoder_Flush(videoCodec);
    OH_VideoDecoder_Stop(videoCodec);
    OH_VideoDecoder_Destroy(videoCodec);
    videoCodec = nullptr;
  }
  if (codecUserData) {
    delete codecUserData;
  }
}

bool HardwareDecoder::initDecoder(const OH_AVCodecCategory avCodecCategory) {
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
  if (!start()) {
    LOGE("video decoder start failed!, ret:%d", ret);
    return false;
  }
  return true;
}

DecodingResult HardwareDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  LOGI("codecUserData->inputBufferInfoQueue---onSendBytes  time:%d", time);  
  std::unique_lock<std::mutex> lock(codecUserData->inputMutex);
  codecUserData->inputCondition.wait(
      lock, [this]() { 
        LOGI("codecUserData->inputBufferInfoQueue---wait  size:%d", codecUserData->inputBufferInfoQueue.size());
        return codecUserData->inputBufferInfoQueue.size() > 0;
        });
  CodecBufferInfo codecBufferInfo = codecUserData->inputBufferInfoQueue.front();
  lock.unlock();

  OH_AVCodecBufferAttr bufferAttr;
  bufferAttr.size = length;
  bufferAttr.offset = 0;
  bufferAttr.pts = time >= 0 ? time : 0;
  bufferAttr.flags = length == 0 ? AVCODEC_BUFFER_FLAGS_EOS : AVCODEC_BUFFER_FLAGS_NONE;

  if (bytes != nullptr && length > 0) {
    memcpy(OH_AVBuffer_GetAddr(codecBufferInfo.buffer), bytes, length);
  }
  int ret = OH_AVBuffer_SetBufferAttr(codecBufferInfo.buffer, &bufferAttr);
  if (ret == AV_ERR_OK) {
    ret = OH_VideoDecoder_PushInputBuffer(videoCodec, codecBufferInfo.bufferIndex);
    if (ret != AV_ERR_OK) {
      LOGE("OH_VideoDecoder_PushInputBuffer failed, ret:%d", ret);
    }
  } else {
     LOGE("OH_AVBuffer_SetBufferAttr failed, ret:%d", ret);   
  }
  lock.lock();
  codecUserData->inputBufferInfoQueue.pop();
  lock.unlock();

  if (ret != AV_ERR_OK) {
    return DecodingResult::Error;
  }
  if (length > 0 && time >= 0) {
    pendingFrames.push_back(time);
  }
  LOGI("------ OH_VideoDecoder_PushInputBuffer success, pts:%d", time);  
  return DecodingResult::Success;
}

DecodingResult HardwareDecoder::onEndOfStream() {
  return onSendBytes(nullptr, 0, 0);
}

DecodingResult HardwareDecoder::onDecodeFrame() {
  std::unique_lock<std::mutex> lock(codecUserData->outputMutex);  
  codecUserData->outputCondition.wait(lock, [this]() {
    // In the PAG file, video sequence frame software decoding testing requires sending additional
    // data before obtaining the decoded data. If compatibility with user video decoding is desired,
    // retesting is necessary.       
    LOGI("------ HardwareDecoder::onDecodeFrame wait, pendingFrames.size:%d", pendingFrames.size());  
    return codecUserData->outputBufferInfoQueue.size() > 0 ||
           pendingFrames.size() <= static_cast<size_t>(videoFormat.maxReorderSize) + 1;    
//     return codecUserData->outputBufferInfoQueue.size() > 0 ||
//            pendingFrames.size() <=
//                (static_cast<size_t>(videoFormat.maxReorderSize) + codecCategory == SOFTWARE ? 1
//                                                                                             : 0);
  });
  if (codecUserData->outputBufferInfoQueue.size() > 0) {
    codecBufferInfo = codecUserData->outputBufferInfoQueue.front();
    codecUserData->outputBufferInfoQueue.pop();
    lock.unlock();
    pendingFrames.remove(codecBufferInfo.attr.pts);
    LOGI("------ HardwareDecoder::onDecodeFrame success, pts:%d", codecBufferInfo.attr.pts);      
  } else {
    lock.unlock();
     LOGI("------ HardwareDecoder::onDecodeFrame TryAgainLater");     
    return DecodingResult::TryAgainLater;
  }
  int ret = OH_VideoDecoder_FreeOutputBuffer(videoCodec, codecBufferInfo.bufferIndex);
    if (ret != AV_ERR_OK) {
      LOGE("OH_VideoDecoder_FreeOutputBuffer failed, ret:%d", ret);
    }    
  if (codecBufferInfo.attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
    return DecodingResult::EndOfStream;
  }
  return DecodingResult::Success;
}

void HardwareDecoder::onFlush() {
  int ret = OH_VideoDecoder_Flush(videoCodec);
  if (ret != AV_ERR_OK) {
    return;
  }
  codecUserData->clearQueue();
  pendingFrames.clear();
  start();
}

bool HardwareDecoder::start() {
  int ret = OH_VideoDecoder_Start(videoCodec);
  if (ret != AV_ERR_OK) {
    LOGE("video decoder start failed!, ret:%d", ret);
    return false;
  }
  for (auto& header : videoFormat.headers) {
    onSendBytes(const_cast<void*>(header->data()), header->size(), -1);
  }
  return true;
}

int64_t HardwareDecoder::presentationTime() {
  return codecBufferInfo.attr.pts;
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::onRenderFrame() {
  LOGI("------ HardwareDecoder::onRenderFrame");      
  std::shared_ptr<tgfx::ImageBuffer> imageBuffer = nullptr;
  if (codecCategory == HARDWARE) {
    imageBuffer = tgfx::ImageBuffer::MakeFrom(OH_AVBuffer_GetNativeBuffer(codecBufferInfo.buffer),
                                              videoFormat.colorSpace);
  } else {
    if (videoStride == 0) {
      OH_AVFormat* format = OH_VideoDecoder_GetOutputDescription(videoCodec);
      if (format) {
        OH_AVFormat_GetIntValue(format, OH_MD_KEY_VIDEO_STRIDE, &videoStride);
        OH_AVFormat_GetIntValue(format, OH_MD_KEY_VIDEO_SLICE_HEIGHT, &videoSliceHeight);
      } else {
        return nullptr;
      }
      yBufferSize = videoStride * videoSliceHeight;
      uvBufferSize = codecBufferInfo.attr.size - yBufferSize;
      yuvBuffer.data[0] = new uint8_t[yBufferSize];
      yuvBuffer.data[1] = new uint8_t[uvBufferSize];
      yuvBuffer.lineSize[0] = videoStride;
      yuvBuffer.lineSize[1] = videoStride;      
      OH_AVFormat_Destroy(format);
    }
    uint8_t* yuvAddress = OH_AVBuffer_GetAddr(codecBufferInfo.buffer);
    memcpy(yuvBuffer.data[0], yuvAddress, yBufferSize);
    memcpy(yuvBuffer.data[1], yuvAddress + yBufferSize, uvBufferSize);
    auto yuvData = SoftwareData<HardwareDecoder>::Make(
        videoFormat.width, videoFormat.height, yuvBuffer.data, yuvBuffer.lineSize, NV12_PLANE_COUNT,
        std::shared_ptr<HardwareDecoder>(this));
    imageBuffer = tgfx::ImageBuffer::MakeNV12(yuvData, videoFormat.colorSpace);
  }
//   int ret = OH_VideoDecoder_FreeOutputBuffer(videoCodec, codecBufferInfo.bufferIndex);
//   if (ret != AV_ERR_OK) {
//     LOGE("OH_VideoDecoder_FreeOutputBuffer failed, ret:%d", ret);
//   }
  return imageBuffer;
}

}  // namespace pag