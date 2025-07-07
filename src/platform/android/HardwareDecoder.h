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

#pragma once

#include "JNIHelper.h"
#include "android/native_window.h"
#include "android/native_window_jni.h"
#include "media/NdkMediaCodec.h"
#include "media/NdkMediaFormat.h"
#include "platform/android/JVideoSurface.h"
#include "rendering/video/VideoDecoder.h"
#include "tgfx/platform/android/SurfaceTextureReader.h"

namespace pag {
class HardwareDecoder : public VideoDecoder {
 public:
  ~HardwareDecoder() override;

  DecodingResult onSendBytes(void* bytes, size_t length, int64_t time) override;

  DecodingResult onEndOfStream() override;

  DecodingResult onDecodeFrame() override;

  void onFlush() override;

  int64_t presentationTime() override;

  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override;

  bool releaseOutputBuffer(bool render);

 private:
  bool isValid = false;

  // AMediaCodec_start(原decoder.start)
  // AMediaCodec_flush(原decoder.flush)
  // HUAWEI Mate 40 Pro，在连续或者相近的时间执行上面代码会解码失败，
  // 报 `VIDEO-[pps_sps_check_tmp_id]:[5994]pps is null ppsid = 0 havn't decode`
  bool disableFlush = true;

  std::shared_ptr<tgfx::SurfaceTextureReader> imageReader = nullptr;
  AMediaCodec* videoDecoder = nullptr;
  ANativeWindow* nativeWindow = nullptr;
  ssize_t lastOutputBufferIndex = -1;
  AMediaCodecBufferInfo* bufferInfo = nullptr;

  explicit HardwareDecoder(const VideoFormat& format);
  bool initDecoder(const VideoFormat& format);

  friend class HardwareDecoderFactory;
};
}  // namespace pag
