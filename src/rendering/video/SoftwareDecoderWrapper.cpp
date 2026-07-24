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

#include "SoftwareDecoderWrapper.h"
#include "platform/Platform.h"
#include "rendering/video/SoftwareData.h"

#if defined(__ANDROID__) || defined(ANDROID)
#include <atomic>
#include "base/utils/Log.h"
#include "libyuv/convert_argb.h"
#include "tgfx/platform/HardwareBuffer.h"
#endif

namespace pag {

#define I420_PLANE_COUNT 3

#if defined(__ANDROID__) || defined(ANDROID)
// libyuv writes ABGR into memory as R,G,B,A byte order, matching Android's
// AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM used by tgfx::HardwareBufferAllocate. Returns 0 on
// success, -1 when the color space has no matching libyuv routine so the caller can fall back to
// the plain YUV upload path.
static int ConvertI420ToRGBA(tgfx::YUVColorSpace colorSpace, const uint8_t* srcY, int strideY,
                             const uint8_t* srcU, int strideU, const uint8_t* srcV, int strideV,
                             uint8_t* dst, int dstStride, int width, int height) {
  switch (colorSpace) {
    case tgfx::YUVColorSpace::BT601_LIMITED:
      return libyuv::I420ToABGR(srcY, strideY, srcU, strideU, srcV, strideV, dst, dstStride, width,
                                height);
    case tgfx::YUVColorSpace::BT601_FULL:
    case tgfx::YUVColorSpace::JPEG_FULL:
      return libyuv::J420ToABGR(srcY, strideY, srcU, strideU, srcV, strideV, dst, dstStride, width,
                                height);
    case tgfx::YUVColorSpace::BT709_LIMITED:
      return libyuv::H420ToABGR(srcY, strideY, srcU, strideU, srcV, strideV, dst, dstStride, width,
                                height);
    case tgfx::YUVColorSpace::BT2020_LIMITED:
      return libyuv::U420ToABGR(srcY, strideY, srcU, strideU, srcV, strideV, dst, dstStride, width,
                                height);
    default:
      // BT709_FULL / BT2020_FULL have no matching libyuv routine; the caller falls back to the
      // plain YUV upload path.
      return -1;
  }
}

// Converts a software-decoded I420 frame into a RGBA HardwareBuffer-backed ImageBuffer. Returns
// nullptr if the HardwareBuffer path is unavailable, allocation fails, or the frame's color space
// has no matching libyuv routine, in which case the caller falls back to the plain YUV upload
// path.
static std::shared_ptr<tgfx::ImageBuffer> MakeHardwareBufferFrame(const YUVBuffer* frame,
                                                                  const VideoFormat& videoFormat) {
  if (!tgfx::HardwareBufferAvailable()) {
    return nullptr;
  }
  for (int i = 0; i < I420_PLANE_COUNT; i++) {
    if (frame->data[i] == nullptr || frame->lineSize[i] <= 0) {
      return nullptr;
    }
  }
  auto hardwareBuffer = tgfx::HardwareBufferAllocate(videoFormat.width, videoFormat.height, false);
  if (hardwareBuffer == nullptr) {
    return nullptr;
  }
  auto info = tgfx::HardwareBufferGetInfo(hardwareBuffer);
  auto* dst = static_cast<uint8_t*>(tgfx::HardwareBufferLock(hardwareBuffer));
  if (dst == nullptr) {
    tgfx::HardwareBufferRelease(hardwareBuffer);
    return nullptr;
  }
  int ret =
      ConvertI420ToRGBA(videoFormat.colorSpace, frame->data[0], frame->lineSize[0], frame->data[1],
                        frame->lineSize[1], frame->data[2], frame->lineSize[2], dst,
                        static_cast<int>(info.rowBytes), videoFormat.width, videoFormat.height);
  tgfx::HardwareBufferUnlock(hardwareBuffer);
  if (ret != 0) {
    tgfx::HardwareBufferRelease(hardwareBuffer);
    return nullptr;
  }
  auto imageBuffer = tgfx::ImageBuffer::MakeFrom(hardwareBuffer);
  tgfx::HardwareBufferRelease(hardwareBuffer);
  return imageBuffer;
}
#endif

std::unique_ptr<VideoDecoder> SoftwareDecoderWrapper::Wrap(
    std::shared_ptr<SoftwareDecoder> softwareDecoder, const VideoFormat& format) {
  if (softwareDecoder == nullptr) {
    return nullptr;
  }
  auto decoder = new SoftwareDecoderWrapper(std::move(softwareDecoder));
  if (!decoder->onConfigure(format)) {
    delete decoder;
    return nullptr;
  }
  return std::unique_ptr<VideoDecoder>(decoder);
}

SoftwareDecoderWrapper::SoftwareDecoderWrapper(std::shared_ptr<SoftwareDecoder> externalDecoder)
    : softwareDecoder(std::move(externalDecoder)) {
}

SoftwareDecoderWrapper::~SoftwareDecoderWrapper() {
  delete frameBuffer;
}

bool SoftwareDecoderWrapper::onConfigure(const VideoFormat& format) {
  videoFormat = format;
  if (Platform::Current()->naluType() != NALUType::AnnexB) {
    // External decoders only support AnnexB format.
    videoFormat.headers.clear();
    for (auto& header : format.headers) {
      if (header->size() <= 4) {
        return false;
      }
      tgfx::Buffer buffer(header->data(), header->size());
      buffer[0] = 0;
      buffer[1] = 0;
      buffer[2] = 0;
      buffer[3] = 1;
      videoFormat.headers.push_back(buffer.release());
    }
  }
  std::vector<HeaderData> codecHeaders = {};
  for (auto& header : videoFormat.headers) {
    HeaderData newHeader = {};
    newHeader.data = const_cast<uint8_t*>(header->bytes());
    newHeader.length = header->size();
    codecHeaders.push_back(newHeader);
  }
  return softwareDecoder->onConfigure(codecHeaders, videoFormat.mimeType, videoFormat.width,
                                      videoFormat.height);
}

DecodingResult SoftwareDecoderWrapper::onSendBytes(void* bytes, size_t length, int64_t time) {
  DecodingResult result = DecodingResult::Error;
  if (softwareDecoder != nullptr) {
    // External decoders only support AnnexB format.
    if (bytes != nullptr && length > 0 && Platform::Current()->naluType() != NALUType::AnnexB) {
      if (frameBuffer != nullptr && frameBuffer->size() < length) {
        delete frameBuffer;
        frameBuffer = nullptr;
      }
      uint32_t pos = 0;
      if (frameBuffer == nullptr) {
        frameBuffer = new tgfx::Buffer(bytes, length);
      } else {
        frameBuffer->writeRange(0, length, bytes);
      }
      while (pos < length) {
        (*frameBuffer)[pos] = 0;
        (*frameBuffer)[pos + 1] = 0;
        (*frameBuffer)[pos + 2] = 0;
        (*frameBuffer)[pos + 3] = 1;
        uint32_t naluLength = (static_cast<uint8_t*>(bytes)[pos] << 24) +
                              (static_cast<uint8_t*>(bytes)[pos + 1] << 16) +
                              (static_cast<uint8_t*>(bytes)[pos + 2] << 8) +
                              static_cast<uint8_t*>(bytes)[pos + 3];
        pos += 4 + naluLength;
      }

      result = static_cast<DecodingResult>(
          softwareDecoder->onSendBytes(frameBuffer->data(), length, time));
    } else {
      result = static_cast<DecodingResult>(softwareDecoder->onSendBytes(bytes, length, time));
    }

    if (result != DecodingResult::Error) {
      pendingFrames.push_back(time);
    }
  }
  return result;
}

DecodingResult SoftwareDecoderWrapper::onDecodeFrame() {
  DecodingResult result = DecodingResult::Error;
  if (softwareDecoder != nullptr) {
    result = static_cast<DecodingResult>(softwareDecoder->onDecodeFrame());
    currentDecodedTime = -1;
    if (result == DecodingResult::Success) {
      if (!pendingFrames.empty()) {
        pendingFrames.sort();
        currentDecodedTime = pendingFrames.front();
        pendingFrames.pop_front();
      }
    }
  }
  return result;
}

void SoftwareDecoderWrapper::onFlush() {
  softwareDecoder->onFlush();
  pendingFrames.clear();
  currentDecodedTime = -1;
}

DecodingResult SoftwareDecoderWrapper::onEndOfStream() {
  DecodingResult result = DecodingResult::Error;
  if (softwareDecoder != nullptr) {
    result = static_cast<DecodingResult>(softwareDecoder->onEndOfStream());
  }
  return result;
}

std::shared_ptr<tgfx::ImageBuffer> SoftwareDecoderWrapper::onRenderFrame() {
  auto frame = softwareDecoder->onRenderFrame();
  if (frame == nullptr) {
    return nullptr;
  }
#if defined(__ANDROID__) || defined(ANDROID)
  // Route software-decoded frames through a RGBA HardwareBuffer bound via
  // EGL_ANDROID_image_native_buffer instead of uploading three YUV planes with glTexSubImage2D.
  // This avoids the cross-view frame corruption observed on Mali GPUs when multiple PAGImageView
  // instances upload software-decoded frames concurrently. See issue #3540.
  if (auto imageBuffer = MakeHardwareBufferFrame(frame.get(), videoFormat)) {
    return imageBuffer;
  }
  // Report the first fallback so unexpected regressions on the HardwareBuffer path are visible in
  // logs; subsequent fallbacks stay silent to avoid log spam.
  static std::atomic_flag fallbackLogged = ATOMIC_FLAG_INIT;
  if (!fallbackLogged.test_and_set(std::memory_order_relaxed)) {
    LOGI(
        "SoftwareDecoderWrapper: HardwareBuffer path unavailable, falling back to YUV upload. "
        "size=%dx%d colorSpace=%d",
        videoFormat.width, videoFormat.height, static_cast<int>(videoFormat.colorSpace));
  }
#endif
  auto yuvData =
      SoftwareData<SoftwareDecoder>::Make(videoFormat.width, videoFormat.height, frame->data,
                                          frame->lineSize, I420_PLANE_COUNT, softwareDecoder);
  return tgfx::ImageBuffer::MakeI420(std::move(yuvData), videoFormat.colorSpace);
}

int64_t SoftwareDecoderWrapper::presentationTime() {
  return currentDecodedTime;
}
}  // namespace pag
