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
#include "rendering/sequences/SequenceReaderFactory.h"

namespace pag {
class VideoSequenceReader : public SequenceReader {
 public:
  VideoSequenceReader(std::shared_ptr<File> file, VideoSequence* sequence);

  ~VideoSequenceReader() override;

  void prepare(Frame targetFrame) override;

 protected:
  bool decodeFrame(Frame) override {
    // NOP
    return true;
  }

  std::shared_ptr<tgfx::Texture> makeTexture(tgfx::Context* context) override;

  void recordPerformance(Performance* performance, int64_t decodingTime) override;

  void prepareNext(Frame) override {
  }

 private:
  // Keep a reference to the File in case the Sequence object is released while we are using it.
  std::shared_ptr<File> file = nullptr;
  emscripten::val videoReader = emscripten::val::null();
  std::shared_ptr<tgfx::Texture> texture = nullptr;
  int32_t width = 0;
  int32_t height = 0;
};
}  // namespace pag
