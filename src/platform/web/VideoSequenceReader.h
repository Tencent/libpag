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

#include <emscripten/val.h>
#include <pag/gpu.h>
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/sequences/SequenceReader.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace pag {
class WebVideoTexture : public tgfx::GLTexture {
 public:
  static std::shared_ptr<WebVideoTexture> Make(tgfx::Context* context, int width, int height,
                                               bool isAndroidMiniprogram);

  WebVideoTexture(const tgfx::GLSampler& sampler, int width, int height, tgfx::ImageOrigin origin,
                  bool isMiniprogramAndroid);

  tgfx::Point getTextureCoord(float x, float y) const override;

  size_t memoryUsage() const override;

 private:
  void onReleaseGPU() override;

  bool isAndroidMiniprogram = false;
  int androidAlignment = 16;
};

class VideoSequenceReader : public SequenceReader {
 public:
  VideoSequenceReader(std::shared_ptr<File> file, VideoSequence* sequence,
                      PAGFile* pagFile = nullptr);

  ~VideoSequenceReader() override;

  void prepare(Frame targetFrame) override;

 protected:
  bool decodeFrame(Frame) override {
    // NOP
    return true;
  }

  std::shared_ptr<tgfx::Texture> onMakeTexture(tgfx::Context* context) override;

  void prepareNext(Frame) override {
  }

 private:
  // Keep a reference to the File in case the Sequence object is released while we are using it.
  std::shared_ptr<File> file = nullptr;
  PAGFile* rootFile = nullptr;
  emscripten::val videoReader = emscripten::val::null();
  std::shared_ptr<WebVideoTexture> webVideoTexture = nullptr;
  int32_t width = 0;
  int32_t height = 0;
  std::unique_ptr<ByteData> mp4Data = nullptr;
};
}  // namespace pag
