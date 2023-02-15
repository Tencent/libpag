/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "tgfx/gpu/opengl/GLTexture.h"

namespace pag {
class WebVideoTexture : public tgfx::GLTexture {
 public:
  static std::shared_ptr<WebVideoTexture> Make(tgfx::Context* context, int width, int height,
                                               bool isAndroidMiniprogram);

  WebVideoTexture(const tgfx::GLSampler& sampler, int width, int height, tgfx::ImageOrigin origin);

  tgfx::Point getTextureCoord(float x, float y) const override;

  size_t memoryUsage() const override;

 private:
  int textureWidth = 0;
  int textureHeight = 0;

  void onReleaseGPU() override;
};

class WebVideoBuffer : public tgfx::ImageBuffer {
 public:
  WebVideoBuffer(int width, int height, emscripten::val videoReader);

  ~WebVideoBuffer() override;

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  bool isAlphaOnly() const override {
    return false;
  }

 protected:
  std::shared_ptr<tgfx::Texture> onMakeTexture(tgfx::Context* context, bool) const override;

 private:
  mutable std::mutex locker = {};
  mutable std::shared_ptr<WebVideoTexture> webVideoTexture = nullptr;
  int _width = 0;
  int _height = 0;
  emscripten::val videoReader = emscripten::val::null();

  friend class HardwareDecoder;
};

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
  std::shared_ptr<WebVideoBuffer> videoBuffer = nullptr;
  int32_t _width = 0;
  int32_t _height = 0;
  float frameRate = 30.0f;
  std::unique_ptr<ByteData> mp4Data = nullptr;

  explicit HardwareDecoder(const VideoFormat& format);

  friend class WebVideoBuffer;
  friend class HardwareDecoderFactory;
};
}  // namespace pag
