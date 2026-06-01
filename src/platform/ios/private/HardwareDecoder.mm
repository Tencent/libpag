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

#include "HardwareDecoder.h"
#import <UIKit/UIKit.h>
#include <algorithm>
#include <mutex>
#include <vector>
#include "base/utils/Log.h"
#include "base/utils/USE.h"

namespace pag {

namespace {
std::mutex& GetActiveDecodersMutex() {
  static std::mutex m;
  return m;
}

std::vector<HardwareDecoder*>& GetActiveDecoders() {
  static std::vector<HardwareDecoder*> list;
  return list;
}

// Registers a global willResignActive observer once. The observer block runs on a dedicated
// NSOperationQueue (not the main thread), so it bypasses the synchronous-observer ordering
// that lets EAGLDevice::finish() acquire device.locker first and stall the main thread before
// HardwareDecoder gets a chance to abort its in-progress VT decode.
void RegisterWillResignObserverOnce() {
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    NSOperationQueue* queue = [[NSOperationQueue alloc] init];
    queue.maxConcurrentOperationCount = 1;
    queue.name = @"libpag.HardwareDecoder.willResign";
    [[NSNotificationCenter defaultCenter]
        addObserverForName:UIApplicationWillResignActiveNotification
                    object:nil
                     queue:queue
                usingBlock:^(NSNotification*) {
          std::lock_guard<std::mutex> lock(GetActiveDecodersMutex());
          for (auto* decoder : GetActiveDecoders()) {
            decoder->invalidateSession();
          }
        }];
  });
}
}  // namespace

void HardwareDecoder::invalidateSession() {
  // Retain a reference under the lock so concurrent resetVideoToolBox()/dealloc cannot release
  // the session pointer between our load and the VT call below. Then drop the lock before
  // calling into VT to avoid blocking the worker thread that may also touch the session.
  VTDecompressionSessionRef sessionToInvalidate = nullptr;
  {
    std::lock_guard<std::mutex> lock(sessionLocker);
    if (session) {
      sessionToInvalidate = static_cast<VTDecompressionSessionRef>(CFRetain(session));
    }
  }
  if (sessionToInvalidate) {
    VTDecompressionSessionInvalidate(sessionToInvalidate);
    CFRelease(sessionToInvalidate);
  }
}

HardwareDecoder::HardwareDecoder(const VideoFormat& format)
    : sourceColorSpace(format.colorSpace),
      destinationColorSpace(format.colorSpace),
      maxNumReorder(format.maxReorderSize) {
  isInitialized = initVideoToolBox(format.headers, format.mimeType);
  RegisterWillResignObserverOnce();
  std::lock_guard<std::mutex> lock(GetActiveDecodersMutex());
  GetActiveDecoders().push_back(this);
}

HardwareDecoder::~HardwareDecoder() {
  {
    std::lock_guard<std::mutex> lock(GetActiveDecodersMutex());
    auto& list = GetActiveDecoders();
    list.erase(std::remove(list.begin(), list.end(), this), list.end());
  }
  {
    std::lock_guard<std::mutex> lock(sessionLocker);
    if (session) {
      VTDecompressionSessionFinishDelayedFrames(session);
      VTDecompressionSessionInvalidate(session);
      CFRelease(session);
      session = nullptr;
    }
  }

  if (videoFormatDescription) {
    CFRelease(videoFormatDescription);
    videoFormatDescription = nullptr;
  }

  cleanResources();
}

void HardwareDecoder::cleanResources() {
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

bool HardwareDecoder::initVideoToolBox(const std::vector<std::shared_ptr<tgfx::Data>>& headers,
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
            kCFAllocatorDefault, size, parameterSetPointers.data(), parameterSetSizes.data(), 4,
            NULL, &videoFormatDescription);
      } else {
        status = -1;
      }
      if (status != noErr) {
        LOGE("HardwareDecoder:format description create failed status = %d", status);
        return false;
      }
    } else {
      // create video format description
      status = CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault,
                                                                   size,  // param count
                                                                   parameterSetPointers.data(),
                                                                   parameterSetSizes.data(),
                                                                   4,  // nal start code size
                                                                   &videoFormatDescription);
      if (status != noErr) {
        LOGE("HardwareDecoder:format description create failed status = %d", status);
        return false;
      }
    }
    return resetVideoToolBox();
  }
  return true;
}

bool HardwareDecoder::resetVideoToolBox() {
  // Release the old session under the lock so concurrent invalidateSession() never observes a
  // pointer that's mid-CFRelease. The new session is built into a local first and assigned to
  // the member only after creation succeeds, again under the lock.
  {
    std::lock_guard<std::mutex> lock(sessionLocker);
    if (session) {
      auto old = session;
      session = nullptr;
      VTDecompressionSessionFinishDelayedFrames(old);
      VTDecompressionSessionInvalidate(old);
      CFRelease(old);
    }
  }

  // create decompression session
  CFDictionaryRef attrs = NULL;
  const void* keys[] = {kCVPixelBufferPixelFormatTypeKey, kCVPixelBufferOpenGLESCompatibilityKey,
                        kCVPixelBufferIOSurfacePropertiesKey};

  uint32_t openGLESCompatibility = true;
  uint32_t pixelFormatType = tgfx::IsLimitedYUVColorRange(sourceColorSpace)
                                 ? kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                                 : kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;

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

  VTDecompressionSessionRef newSession = nullptr;
  OSStatus status;
  status = VTDecompressionSessionCreate(kCFAllocatorDefault, videoFormatDescription, NULL, attrs,
                                        &callBackRecord, &newSession);

  CFRelease(attrs);
  CFRelease(pixelFormatTypeValue);
  CFRelease(openGLESCompatibilityValue);
  CFRelease(ioSurfaceParam);

  if (@available(iOS 10.0, *)) {
    if (newSession != nullptr &&
        (sourceColorSpace == tgfx::YUVColorSpace::BT2020_LIMITED ||
         sourceColorSpace == tgfx::YUVColorSpace::BT2020_FULL)) {
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
      VTSessionSetProperty(newSession, pixelTransferProperties, pixelTransferPropertiesParam);

      CFRelease(destinationColorPrimaries);
      CFRelease(destinationTransferFunction);
      CFRelease(destinationYCbCrMatrix);
      CFRelease(pixelTransferProperties);
      if (tgfx::IsLimitedYUVColorRange(sourceColorSpace)) {
        destinationColorSpace = tgfx::YUVColorSpace::BT709_LIMITED;
      } else {
        destinationColorSpace = tgfx::YUVColorSpace::BT709_FULL;
      }
    }
  }

  if (status != noErr || !newSession) {
    LOGE("HardwareDecoder:decoder session create failed status = %d", status);
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(sessionLocker);
    session = newSession;
  }
  return true;
}

pag::DecodingResult HardwareDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  if (!bytes || length == 0) {
    LOGE("HardwareDecoder: Error on sending bytes for decoding.\n");
    return DecodingResult::Error;
  }

  // create block buffer
  OSStatus status;
  status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, bytes, length, kCFAllocatorNull,
                                              NULL, 0, length, 0, &blockBuffer);

  if (status != noErr) {
    LOGE("HardwareDecoder:CMBlockBufferRef failed status=%d", (int)status);
    return DecodingResult::Error;
  }

  // create sample buffer
  const size_t sampleSizeArray[] = {length};
  status = CMSampleBufferCreateReady(kCFAllocatorDefault, blockBuffer, videoFormatDescription, 1, 0,
                                     NULL, 1, sampleSizeArray, &sampleBuffer);

  if (status != noErr) {
    LOGE("HardwareDecoder:CMSampleBufferRef failed status=%d", (int)status);
    return DecodingResult::Error;
  }

  inputEndOfStream = false;
  sendFrameTime = time;
  pendingFrames.push_back(time);
  return DecodingResult::Success;
}

pag::DecodingResult HardwareDecoder::onEndOfStream() {
  inputEndOfStream = true;
  return DecodingResult::Success;
}

pag::DecodingResult HardwareDecoder::onDecodeFrame() {
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
        LOGI("HardwareDecoder: Resetting VideoToolBox session, which may be caused by app entered "
             "background, initialized = %d",
             isInitialized);
      } else {
        LOGE("HardwareDecoder:Decode failed status = %d", (int)status);
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

void HardwareDecoder::onFlush() {
  cleanResources();
}

int64_t HardwareDecoder::presentationTime() {
  if (outputFrame == nullptr) {
    return -1;
  }
  return outputFrame->frameTime;
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::onRenderFrame() {
  if (outputFrame == nullptr) {
    return nullptr;
  }
  return tgfx::ImageBuffer::MakeFrom(outputFrame->outputPixelBuffer, destinationColorSpace);
}
}  // namespace pag
