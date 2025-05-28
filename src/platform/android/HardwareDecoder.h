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

#include "JNIHelper.h"
#include "platform/android/JVideoSurface.h"
#include "rendering/video/VideoDecoder.h"
#include "tgfx/platform/android/SurfaceTextureReader.h"
#include "media/NdkMediaCodec.h"
#include "media/NdkMediaFormat.h"
#include "android/native_window.h"
#include "android/native_window_jni.h"

namespace pag {
class HardwareDecoder : public VideoDecoder {
 public:
  static void InitJNI(JNIEnv* env);

  ~HardwareDecoder() override;

  DecodingResult onSendBytes(void* bytes, size_t length, int64_t time) override;

  DecodingResult onEndOfStream() override;

  DecodingResult onDecodeFrame() override;

  void onFlush() override;

  int64_t presentationTime() override;

  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override;

private:
    bool isValid = false;
    bool disableFlush = true;
    std::shared_ptr<tgfx::SurfaceTextureReader> imageReader = nullptr;
    AMediaCodec* videoDecoder = nullptr;
    static const int TIMEOUT_US = 1000;
    ssize_t lastOutputBufferIndex = -1;
    AMediaCodecBufferInfo* bufferInfo = new AMediaCodecBufferInfo();

  explicit HardwareDecoder(const VideoFormat& format);
  bool initDecoder(const VideoFormat& format);

  friend class HardwareDecoderFactory;
};
}  // namespace pag
