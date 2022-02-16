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
#include "I420Buffer.h"
#include "platform/Platform.h"

namespace pag {
class SoftwareI420Buffer : public I420Buffer {
 public:
  static std::shared_ptr<SoftwareI420Buffer> Make(
      int width, int height, uint8_t* data[3], const int lineSize[3],
      tgfx::YUVColorSpace colorSpace, tgfx::YUVColorRange colorRange,
      std::shared_ptr<SoftwareDecoder> softwareDecoder) {
    auto buffer = new SoftwareI420Buffer(width, height, data, lineSize, colorSpace, colorRange,
                                         std::move(softwareDecoder));
    return std::shared_ptr<SoftwareI420Buffer>(buffer);
  }

 private:
  // hold a reference to software decoder in case it is released before us.
  std::shared_ptr<SoftwareDecoder> softwareDecoder = nullptr;

  SoftwareI420Buffer(int width, int height, uint8_t* data[3], const int lineSize[3],
                     tgfx::YUVColorSpace colorSpace, tgfx::YUVColorRange colorRange,
                     std::shared_ptr<SoftwareDecoder> softwareDecoder)
      : I420Buffer(width, height, data, lineSize, colorSpace, colorRange),
        softwareDecoder(std::move(softwareDecoder)) {
  }
};

std::unique_ptr<VideoDecoder> SoftwareDecoderWrapper::Wrap(
    std::shared_ptr<SoftwareDecoder> softwareDecoder, const VideoConfig& config) {
  if (softwareDecoder == nullptr) {
    return nullptr;
  }
  auto decoder = new SoftwareDecoderWrapper(std::move(softwareDecoder));
  if (!decoder->onConfigure(config)) {
    delete decoder;
    return nullptr;
  }
  return std::unique_ptr<VideoDecoder>(decoder);
}

SoftwareDecoderWrapper::SoftwareDecoderWrapper(std::shared_ptr<SoftwareDecoder> externalDecoder)
    : softwareDecoder(std::move(externalDecoder)) {
}

SoftwareDecoderWrapper::~SoftwareDecoderWrapper() {
  delete frameBytes;
  pendingFrames.clear();
}

bool SoftwareDecoderWrapper::onConfigure(const VideoConfig& config) {
  videoConfig = config;
  if (Platform::Current()->naluType() != NALUType::AnnexB) {
    // External decoders only support AnnexB format.
    videoConfig.headers.clear();
    for (auto& header : config.headers) {
      if (header->length() <= 4) {
        return false;
      }
      auto newHeader = ByteData::Make(header->length());
      auto bytes = newHeader->data();
      bytes[0] = 0;
      bytes[1] = 0;
      bytes[2] = 0;
      bytes[3] = 1;
      memcpy(bytes + 4, header->data() + 4, header->length() - 4);
      videoConfig.headers.push_back(std::move(newHeader));
    }
  }
  std::vector<HeaderData> codecHeaders = {};
  for (auto& header : videoConfig.headers) {
    HeaderData newHeader = {};
    newHeader.data = header->data();
    newHeader.length = header->length();
    codecHeaders.push_back(newHeader);
  }
  return softwareDecoder->onConfigure(codecHeaders, videoConfig.mimeType, videoConfig.width,
                                      videoConfig.height);
}

DecodingResult SoftwareDecoderWrapper::onSendBytes(void* bytes, size_t length, int64_t time) {
  DecodingResult result = DecodingResult::Error;
  if (softwareDecoder != nullptr) {
    // External decoders only support AnnexB format.
    if (bytes != nullptr && length > 0 && Platform::Current()->naluType() != NALUType::AnnexB) {
      if (frameBytes != nullptr && frameLength < length) {
        delete frameBytes;
        frameBytes = nullptr;
      }
      uint32_t pos = 0;
      if (frameBytes == nullptr) {
        frameBytes = new uint8_t[length];
        frameLength = length;
      }
      while (pos < length) {
        frameBytes[pos] = 0;
        frameBytes[pos + 1] = 0;
        frameBytes[pos + 2] = 0;
        frameBytes[pos + 3] = 1;
        uint32_t NALULength = (static_cast<uint8_t*>(bytes)[pos] << 24) +
                              (static_cast<uint8_t*>(bytes)[pos + 1] << 16) +
                              (static_cast<uint8_t*>(bytes)[pos + 2] << 8) +
                              static_cast<uint8_t*>(bytes)[pos + 3];
        pos += 4;
        memcpy(frameBytes + pos, static_cast<uint8_t*>(bytes) + pos, NALULength);
        pos += NALULength;
      }

      result = static_cast<DecodingResult>(softwareDecoder->onSendBytes(frameBytes, length, time));
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

std::shared_ptr<VideoBuffer> SoftwareDecoderWrapper::onRenderFrame() {
  auto frame = softwareDecoder->onRenderFrame();
  if (frame == nullptr) {
    return nullptr;
  }
  return SoftwareI420Buffer::Make(videoConfig.width, videoConfig.height, frame->data,
                                  frame->lineSize, videoConfig.colorSpace, videoConfig.colorRange,
                                  softwareDecoder);
}

int64_t SoftwareDecoderWrapper::presentationTime() {
  return currentDecodedTime;
}
}  // namespace pag
