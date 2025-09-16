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

#include "SequenceReader.h"
#include "pag/file.h"
#include "rendering/Performance.h"
#include "tgfx/core/Bitmap.h"

namespace pag {
class BitmapSequenceReader : public SequenceReader {
 public:
  BitmapSequenceReader(std::shared_ptr<File> file, BitmapSequence* sequence);

  int width() const override {
    return sequence->width;
  }

  int height() const override {
    return sequence->height;
  }

  ~BitmapSequenceReader() override;

 protected:
  std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(Frame targetFrame) override;

  void onReportPerformance(Performance* performance, int64_t decodingTime) override;

  Frame findStartFrame(Frame targetFrame);

  std::mutex locker = {};
  // Keep a reference to the File in case the Sequence object is released while we are using it.
  std::shared_ptr<File> file = nullptr;
  BitmapSequence* sequence = nullptr;
  Frame lastDecodeFrame = -1;
  std::shared_ptr<tgfx::ImageBuffer> imageBuffer = nullptr;
  tgfx::ImageInfo info = {};
  std::shared_ptr<tgfx::Data> pixels = nullptr;
  HardwareBufferRef hardWareBuffer = nullptr;
};
}  // namespace pag
