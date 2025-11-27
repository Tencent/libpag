/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#pragma once

#include <emscripten/val.h>
#include <mutex>
#include "pag/pag.h"
#include "rendering/video/VideoDecoderFactory.h"
#include "tgfx/platform/web/VideoElementReader.h"

namespace pag {
class HardwareDecoder : public VideoDecoder {
 public:
  DecodingResult onSendBytes(void* bytes, size_t length, int64_t time) override;

  DecodingResult onEndOfStream() override;

  DecodingResult onDecodeFrame() override;

  void onFlush() override;

  int64_t presentationTime() override;

  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override;

 private:
  int64_t pendingTimeStamp = 0;
  int64_t currentTimeStamp = 0;
  bool endOfStream = true;
  // Keep a reference to the File in case the Sequence object is released while we are using it.
  std::shared_ptr<File> file = nullptr;
  PAGFile* rootFile = nullptr;
  emscripten::val videoReader = emscripten::val::null();
  std::shared_ptr<tgfx::VideoElementReader> imageReader = nullptr;
  int32_t _width = 0;
  int32_t _height = 0;
  float frameRate = 30.0f;
  int currentFrame = -1;
  std::shared_ptr<tgfx::ImageBuffer> lastDecodedBuffer = nullptr;

  explicit HardwareDecoder(const VideoFormat& format);

  friend class HardwareDecoderFactory;
};
}  // namespace pag
