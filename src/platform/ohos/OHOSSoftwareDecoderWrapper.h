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

#pragma once

#include "platform/ohos/OHOSVideoDecoder.h"
#include "rendering/video/VideoDecoder.h"

namespace pag {
class OHOSSoftwareDecoderWrapper : public VideoDecoder {
 public:
  static std::unique_ptr<VideoDecoder> Wrap(std::shared_ptr<OHOSVideoDecoder> ohosVideoDecoder);

  ~OHOSSoftwareDecoderWrapper() override;

  DecodingResult onSendBytes(void* bytes, size_t length, int64_t time) override;

  DecodingResult onEndOfStream() override;

  DecodingResult onDecodeFrame() override;

  void onFlush() override;

  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override;

  int64_t presentationTime() override;

 private:
  std::shared_ptr<OHOSVideoDecoder> ohosVideoDecoder = nullptr;
  explicit OHOSSoftwareDecoderWrapper(std::shared_ptr<OHOSVideoDecoder> ohosVideoDecoder);
};
}  // namespace pag
