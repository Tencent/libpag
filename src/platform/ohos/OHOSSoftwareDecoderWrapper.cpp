/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

#include "OHOSSoftwareDecoderWrapper.h"
#include "rendering/video/SoftwareDecoderWrapper.h"

namespace pag {
std::unique_ptr<VideoDecoder> OHOSSoftwareDecoderWrapper::Wrap(
    std::shared_ptr<OHOSVideoDecoder> ohosVideoDecoder) {
  if (ohosVideoDecoder == nullptr) {
    return nullptr;
  }
  auto decoder = new OHOSSoftwareDecoderWrapper(ohosVideoDecoder);
  return std::unique_ptr<VideoDecoder>(decoder);
}

OHOSSoftwareDecoderWrapper::OHOSSoftwareDecoderWrapper(
    std::shared_ptr<OHOSVideoDecoder> ohosVideoDecoder)
    : ohosVideoDecoder(std::move(ohosVideoDecoder)) {
}

OHOSSoftwareDecoderWrapper::~OHOSSoftwareDecoderWrapper() {
}

DecodingResult OHOSSoftwareDecoderWrapper::onSendBytes(void* bytes, size_t length, int64_t time) {
  return ohosVideoDecoder->onSendBytes(bytes, length, time);
}

DecodingResult OHOSSoftwareDecoderWrapper::onDecodeFrame() {
  return ohosVideoDecoder->onDecodeFrame();
}

DecodingResult OHOSSoftwareDecoderWrapper::onEndOfStream() {
  return ohosVideoDecoder->onEndOfStream();
}

void OHOSSoftwareDecoderWrapper::onFlush() {
  ohosVideoDecoder->onFlush();
}

std::shared_ptr<tgfx::ImageBuffer> OHOSSoftwareDecoderWrapper::onRenderFrame() {
  return ohosVideoDecoder->onRenderFrame();
}

int64_t OHOSSoftwareDecoderWrapper::presentationTime() {
  return ohosVideoDecoder->presentationTime();
}

}  // namespace pag
