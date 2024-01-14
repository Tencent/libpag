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

#include "SoftwareDecoderWrapper.h"
#include "platform/Platform.h"
#include "tgfx/core/YUVData.h"

namespace pag {
class SoftwareI420Data : public tgfx::YUVData {
 public:
  static std::shared_ptr<YUVData> Make(int width, int height, uint8_t* buffer[3],
                                       const int lineSize[3],
                                       std::shared_ptr<SoftwareDecoder> softwareDecoder) {
    auto data = new SoftwareI420Data(width, height, buffer, lineSize, std::move(softwareDecoder));
    return std::shared_ptr<YUVData>(data);
  }

 private:
  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  size_t planeCount() const override {
    return data.size();
  }

  const void* getBaseAddressAt(size_t planeIndex) const override {
    return data[planeIndex];
  }

  size_t getRowBytesAt(size_t planeIndex) const override {
    return rowBytes[planeIndex];
  }

 private:
  int _width = 0;
  int _height = 0;
  std::vector<const void*> data = {};
  std::vector<size_t> rowBytes = {};
  // hold a reference to the software decoder to keep the yuv data alive.
  std::shared_ptr<SoftwareDecoder> softwareDecoder = nullptr;

  SoftwareI420Data(int width, int height, uint8_t* buffer[3], const int lineSize[3],
                   std::shared_ptr<SoftwareDecoder> softwareDecoder)
      : _width(width), _height(height), softwareDecoder(std::move(softwareDecoder)) {
    auto planeCount = static_cast<int>(tgfx::YUVData::I420_PLANE_COUNT);
    data.reserve(planeCount);
    rowBytes.reserve(planeCount);
    for (int i = 0; i < planeCount; i++) {
      data.push_back(buffer[i]);
      rowBytes.push_back(lineSize[i]);
    }
  }
};

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
  auto yuvData = SoftwareI420Data::Make(videoFormat.width, videoFormat.height, frame->data,
                                        frame->lineSize, softwareDecoder);
  return tgfx::ImageBuffer::MakeI420(std::move(yuvData), videoFormat.colorSpace);
}

int64_t SoftwareDecoderWrapper::presentationTime() {
  return currentDecodedTime;
}
}  // namespace pag
