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

#include "GPUDecoder.h"
#include "VideoImage.h"
#include "tgfx/core/Buffer.h"

namespace pag {
GPUDecoder::GPUDecoder(const VideoFormat& format)
    : colorSpace(format.colorSpace), maxNumReorder(format.maxReorderSize) {
  isInitialized = initVideoToolBox(format.headers, format.mimeType);
}

GPUDecoder::~GPUDecoder() {
  if (session) {
    VTDecompressionSessionInvalidate(session);
    CFRelease(session);
    session = nullptr;
  }

  if (videoFormatDescription) {
    CFRelease(videoFormatDescription);
    videoFormatDescription = nullptr;
  }

  cleanResources();
}

void GPUDecoder::cleanResources() {
  if (outputFrame) {
    CVPixelBufferRelease(outputFrame->outputPixelBuffer);
    delete outputFrame;
    outputFrame = nullptr;
  }

  if (sampleBuffer) {
    CFRelease(sampleBuffer);
    sampleBuffer = nullptr;
  }

  if (blockBuffer) {
    CFRelease(blockBuffer);
    blockBuffer = nullptr;
  }

  for (auto& item : outputFrameCaches) {
    CVPixelBufferRelease(item.second->outputPixelBuffer);
    delete item.second;
  }
  pendingFrames.clear();
  outputFrameCaches.clear();
  inputEndOfStream = false;
}

void DidDecompress(void*, void* sourceFrameRefCon, OSStatus status, VTDecodeInfoFlags,
                   CVImageBufferRef pixelBuffer, CMTime, CMTime) {
  if (status == noErr && pixelBuffer) {
    CVPixelBufferRef* outputPixelBuffer = static_cast<CVPixelBufferRef*>(sourceFrameRefCon);
    *outputPixelBuffer = CVPixelBufferRetain(pixelBuffer);
  }
}

bool GPUDecoder::initVideoToolBox(const std::vector<std::shared_ptr<tgfx::Data>>& headers,
                                  const std::string& mimeType) {
  if (videoFormatDescription == nullptr) {
    OSStatus status;
    auto size = headers.size();

    std::vector<const uint8_t*> parameterSetPointers = {};
    std::vector<size_t> parameterSetSizes = {};
    for (const auto& header : headers) {
      parameterSetPointers.push_back(header->bytes() + 4);
      parameterSetSizes.push_back(header->size() - 4);
    }

    if (mimeType == "video/hevc") {
      status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(
          kCFAllocatorDefault, size, &parameterSetPointers[0], &parameterSetSizes[0], 4, NULL,
          &videoFormatDescription);

      if (status != noErr) {
        LOGE("GPUDecoder:format description create failed status = %d", status);
        return false;
      }
    } else {
      // create video format description
      status = CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault,
                                                                   size,  // param count
                                                                   &parameterSetPointers[0],
                                                                   &parameterSetSizes[0],
                                                                   4,  // nal start code size
                                                                   &videoFormatDescription);

      if (status != noErr) {
        LOGE("GPUDecoder:format description create failed status = %d", status);
        return false;
      }
    }
    return resetVideoToolBox();
  }
  return true;
}

bool GPUDecoder::resetVideoToolBox() {
  if (session) {
    VTDecompressionSessionInvalidate(session);
    CFRelease(session);
    session = nullptr;
  }

  // create decompression session
  CFDictionaryRef inAttrs = NULL;
  const void* keys[] = {kCVPixelBufferPixelFormatTypeKey, kCVPixelBufferOpenGLCompatibilityKey,
                        kCVPixelBufferIOSurfacePropertiesKey};

  uint32_t pixelFormatType = kCVPixelFormatType_32BGRA;
  uint32_t openGLCompatibility = true;

  CFNumberRef pixelFormatTypeValue = CFNumberCreate(NULL, kCFNumberSInt32Type, &pixelFormatType);
  CFNumberRef openGLCompatibilityValue =
      CFNumberCreate(NULL, kCFNumberSInt32Type, &openGLCompatibility);
  CFDictionaryRef ioSurfaceParam =
      CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0, NULL, NULL);

  const void* values[] = {pixelFormatTypeValue, openGLCompatibilityValue, ioSurfaceParam};
  inAttrs = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 3, NULL, NULL);
  const void* combineDics[] = {inAttrs};
  CFArrayRef combines = CFArrayCreate(NULL, combineDics, 1, NULL);
  CFDictionaryRef outAttrs = NULL;
  CVPixelBufferCreateResolvedAttributesDictionary(NULL, combines, &outAttrs);
  VTDecompressionOutputCallbackRecord callBackRecord;
  callBackRecord.decompressionOutputCallback = DidDecompress;
  callBackRecord.decompressionOutputRefCon = NULL;

  OSStatus status = VTDecompressionSessionCreate(kCFAllocatorDefault, videoFormatDescription, NULL,
                                                 outAttrs, &callBackRecord, &session);

  CFRelease(outAttrs);
  CFRelease(combines);
  CFRelease(inAttrs);
  CFRelease(pixelFormatTypeValue);
  CFRelease(openGLCompatibilityValue);
  CFRelease(ioSurfaceParam);

  if (colorSpace == tgfx::YUVColorSpace::Rec2020) {
    CFMutableDictionaryRef pixelTransferProperties = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(pixelTransferProperties,
                         kVTPixelTransferPropertyKey_DestinationColorPrimaries,
                         kCVImageBufferColorPrimaries_ITU_R_709_2);
    CFDictionarySetValue(pixelTransferProperties,
                         kVTPixelTransferPropertyKey_DestinationTransferFunction,
                         kCVImageBufferTransferFunction_ITU_R_709_2);
    CFDictionarySetValue(pixelTransferProperties,
                         kVTPixelTransferPropertyKey_DestinationYCbCrMatrix,
                         kCVImageBufferYCbCrMatrix_ITU_R_709_2);
    VTSessionSetProperty(session, kVTDecompressionPropertyKey_PixelTransferProperties,
                         pixelTransferProperties);
    CFRelease(pixelTransferProperties);
  }

  if (status != noErr || !session) {
    LOGE("GPUDecoder:decoder session create failed status = %d", status);
    return false;
  }

  return true;
}

pag::DecodingResult GPUDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  if (!bytes || length == 0) {
    return DecodingResult::Error;
  }

  // create block buffer
  OSStatus status;
  status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, bytes, length, kCFAllocatorNull,
                                              NULL, 0, length, 0, &blockBuffer);

  if (status != noErr) {
    LOGE("GPUDecoder:CMBlockBufferRef failed status=%d", (int)status);
    return DecodingResult::Error;
  }

  // create sample buffer
  const size_t sampleSizeArray[] = {length};
  if (@available(macOS 10.10, *)) {
    status = CMSampleBufferCreateReady(kCFAllocatorDefault, blockBuffer, videoFormatDescription, 1,
                                       0, NULL, 1, sampleSizeArray, &sampleBuffer);
  } else {
    LOGE("GPUDecoder: Error on sending bytes for decoding.\n");
    return DecodingResult::Error;
  }

  if (status != noErr) {
    LOGE("GPUDecoder:CMSampleBufferRef failed status=%d", (int)status);
    return DecodingResult::Error;
  }

  inputEndOfStream = false;
  sendFrameTime = time;
  pendingFrames.push_back(time);
  return DecodingResult::Success;
}

pag::DecodingResult GPUDecoder::onEndOfStream() {
  inputEndOfStream = true;
  return DecodingResult::Success;
}

pag::DecodingResult GPUDecoder::onDecodeFrame() {
  if (outputFrame) {
    CVPixelBufferRelease(outputFrame->outputPixelBuffer);
    delete outputFrame;
    outputFrame = nullptr;
  }

  CVPixelBufferRef outputPixelBuffer = nullptr;

  if (sampleBuffer) {
    OSStatus status = 0;
    VTDecodeInfoFlags infoFlags = 0;

    status =
        VTDecompressionSessionDecodeFrame(session, sampleBuffer, 0, &outputPixelBuffer, &infoFlags);

    if (status != noErr && !outputPixelBuffer) {
      LOGE("GPUDecoder:Decode failed status = %d", (int)status);
      if (status == kVTInvalidSessionErr) {
        cleanResources();
        isInitialized = resetVideoToolBox();
        LOGI("GPUDecoder:reset VideoToolBox, is initialized = %d", isInitialized);
      }
      LOGE("GPUDecoder: Error on decoding frame.\n");
      return DecodingResult::Error;
    }

    CFRelease(sampleBuffer);
    sampleBuffer = nullptr;
  }

  if (blockBuffer) {
    CFRelease(blockBuffer);
    blockBuffer = nullptr;
  }

  if (outputPixelBuffer) {
    auto newFrame = new OutputFrame();
    newFrame->frameTime = sendFrameTime;
    newFrame->outputPixelBuffer = outputPixelBuffer;
    outputPixelBuffer = nullptr;
    outputFrameCaches.insert(std::pair<pag::Frame, OutputFrame*>(sendFrameTime, newFrame));
  }

  if (!inputEndOfStream && pendingFrames.size() <= static_cast<size_t>(maxNumReorder)) {
    return DecodingResult::TryAgainLater;
  }

  if (!pendingFrames.empty()) {
    pendingFrames.sort();
    auto frame = *(pendingFrames.begin());
    pendingFrames.pop_front();
    auto iter = outputFrameCaches.find(frame);
    if (iter != outputFrameCaches.end()) {
      outputFrame = iter->second;
      outputFrameCaches.erase(iter);
      return DecodingResult::Success;
    } else {
      return DecodingResult::TryAgainLater;
    }
  } else if (inputEndOfStream) {
    return DecodingResult::EndOfStream;
  }

  return DecodingResult::TryAgainLater;
}

void GPUDecoder::onFlush() {
  cleanResources();
}

int64_t GPUDecoder::presentationTime() {
  if (outputFrame == nullptr) {
    return -1;
  }
  return outputFrame->frameTime;
}

std::shared_ptr<VideoBuffer> GPUDecoder::onRenderFrame() {
  if (outputFrame == nullptr) {
    return nullptr;
  }
  return VideoImage::MakeFrom(outputFrame->outputPixelBuffer);
}
}
