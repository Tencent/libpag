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
  VideoSequenceReader(std::shared_ptr<File> file, VideoSequence* sequence, DecodingPolicy);

  ~VideoSequenceReader() override;

  bool staticContent() const override {
    return sequence->composition->staticContent();
  }

 protected:
  int64_t getNextFrameAt(int64_t targetFrame) override;

  bool decodeFrame(int64_t targetFrame) override;

  std::shared_ptr<tgfx::Texture> makeTexture(tgfx::Context* context) override;

  void reportPerformance(Performance* performance, int64_t decodingTime) const override;

 private:
  // Keep a reference to the File in case the Sequence object is released while we are using it.
  std::shared_ptr<File> file = nullptr;
  VideoSequence* sequence = nullptr;
  emscripten::val videoReader = emscripten::val::null();
  std::shared_ptr<tgfx::Texture> texture = nullptr;
  int32_t width = 0;
  int32_t height = 0;
};
}  // namespace pag
