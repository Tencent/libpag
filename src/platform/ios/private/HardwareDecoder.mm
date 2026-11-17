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

// Uses CVPixelBuffer OpenGLES compatibility keys, deprecated since iOS 12 along with the rest of
// the OpenGLES API. libpag's own build defines GLES_SILENCE_DEPRECATION (see CMakeLists); define
// it locally too so IDE indexing without the build flags stays clean.
#ifndef GLES_SILENCE_DEPRECATION
#define GLES_SILENCE_DEPRECATION 1
#endif
#include "HardwareDecoder.h"
#import <UIKit/UIKit.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>
#include "base/utils/Log.h"
#include "base/utils/USE.h"

namespace pag {

// A per-decoder liveness token held by shared_ptr. The decoder owns the only strong reference; the
// registry keeps weak references so the willResignActive observer can take a snapshot under
// GetActiveDecodersMutex, release it, and then invalidate each session off the mutex. mutex guards
// decoder, which the destructor clears before tearing the session down, so an observer either runs
// invalidateSession() on a live decoder or sees a null decoder and skips it, never racing
// destruction.
struct DecoderToken {
  std::mutex mutex = {};
  HardwareDecoder* decoder = nullptr;
};

namespace {
// VTDecompressionSessionInvalidate may crash inside VideoToolbox when it runs while
// VTDecompressionSessionDecodeFrame is still executing on the same session, so an in-flight decode
// gets this long to return before the session is torn down underneath it.
static constexpr int MAX_INVALIDATE_WAIT_TIME_MS = 200;

std::mutex& GetActiveDecodersMutex() {
  static std::mutex m;
  return m;
}

std::vector<std::weak_ptr<DecoderToken>>& GetActiveDecoders() {
  static std::vector<std::weak_ptr<DecoderToken>> list;
  return list;
}

// Reflects whether the application is currently inactive (between willResignActive and
// didBecomeActive). Read by onDecodeFrame() to fast-fail any decode attempt while in this
// window, since calling VTDecompressionSessionDecodeFrame may block indefinitely once the
// app moves to the background, defeating the invalidateSession() unblock done at willResign.
std::atomic<bool> appInactive{false};

// Registers global willResignActive / didBecomeActive observers once. The blocks run on a
// dedicated NSOperationQueue (not the main thread), so they bypass the synchronous-observer
// ordering that lets EAGLDevice::finish() acquire device.locker first and stall the main
// thread before HardwareDecoder gets a chance to abort its in-progress VT decode.
void RegisterAppLifecycleObserversOnce() {
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    NSOperationQueue* queue = [[NSOperationQueue alloc] init];
    queue.maxConcurrentOperationCount = 1;
    queue.name = @"libpag.HardwareDecoder.lifecycle";
    [[NSNotificationCenter defaultCenter]
        addObserverForName:UIApplicationWillResignActiveNotification
                    object:nil
                     queue:queue
                usingBlock:^(NSNotification*) {
                  appInactive.store(true, std::memory_order_release);
                  std::vector<std::shared_ptr<DecoderToken>> tokens;
                  {
                    std::lock_guard<std::mutex> lock(GetActiveDecodersMutex());
                    for (auto& weakToken : GetActiveDecoders()) {
                      auto token = weakToken.lock();
                      if (token != nullptr) {
                        tokens.push_back(std::move(token));
                      }
                    }
                  }
                  // One deadline for the whole batch so the total wait is capped at the budget
                  // rather than the budget times the number of decoders, and the invalidate runs
                  // off GetActiveDecodersMutex so it never blocks decoder construction or
                  // destruction.
                  auto deadline = std::chrono::steady_clock::now() +
                                  std::chrono::milliseconds(MAX_INVALIDATE_WAIT_TIME_MS);
                  for (auto& token : tokens) {
                    std::lock_guard<std::mutex> lock(token->mutex);
                    if (token->decoder != nullptr) {
                      token->decoder->invalidateSession(deadline);
                    }
                  }
                }];
    [[NSNotificationCenter defaultCenter]
        addObserverForName:UIApplicationDidBecomeActiveNotification
                    object:nil
                     queue:queue
                usingBlock:^(NSNotification*) {
                  appInactive.store(false, std::memory_order_release);
                }];
  });
}
}  // namespace

void HardwareDecoder::invalidateSession(std::chrono::steady_clock::time_point deadline) {
  // VideoToolbox tears down the decoder state that VTDecompressionSessionDecodeFrame is still
  // executing inside, which crashes inside the decoder plugin. Give a running decode until the
  // shared deadline to return first, and only invalidate underneath it once it is really stuck,
  // because unblocking a stuck decode is what this method exists for.
  const bool hasDecodeLocker = decodeLocker.try_lock_until(deadline);
  // sessionLocker is only ever held briefly now that teardown uses the non-blocking
  // FinishDelayedFrames, but bound it with the same deadline so the observer's whole batch stays
  // capped rather than blocking here if another thread is mid session swap.
  if (sessionLocker.try_lock_until(deadline)) {
    if (session) {
      VTDecompressionSessionInvalidate(session);
    }
    sessionLocker.unlock();
  } else {
    LOGI("HardwareDecoder: skipped invalidate because the session stayed busy past the deadline");
  }
  if (hasDecodeLocker) {
    decodeLocker.unlock();
  }
}

HardwareDecoder::HardwareDecoder(const VideoFormat& format)
    : sourceColorSpace(format.colorSpace),
      destinationColorSpace(format.colorSpace),
      maxNumReorder(format.maxReorderSize) {
  isInitialized = initVideoToolBox(format.headers, format.mimeType);
  lifecycleToken = std::make_shared<DecoderToken>();
  lifecycleToken->decoder = this;
  RegisterAppLifecycleObserversOnce();
  std::lock_guard<std::mutex> lock(GetActiveDecodersMutex());
  auto& list = GetActiveDecoders();
  for (auto item = list.begin(); item != list.end();) {
    if (item->expired()) {
      item = list.erase(item);
    } else {
      ++item;
    }
  }
  list.push_back(lifecycleToken);
}

HardwareDecoder::~HardwareDecoder() {
  {
    // Stop the observer from reaching this decoder before the session is torn down. A blocking
    // invalidateSession() already in progress finishes here while this holds the token mutex.
    std::lock_guard<std::mutex> lock(lifecycleToken->mutex);
    lifecycleToken->decoder = nullptr;
  }
  {
    std::lock_guard<std::mutex> lock(GetActiveDecodersMutex());
    auto& list = GetActiveDecoders();
    for (auto item = list.begin(); item != list.end();) {
      auto token = item->lock();
      if (token == nullptr || token == lifecycleToken) {
        item = list.erase(item);
      } else {
        ++item;
      }
    }
  }
  {
    std::lock_guard<std::timed_mutex> lock(sessionLocker);
    if (session) {
      // Non-blocking: asks VideoToolbox to emit any delayed frame but may return before it does,
      // so this never stalls the ANR-sensitive teardown path the way a wait-for-frames barrier
      // would. A still-pending context is handed to the graveyard below instead of waited on.
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
  // Resolved contexts are deleted; a context still pending (a bounded late callback not yet landed)
  // moves to the graveyard so the callback has a live object to write into.
  contextPool.drain();
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
      // CMVideoFormatDescriptionCreateFromHEVCParameterSets needs iOS 11.0, well below the 15.0
      // floor.
      status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(
          kCFAllocatorDefault, size, parameterSetPointers.data(), parameterSetSizes.data(), 4, NULL,
          &videoFormatDescription);
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
    std::lock_guard<std::timed_mutex> lock(sessionLocker);
    if (session) {
      auto old = session;
      session = nullptr;
      // Non-blocking emit of any delayed frame, then tear down; a still-pending context is handed
      // to the graveyard by drain() rather than waited on here.
      VTDecompressionSessionFinishDelayedFrames(old);
      VTDecompressionSessionInvalidate(old);
      CFRelease(old);
    }
  }

  // Resolved contexts of the old session are deleted; a context still pending moves to the
  // graveyard so a bounded late callback can still land safely.
  contextPool.drain();

  // create decompression session
  CFDictionaryRef attrs = NULL;
  // Ask CoreVideo to make the decoded CVPixelBuffer usable directly by whichever GPU backend
  // libpag was built against. Wrong compatibility flag causes tgfx to fall back to a CPU copy
  // and hurts decode → render performance.
#if defined(TGFX_USE_METAL)
  const void* keys[] = {kCVPixelBufferPixelFormatTypeKey, kCVPixelBufferMetalCompatibilityKey,
                        kCVPixelBufferIOSurfacePropertiesKey};
#else
  const void* keys[] = {kCVPixelBufferPixelFormatTypeKey, kCVPixelBufferOpenGLESCompatibilityKey,
                        kCVPixelBufferIOSurfacePropertiesKey};
#endif

  uint32_t pixelFormatType = tgfx::IsLimitedYUVColorRange(sourceColorSpace)
                                 ? kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                                 : kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;

  CFNumberRef pixelFormatTypeValue = CFNumberCreate(NULL, kCFNumberSInt32Type, &pixelFormatType);
  CFDictionaryRef ioSurfaceParam =
      CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks);

  // The compatibility keys expect a CFBoolean, not a CFNumber: passing a CFNumber makes
  // CVPixelBufferCreateResolvedAttributesDictionary reject the whole attribute set on newer
  // systems (the same issue main fixed for macOS in PR #3745). kCFBooleanTrue is a shared
  // constant and must not be released.
  const void* values[] = {pixelFormatTypeValue, kCFBooleanTrue, ioSurfaceParam};

  attrs = CFDictionaryCreate(NULL, keys, values, 3, &kCFTypeDictionaryKeyCallBacks,
                             &kCFTypeDictionaryValueCallBacks);

  VTDecompressionOutputCallbackRecord callBackRecord;
  callBackRecord.decompressionOutputCallback = DidDecompress;
  callBackRecord.decompressionOutputRefCon = NULL;

  VTDecompressionSessionRef newSession = nullptr;
  OSStatus status;
  status = VTDecompressionSessionCreate(kCFAllocatorDefault, videoFormatDescription, NULL, attrs,
                                        &callBackRecord, &newSession);

  CFRelease(attrs);
  CFRelease(pixelFormatTypeValue);
  CFRelease(ioSurfaceParam);

  if (newSession != nullptr && (sourceColorSpace == tgfx::YUVColorSpace::BT2020_LIMITED ||
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

  if (status != noErr || !newSession) {
    LOGE("HardwareDecoder:decoder session create failed status = %d", status);
    return false;
  }

  {
    std::lock_guard<std::timed_mutex> lock(sessionLocker);
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

  // Fast-fail while the app is inactive (between willResignActive and didBecomeActive).
  // Calling VTDecompressionSessionDecodeFrame in this window may block indefinitely because
  // iOS suspends the hardware decoder pipeline as the app heads to the background; returning
  // Error here lets the caller release device.locker so the main thread can complete
  // EAGLDevice::finish() within the watchdog window.
  if (appInactive.load(std::memory_order_acquire)) {
    return DecodingResult::Error;
  }

  CVPixelBufferRef outputPixelBuffer = nullptr;

  if (sampleBuffer) {
    OSStatus status = 0;
    VTDecodeInfoFlags infoFlags = 0;
    auto* context = contextPool.create();

    {
      // Held so invalidateSession() can tell whether a decode is still running before tearing the
      // session down.
      std::lock_guard<std::timed_mutex> lock(decodeLocker);
      status = VTDecompressionSessionDecodeFrame(session, sampleBuffer, 0, context, &infoFlags);
    }

    outputPixelBuffer = context->takeBuffer();
    if (status != noErr) {
      // VideoToolbox will not call back for a rejected frame, so the context needs no callback to
      // become resolved and is reclaimed below instead of lingering in the graveyard.
      context->abandon();
    }
    contextPool.reclaimResolved();

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
