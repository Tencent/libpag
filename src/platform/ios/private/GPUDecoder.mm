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
#include "base/utils/USE.h"
#include "tgfx/core/YUVBuffer.h"

namespace pag {

GPUDecoder::GPUDecoder(const VideoFormat& format)
    : sourceColorSpace(format.colorSpace),
      destinationColorSpace(format.colorSpace),
      colorRange(format.colorRange),
      maxNumReorder(format.maxReorderSize) {
  isInitialized = initVideoToolBox(format.headers, format.mimeType);
}

GPUDecoder::~GPUDecoder() {
  if (session) {
    VTDecompressionSessionFinishDelayedFrames(session);
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
    int size = static_cast<int>(headers.size());
    std::vector<const uint8_t*> parameterSetPointers = {};
    std::vector<size_t> parameterSetSizes = {};
    for (const auto& header : headers) {
      parameterSetPointers.push_back(header->bytes() + 4);
      parameterSetSizes.push_back(header->size() - 4);
    }

    if (mimeType == "video/hevc") {
      if (@available(iOS 11.0, *)) {
        status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(
            kCFAllocatorDefault, size, &parameterSetPointers[0], &parameterSetSizes[0], 4, NULL,
            &videoFormatDescription);
      } else {
        status = -1;
      }
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
    VTDecompressionSessionFinishDelayedFrames(session);
    VTDecompressionSessionInvalidate(session);
    CFRelease(session);
    session = nullptr;
  }

  // create decompression session
  CFDictionaryRef attrs = NULL;
  const void* keys[] = {kCVPixelBufferPixelFormatTypeKey, kCVPixelBufferOpenGLESCompatibilityKey,
                        kCVPixelBufferIOSurfacePropertiesKey};

  uint32_t openGLESCompatibility = true;
  uint32_t pixelFormatType = colorRange == tgfx::YUVColorRange::JPEG
                                 ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                                 : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;

  CFNumberRef pixelFormatTypeValue = CFNumberCreate(NULL, kCFNumberSInt32Type, &pixelFormatType);
  CFNumberRef openGLESCompatibilityValue =
      CFNumberCreate(NULL, kCFNumberSInt32Type, &openGLESCompatibility);
  CFDictionaryRef ioSurfaceParam =
      CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0, NULL, NULL);

  const void* values[] = {pixelFormatTypeValue, openGLESCompatibilityValue, ioSurfaceParam};

  attrs = CFDictionaryCreate(NULL, keys, values, 3, NULL, NULL);

  VTDecompressionOutputCallbackRecord callBackRecord;
  callBackRecord.decompressionOutputCallback = DidDecompress;
  callBackRecord.decompressionOutputRefCon = NULL;

  OSStatus status;
  status = VTDecompressionSessionCreate(kCFAllocatorDefault, videoFormatDescription, NULL, attrs,
                                        &callBackRecord, &session);

  CFRelease(attrs);
  CFRelease(pixelFormatTypeValue);
  CFRelease(openGLESCompatibilityValue);
  CFRelease(ioSurfaceParam);

  if (@available(iOS 10.0, *)) {
    if (sourceColorSpace == tgfx::YUVColorSpace::Rec2020) {
      CFStringRef destinationColorPrimaries = CFStringCreateWithCString(
          kCFAllocatorDefault, "DestinationColorPrimaries", kCFStringEncodingUTF8);
      CFStringRef destinationTransferFunction = CFStringCreateWithCString(
          kCFAllocatorDefault, "DestinationTransferFunction", kCFStringEncodingUTF8);
      CFStringRef destinationYCbCrMatrix = CFStringCreateWithCString(
          kCFAllocatorDefault, "DestinationYCbCrMatrix", kCFStringEncodingUTF8);
      CFStringRef pixelTransferProperties = CFStringCreateWithCString(
          kCFAllocatorDefault, "PixelTransferProperties", kCFStringEncodingUTF8);

      CFMutableDictionaryRef pixelTransferPropertiesParam = CFDictionaryCreateMutable(
          kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
      CFDictionarySetValue(pixelTransferPropertiesParam, destinationColorPrimaries,
                           kCVImageBufferColorPrimaries_ITU_R_709_2);
      CFDictionarySetValue(pixelTransferPropertiesParam, destinationTransferFunction,
                           kCVImageBufferTransferFunction_ITU_R_709_2);
      CFDictionarySetValue(pixelTransferPropertiesParam, destinationYCbCrMatrix,
                           kCVImageBufferYCbCrMatrix_ITU_R_709_2);
      VTSessionSetProperty(session, pixelTransferProperties, pixelTransferPropertiesParam);

      CFRelease(destinationColorPrimaries);
      CFRelease(destinationTransferFunction);
      CFRelease(destinationYCbCrMatrix);
      CFRelease(pixelTransferProperties);

      destinationColorSpace = tgfx::YUVColorSpace::Rec709;
    }
  }

  if (status != noErr || !session) {
    LOGE("GPUDecoder:decoder session create failed status = %d", status);
    return false;
  }

  return true;
}

pag::DecodingResult GPUDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  if (!bytes || length == 0) {
    LOGE("GPUDecoder: Error on sending bytes for decoding.\n");
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
  status = CMSampleBufferCreateReady(kCFAllocatorDefault, blockBuffer, videoFormatDescription, 1, 0,
                                     NULL, 1, sampleSizeArray, &sampleBuffer);

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
      if (status == kVTInvalidSessionErr) {
        cleanResources();
        isInitialized = resetVideoToolBox();
        LOGI("GPUDecoder: Resetting VideoToolBox session, which may be caused by app entered "
             "background, initialized = %d",
             isInitialized);
      } else {
        LOGE("GPUDecoder:Decode failed status = %d", (int)status);
      }
      return DecodingResult::Error;
    }

    CFRelease(sampleBuffer);
    CFRelease(blockBuffer);
    sampleBuffer = nullptr;
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

std::shared_ptr<tgfx::ImageBuffer> GPUDecoder::onRenderFrame() {
  if (outputFrame == nullptr) {
    return nullptr;
  }
  return tgfx::YUVBuffer::MakeFrom(outputFrame->outputPixelBuffer, destinationColorSpace,
                                   colorRange);
}
}  // namespace pag
