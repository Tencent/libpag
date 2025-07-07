/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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
#include "rendering/sequences/VideoReader.h"
#include "tgfx/core/Bitmap.h"

namespace pag {
class DiskSequenceReader : public SequenceReader {
 public:
  static std::shared_ptr<DiskSequenceReader> Make(std::shared_ptr<File> file, Sequence* sequence);

  int width() const override;

  int height() const override;

  ~DiskSequenceReader() override;

 private:
  DiskSequenceReader(std::shared_ptr<File> file, Sequence* sequence);
  Sequence* sequence = nullptr;
  std::shared_ptr<PAGDecoder> pagDecoder;
  std::shared_ptr<File> file;
  std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(Frame targetFrame) override;
  void onReportPerformance(Performance* performance, int64_t decodingTime) override;
  std::shared_ptr<tgfx::ImageBuffer> imageBuffer = nullptr;
  std::mutex locker = {};
  tgfx::ImageInfo info = {};
  std::shared_ptr<tgfx::Data> pixels = nullptr;
  bool useFrontBuffer = true;
  HardwareBufferRef backHardwareBuffer = nullptr;
  HardwareBufferRef frontHardWareBuffer = nullptr;
};
}  // namespace pag
