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

#ifdef PAG_USE_LIBAVC

#include "base/utils/Log.h"
#include "pag/decoder.h"
#include "pag/types.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/Data.h"

extern "C" {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "ih264_typedefs.h"
#include "ih264d.h"
#include "iv.h"
#include "ivd.h"
#pragma clang diagnostic pop
}

namespace pag {
/**
 * SoftAVCDecoder supports the annex-b format only.
 */
class SoftAVCDecoder : public SoftwareDecoder {
 public:
  ~SoftAVCDecoder() override;

  bool onConfigure(const std::vector<HeaderData>& headers, std::string mime, int width,
                   int height) override;

  DecoderResult onSendBytes(void* bytes, size_t length, int64_t frame) override;

  DecoderResult onDecodeFrame() override;

  DecoderResult onEndOfStream() override;

  void onFlush() override;

  std::unique_ptr<YUVBuffer> onRenderFrame() override;

 private:
  std::shared_ptr<tgfx::Data> headerData = nullptr;
  tgfx::Buffer* outputFrame = nullptr;
  iv_obj_t* codecContext = nullptr;  // Codec context
  ivd_video_decode_ip_t decodeInput = {};
  ivd_video_decode_op_t decodeOutput = {};
  bool flushed = true;

  bool initDecoder();
  bool openDecoder();
  bool setParams(bool decodeHeader);
  bool setNumCores();
  bool initOutputFrame();
  void destroyDecoder();
  void resetDecoder();
};
}  // namespace pag

#endif
