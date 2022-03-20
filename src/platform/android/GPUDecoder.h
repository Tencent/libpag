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

#pragma once

#include "VideoImage.h"
#include "VideoSurface.h"
#include "platform/android/Global.h"
#include "rendering/video/VideoDecoder.h"

namespace pag {
class GPUDecoder : public VideoDecoder {
 public:
  static void InitJNI(JNIEnv* env, const std::string& className);

  explicit GPUDecoder(const VideoFormat& format);

  ~GPUDecoder() override;

  bool isValid() {
    return _isValid;
  }

  DecodingResult onSendBytes(void* bytes, size_t length, int64_t time) override;

  DecodingResult onEndOfStream() override;

  DecodingResult onDecodeFrame() override;

  void onFlush() override;

  int64_t presentationTime() override;

  std::shared_ptr<VideoBuffer> onRenderFrame() override;

 private:
  bool _isValid = false;
  int videoWidth = 0;
  int videoHeight = 0;
  std::shared_ptr<VideoSurface> videoSurface = nullptr;
  Global<jobject> videoDecoder;

  bool onConfigure(jobject decoder, const VideoFormat& format);
};
}  // namespace pag
